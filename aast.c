#define _GNU_SOURCE // Enable POSIX extensions like strdup, strsep, and getline

#include "aast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/evp.h>

// ----------------------------------------------------------------------------
// Internal Constants and Definitions
// ----------------------------------------------------------------------------

#define AAST_MAX_DEPTH 35000 // Empirically mapped to protect 8MB x86_64 Linux stack limits.

// --- Structs for Ingestion Engine ---
typedef struct TempNode {
    char* key;
    char* type;
    char* payload;
    struct TempNode* parent;
    struct TempNode** children;
    size_t child_count;
    size_t child_capacity;
} TempNode;

// --- Structs for Persistence API ---
typedef struct VisitedNode {
    char hash[65];
    UT_hash_handle hh;
} VisitedNode;

typedef struct NodeMapEntry {
    char hash[65];
    Node* node;
    UT_hash_handle hh;
} NodeMapEntry;

// ----------------------------------------------------------------------------
// Static Forward Declarations for Internal Helper Functions
// ----------------------------------------------------------------------------

// Primitives
static void compute_sha256_hex(const char* data, char outputBuffer[65]);
static char* generate_canonical_buffer(const Node* node);
static int child_sort_by_key(ChildEntry* a, ChildEntry* b); // Moved up

// Recursive Helpers for Public API
static void aast_release_recursive(Node* node, int current_depth);
static int aast_verify_integrity_recursive(const Node* root, int current_depth);
static Node* accrete_recursive_helper(const Node* current_node, const char* const* path, size_t path_len, const char* new_payload); // Corrected signature

// Ingestion Helpers
static Node* convert_temp_to_aast(TempNode* t_node);

// Debugging Helpers
#ifdef DEBUG_PRINT
static void aast_print_tree_recursive(const char* key, const Node* node, int indent_level); // Corrected signature
#endif
// ----------------------------------------------------------------------------
// Cryptographic & Serialization Primitives (Internal)
// ----------------------------------------------------------------------------

static void compute_sha256_hex(const char* data, char outputBuffer[65]) {
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    const EVP_MD *md = EVP_sha256();
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int md_len;
    EVP_DigestInit_ex(mdctx, md, NULL);
    EVP_DigestUpdate(mdctx, data, strlen(data));
    EVP_DigestFinal_ex(mdctx, hash, &md_len);
    EVP_MD_CTX_free(mdctx);
    for(unsigned int i = 0; i < md_len; i++) {
        sprintf(outputBuffer + (i * 2), "%02x", hash[i]);
    }
    outputBuffer[64] = '\0';
}

static char* generate_canonical_buffer(const Node* node) {
    if (node == NULL) return NULL;
    size_t type_len = strlen(node->type);
    size_t payload_len = (node->payload) ? strlen(node->payload) : 0;
    char* hashes_str = NULL;

    if (node->child_count > 0) {
        size_t hashes_str_len = (node->child_count * 64) + (node->child_count - 1);
        hashes_str = (char*)malloc(hashes_str_len + 1);
        if (hashes_str == NULL) return NULL;

        char* p = hashes_str;
        ChildEntry *child_entry, *tmp;
        size_t i = 0;
        // Iterate through the HASH TABLE of children to get their hashes.
        // This relies on the table being sorted before this function is called.
        HASH_ITER(hh, node->children, child_entry, tmp) {
            memcpy(p, child_entry->child_node->hash, 64);
            p += 64;
            if (i < node->child_count - 1) { *p = ','; p++; }
            i++;
        }
        *p = '\0';
    } else {
        hashes_str = "";
    }

    int required_size = snprintf(NULL, 0, "%zu:%s|%zu:%s|%zu:%s",
        type_len, node->type,
        node->child_count, hashes_str,
        payload_len, node->payload ? node->payload : "");

    if (required_size < 0) {
        if (node->child_count > 0) free(hashes_str);
        return NULL;
    }

    char* buffer = (char*)malloc(required_size + 1);
    if (buffer == NULL) {
        if (node->child_count > 0) free(hashes_str);
        return NULL;
    }

    sprintf(buffer, "%zu:%s|%zu:%s|%zu:%s",
        type_len, node->type,
        node->child_count, hashes_str,
        payload_len, node->payload ? node->payload : "");

    if (node->child_count > 0) free(hashes_str);
    return buffer;
}

// ----------------------------------------------------------------------------
// Public API Implementations
// ----------------------------------------------------------------------------
Node* create_node(const char* type, const char* payload, const AastChildInput* children_input, size_t child_count) {
    
    // --- START OF OPERATIONAL CONSTRAINT VALIDATION ---
    // 1. Enforce semantic type constraint
    if (type && strlen(type) >= AAST_MAX_TYPE_LEN) {
        fprintf(stderr, "[A-AST Error] Type string '%s' exceeds maximum limit of %d bytes.\n", type, AAST_MAX_TYPE_LEN - 1);
        return NULL;
    }

    // 2. Enforce key length constraint across all child inputs
    for (size_t i = 0; i < child_count; i++) {
        if (children_input && children_input[i].key) {
            if (strlen(children_input[i].key) > AAST_MAX_KEY_LEN) {
                fprintf(stderr, "[A-AST Error] Child key at index %zu exceeds maximum limit of %d bytes.\n", i, AAST_MAX_KEY_LEN);
                return NULL;
            }
        }
    }

    // 3. Enforce maximum payload chunking limit
    if (payload && strlen(payload) > AAST_MAX_PAYLOAD_SIZE) {
        fprintf(stderr, "[A-AST Error] Payload size (%zu bytes) exceeds maximum chunk threshold of %d bytes.\n", strlen(payload), AAST_MAX_PAYLOAD_SIZE);
        return NULL;
    }
    // --- END OF OPERATIONAL CONSTRAINT VALIDATION ---
    Node* new_node = (Node*)malloc(sizeof(Node));
    if (new_node == NULL) return NULL;
    // Initialize all members for safe cleanup on partial failure
    new_node->payload = NULL;
    new_node->children = NULL; // IMPORTANT: uthash head must be NULL
    new_node->ref_count = 1;
    new_node->child_count = 0; // Will be incremented as we add children
    
    strncpy(new_node->type, type, 15);
    new_node->type[15] = '\0';
   
    if (payload != NULL) {
        new_node->payload = strdup(payload);
        if (new_node->payload == NULL) { free(new_node); return NULL; }
    }
    
    // --- Build the children hash table ---
    for (size_t i = 0; i < child_count; i++) {
        ChildEntry* new_entry = (ChildEntry*)malloc(sizeof(ChildEntry));
        if (!new_entry) goto cleanup_fail; // Jump to error handling

        new_entry->key = strdup(children_input[i].key);
        if (!new_entry->key) { free(new_entry); goto cleanup_fail; }
        
        new_entry->child_node = children_input[i].child;
        
        // The new parent node becomes an owner of this child
        aast_retain(new_entry->child_node);
        
        HASH_ADD_KEYPTR(hh, new_node->children, new_entry->key, strlen(new_entry->key), new_entry);
        new_node->child_count++;
    }

    // --- Sort children by key for deterministic hashing ---
    if (new_node->child_count > 0) {
        HASH_SORT(new_node->children, child_sort_by_key);
    }

    // --- Generate final hash ---
    char* canonical_buffer = generate_canonical_buffer(new_node);
    if (canonical_buffer == NULL) goto cleanup_fail;
    
    compute_sha256_hex(canonical_buffer, new_node->hash);
    free(canonical_buffer);

    return new_node;

cleanup_fail:
    // Comprehensive cleanup on any allocation failure during child processing
    if (new_node) {
        ChildEntry *current_entry, *tmp;
        HASH_ITER(hh, new_node->children, current_entry, tmp) {
            HASH_DEL(new_node->children, current_entry);
            aast_release(current_entry->child_node); // Release the retain we did
            free(current_entry->key);
            free(current_entry);
        }
        free(new_node->payload);
        free(new_node);
    }
    return NULL;
}

void aast_release(Node* node) {
    aast_release_recursive(node, 0);
}

int aast_retain(Node* node) {
    if (node == NULL) return 0;
    if (node->ref_count == SIZE_MAX) return -1;
   node->ref_count++;
    return 0;
}

const Node* aast_find_child_by_key(const Node* parent, const char* key) {
    if (!parent || !key) return NULL;
    
    ChildEntry* found_entry = NULL;
    HASH_FIND_STR(parent->children, key, found_entry);
    
   if (found_entry) {
        return found_entry->child_node;
    }
    return NULL;
}

const Node* aast_query_path(const Node* root, const char* const* path, size_t path_len) {
    if (!root || !path) return NULL;
    if (path_len == 0) return root; // An empty path theoretically points to the root itself

    const Node* current = root;
    for (size_t i = 0; i < path_len; i++) {
        current = aast_find_child_by_key(current, path[i]);
        if (!current) {
            return NULL; // Path broken, fail gracefully
        }
    }
    return current;
}

void aast_iterate_children(const Node* parent, AastChildCallback callback, void* context) {
    if (!parent || !callback) return;

    ChildEntry *current_child, *tmp;
    // Iterate through the O(1) hash table natively
    HASH_ITER(hh, parent->children, current_child, tmp) {
        // Pass the key, the weak node pointer, and the user's context state
        callback(current_child->key, current_child->child_node, context);
    }
}
int aast_validate_utf8_nfc(const Node* validator_root, const char* text) {
    if (!validator_root || !text) return 0;
    
    const unsigned char* p = (const unsigned char*)text;
    while (*p != '\0') {
        int char_len = 1;
        // 1. Determine UTF-8 byte length (Basic UTF-8 structural validation)
        if ((*p & 0x80) == 0x00) char_len = 1;
        else if ((*p & 0xE0) == 0xC0) char_len = 2;
        else if ((*p & 0xF0) == 0xE0) char_len = 3;
        else if ((*p & 0xF8) == 0xF0) char_len = 4;
        else return 0; // Invalid UTF-8 byte structure
        
        const Node* current = validator_root;
        int found_in_trie = 1;
        
        // 2. Walk the A-AST Trie byte-by-byte for this character
        for (int i = 0; i < char_len; i++) {
            if (*(p + i) == '\0') return 0; // Unexpected EOF
            
            char hex_key[3];
            sprintf(hex_key, "%02X", *(p + i));
            
            current = aast_find_child_by_key(current, hex_key);
            if (!current) {
                found_in_trie = 0;
                break; // Path broken -> Not in exception list -> VALID NFC
            }
        }
        
        // 3. If we completed the path, check the rule payload
        if (found_in_trie && current && current->payload) {
            if (strcmp(current->payload, "N") == 0 || strcmp(current->payload, "M") == 0) {
                return 0; // Strictly rejected by UCD rulebook
            }
        }
        
       p += char_len; // Advance to the next character
    }
    return 1;
}

int aast_execute_in_link_context(const Node* link_node, void (*callback)(const Node* loaded_root, void* context), void* context) {
    // 1. Validate inputs and ensure this is actually a link node
    if (!link_node || !callback || strcmp(link_node->type, AAST_TYPE_LINK) != 0 || !link_node->payload) {
        return 0;
    }

    // 2. Construct the filename from the payload hash (e.g., "83c9...e109.aast")
    size_t hash_len = strlen(link_node->payload);
    char* filename = malloc(hash_len + 6); // +5 for ".aast" +1 for '\0'
    if (!filename) return 0;
    sprintf(filename, "%s.aast", link_node->payload);

    // 3. The Isolated Load (Quarantine Zone)
    Node* loaded_sub_tree = aast_deserialize_from_file(filename);
    free(filename);

    if (!loaded_sub_tree) {
        return 0; // File missing or structurally corrupted
    }

    // 4. The Iron Gate Verification
    // (aast_deserialize_from_file already checks the header, but we MUST check 
    // that the loaded root matches the SPECIFIC hash requested by the parent link).
    if (strcmp(loaded_sub_tree->hash, link_node->payload) != 0) {
        fprintf(stderr, "[A-AST Error] Iron Gate Failure: AAST_LINK hash mismatch.\n");
        aast_release(loaded_sub_tree);
        return 0;
    }

    // 5. Context-Managed Execution
    // We pass a const (weak) pointer. The Agent cannot free it or mutate it.
    callback(loaded_sub_tree, context);

    // 6. Guaranteed Memory Flush
    aast_release(loaded_sub_tree);
    
    return 1;
}

Node* accrete_new_state(const Node* root, const char* const* path, size_t path_len, const char* new_payload) {
    if (!root || !path || path_len == 0) {
        return NULL;
    }
    // This public function simply acts as a safe entry point to the recursive helper.
    return accrete_recursive_helper(root, path, path_len, new_payload);
}

int aast_verify_integrity(const Node* root) {
    return aast_verify_integrity_recursive(root, 0);
}

// ----------------------------------------------------------------------------
// Internal Helper Implementations
// ----------------------------------------------------------------------------

static void aast_release_recursive(Node* node, int current_depth) {
    if (!node) return;
    if (current_depth >= AAST_MAX_DEPTH) {
        fprintf(stderr, "WARNING: Max recursion depth reached...\n");
        return;
    }
    node->ref_count--;
    if (node->ref_count == 0) {
        ChildEntry *current_child, *tmp;
        // Step 1: Recursively release all child nodes
        HASH_ITER(hh, node->children, current_child, tmp) {
            aast_release_recursive(current_child->child_node, current_depth + 1);
        }
        // Step 2: Free the hash table itself
        HASH_ITER(hh, node->children, current_child, tmp) {
            HASH_DEL(node->children, current_child);
            free(current_child->key);
            free(current_child);
        }
        // Step 3: Free the node's own data
        free(node->payload);
        free(node);
    }
}

static int aast_verify_integrity_recursive(const Node* root, int current_depth) {
    if (!root) return 1;
    if (current_depth >= AAST_MAX_DEPTH) {
        fprintf(stderr, "ERROR: Max recursion depth reached...\n");
        return 0;
    }
    ChildEntry *child_entry, *tmp;
    HASH_ITER(hh, root->children, child_entry, tmp) {
        if (aast_verify_integrity_recursive(child_entry->child_node, current_depth + 1) == 0) {
            return 0;
        }
    }
    // Sort children before buffer generation for determinism
    Node* mutable_node = (Node*)root;
    HASH_SORT(mutable_node->children, child_sort_by_key);
    
    char* canonical_buffer = generate_canonical_buffer(root);
    if (!canonical_buffer) return 0;
    char fresh_hash[65];
    compute_sha256_hex(canonical_buffer, fresh_hash);
    free(canonical_buffer);
    return (strcmp(root->hash, fresh_hash) == 0);
}

static Node* accrete_recursive_helper(const Node* current_node, const char* const* path, size_t path_len, const char* new_payload) {
    if (!current_node) return NULL;

    // Base Case: If the path is empty, we are at the target node. Recreate it with the new payload.
    if (path_len == 0) {
        // To recreate this node, we must gather its children's info into an AastChildInput array.
        AastChildInput* children_inputs = NULL;
        if (current_node->child_count > 0) {
            children_inputs = malloc(current_node->child_count * sizeof(AastChildInput));
            if (!children_inputs) return NULL;
            
            ChildEntry* child_entry, *tmp;
            size_t i = 0;
            HASH_ITER(hh, current_node->children, child_entry, tmp) {
                children_inputs[i].key = child_entry->key;
                children_inputs[i].child = child_entry->child_node;
                i++;
            }
        }
        
        Node* new_node = create_node(current_node->type, new_payload, children_inputs, current_node->child_count);
        free(children_inputs);
        return new_node;
    }

    // Recursive Step: We need to go deeper.
    const char* next_key = path[0];
    ChildEntry* child_to_follow = NULL;
    HASH_FIND_STR(current_node->children, next_key, child_to_follow);

    if (!child_to_follow) {
        return NULL; // Invalid path, child not found.
    }

    // Recurse down the path. This will return a pointer to a *new* child node.
    Node* new_child_node = accrete_recursive_helper(child_to_follow->child_node, &path[1], path_len - 1, new_payload);
    
    if (!new_child_node) {
        return NULL; // Lower-level recursion failed.
    }

    // --- Accretion: Rebuild the current node with the new child ---
    AastChildInput* parent_children_inputs = malloc(current_node->child_count * sizeof(AastChildInput));
    if (!parent_children_inputs) {
        aast_release(new_child_node);
        return NULL;
    }

    ChildEntry* sibling_entry, *tmp;
    size_t i = 0;
    HASH_ITER(hh, current_node->children, sibling_entry, tmp) {
        if (strcmp(sibling_entry->key, next_key) == 0) {
            parent_children_inputs[i].key = next_key;
            parent_children_inputs[i].child = new_child_node;
        } else {
            parent_children_inputs[i].key = sibling_entry->key;
            parent_children_inputs[i].child = sibling_entry->child_node;
        }
        i++;
    }

    Node* new_parent_node = create_node(current_node->type, current_node->payload, parent_children_inputs, current_node->child_count);

    free(parent_children_inputs);
    // Release our temporary ownership of new_child_node, as it's now owned by new_parent_node.
    aast_release(new_child_node);

    return new_parent_node;
}

/**
 * @brief Comparison function used by uthash to sort ChildEntry structs by key.
 */
static int child_sort_by_key(ChildEntry* a, ChildEntry* b) {
    return strcmp(a->key, b->key);
}

// --- Ingestion Engine ---
static int add_child_to_temp_node(TempNode* parent, TempNode* child) {
    if (parent->child_count >= parent->child_capacity) {
        size_t new_capacity = (parent->child_capacity == 0) ? 8 : parent->child_capacity * 2;
        void* new_children = realloc(parent->children, new_capacity * sizeof(TempNode*));
        if (new_children == NULL) return -1;
        parent->children = new_children;
        parent->child_capacity = new_capacity;
    }
    parent->children[parent->child_count++] = child;
    child->parent = parent;
    return 0;
}

static void free_temp_tree(TempNode* t_node) {
    if (t_node == NULL) return;
    for (size_t i = 0; i < t_node->child_count; i++) free_temp_tree(t_node->children[i]);
    free(t_node->key); free(t_node->type); free(t_node->payload); free(t_node->children); free(t_node);
}

static Node* convert_temp_to_aast(TempNode* t_node) {
    if (t_node == NULL) return NULL;

    AastChildInput* children_inputs = NULL;
    Node* new_node = NULL;

    // --- Step 1: Recursively convert all children first (Post-Order Traversal) ---
    if (t_node->child_count > 0) {
        children_inputs = malloc(t_node->child_count * sizeof(AastChildInput));
        if (children_inputs == NULL) return NULL;

        for (size_t i = 0; i < t_node->child_count; i++) {
            TempNode* temp_child = t_node->children[i];
            
            // Set the key for the child input from the TempNode
            children_inputs[i].key = temp_child->key;
            
            // Recursively convert the child TempNode to a final Node*
            children_inputs[i].child = convert_temp_to_aast(temp_child);
            
            if (children_inputs[i].child == NULL) {
                // Cleanup on failure
                for (size_t j = 0; j < i; j++) {
                    aast_release(children_inputs[j].child);
                }
                free(children_inputs);
                return NULL;
            }
        }
    }

    // --- Step 2: Create the current node using the converted children ---
    // The node itself no longer has a 'key' parameter in create_node
    new_node = create_node(t_node->type, t_node->payload, children_inputs, t_node->child_count);
    
    // --- Step 3: Cleanup ---
    if (children_inputs != NULL) {
        // Release the temporary ownership of the children
        for (size_t i = 0; i < t_node->child_count; i++) {
            aast_release(children_inputs[i].child);
        }
        free(children_inputs);
    }
    
    return new_node;
}

Node* aast_ingest_from_text(const char* text_data, const Node* nfc_validator) {
    if (!text_data) return NULL;

    char* text_copy = strdup(text_data);
    if (!text_copy) return NULL;
    char* to_free = text_copy;

    TempNode* temp_root = calloc(1, sizeof(TempNode));
    if (!temp_root) { free(to_free); return NULL; }
    TempNode* current_parent = temp_root;
    int prev_indent = -2;

    char* p = text_copy;
    while (p && *p != '\0') {
        // 1. Count indentation and handle empty lines
        int current_indent = 0;
        while (*p == ' ') { current_indent++; p++; }
        if (*p == '\n') { p++; continue; }
        if (*p == '\0') break;

        if (current_indent % 2 != 0 || current_indent > prev_indent + 2) goto error_cleanup;

        while (current_indent <= prev_indent) {
            current_parent = current_parent->parent;
            prev_indent -= 2;
        }

        // 2. Extract Key
        char* key = p;
        char* colon1 = strchr(p, ':');
        if (!colon1) goto error_cleanup;
        *colon1 = '\0';

        // 3. Extract Type
        char* type = colon1 + 1;
        char* colon2 = strchr(type, ':');
        if (!colon2) goto error_cleanup;
        *colon2 = '\0';

    // 4. Extract Payload (The State Machine)
        char* payload = colon2 + 1;
        
        // Check for the 3-byte Start Marker: C0 C1 FF
        if (strncmp(payload, "\xC0\xC1\xFF", 3) == 0) {
            // --- OPAQUE PAYLOAD MODE ---
            payload += 3; // Skip opening marker
            
            // Search for the 3-byte End Marker: FF C1 C0
            char* end_marker = strstr(payload, "\xFF\xC1\xC0");
            if (!end_marker) goto error_cleanup; // Missing closing marker
            
            *end_marker = '\0'; // Strip closing marker
            
            p = end_marker + 3; // Move pointer past the end marker
            // Advance parser to the next line
            while (*p != '\n' && *p != '\0') p++;
            if (*p == '\n') p++;
        } else {
            // --- STANDARD SINGLE-LINE MODE ---
            char* newline = strchr(payload, '\n');
            if (newline) {
                *newline = '\0';
                p = newline + 1;
            } else {
                p = payload + strlen(payload); // End of file
            }
        }
        // 5. NFC Hygiene Boundary (Validate the stripped components)
        if (nfc_validator != NULL) {
            if (!aast_validate_utf8_nfc(nfc_validator, key) ||
                !aast_validate_utf8_nfc(nfc_validator, type) ||
                (payload[0] != '\0' && !aast_validate_utf8_nfc(nfc_validator, payload))) {
                fprintf(stderr, "[A-AST Error] Ingestion failed: Text violates strict UTF-8 NFC encoding contract.\n");
                goto error_cleanup;
            }
        }

        // 6. Build the Temp Node
        TempNode* new_temp_node = calloc(1, sizeof(TempNode));
        if (!new_temp_node) goto error_cleanup;
        
        new_temp_node->key = strdup(key);
        new_temp_node->type = strdup(type);
        if (payload[0] != '\0') new_temp_node->payload = strdup(payload);
        
        if (!new_temp_node->key || !new_temp_node->type || (payload[0] != '\0' && !new_temp_node->payload)) {
            free_temp_tree(new_temp_node);
            goto error_cleanup;
        }

        add_child_to_temp_node(current_parent, new_temp_node);
        current_parent = new_temp_node;
        prev_indent = current_indent;
    }

    Node* final_root = NULL;
    if (temp_root->child_count == 1) {
        final_root = convert_temp_to_aast(temp_root->children[0]);
    }
    
free_temp_tree(temp_root);
    free(to_free);
    return final_root;

error_cleanup:
    fprintf(stderr, "[A-AST Parser Debug] Parser aborted! Failed near character: '%c' (Hex: %02X)\n", *p ? *p : 'E', *p);
    free_temp_tree(temp_root);
    free(to_free);
    return NULL;
}

Node* aast_ingest_opaque_node(const char* type, const char* wrapped_payload, const Node* nfc_validator) {
    if (!type || !wrapped_payload) return NULL;

    size_t wrapped_len = strlen(wrapped_payload);
    if (wrapped_len < 6) return NULL; // Must be at least large enough to hold both 3-byte markers

    // 1. Verify directional transport markers
    if (strncmp(wrapped_payload, "\xC0\xC1\xFF", 3) != 0 || 
        strncmp(wrapped_payload + wrapped_len - 3, "\xFF\xC1\xC0", 3) != 0) {
        fprintf(stderr, "[A-AST Error] Opaque ingestion failed: Missing or invalid transport markers.\n");
        return NULL;
    }

    // 2. Allocate a secure C-owned heap buffer to prevent mutating Python/Host memory
    char* pure_payload = strdup(wrapped_payload);
    if (!pure_payload) return NULL;

    // 3. Isolate the pure payload by adjusting pointers and null-terminating
    char* payload_start = pure_payload + 3; // Skip opening marker
    char* payload_end = pure_payload + wrapped_len - 3; // Find closing marker
    *payload_end = '\0'; // Strip closing marker

    // 4. NFC Hygiene Boundary
    if (nfc_validator != NULL) {
        if (!aast_validate_utf8_nfc(nfc_validator, payload_start)) {
            fprintf(stderr, "[A-AST Error] Opaque ingestion failed: Payload violates UTF-8 NFC encoding contract.\n");
            free(pure_payload);
            return NULL;
        }
    }

    // 5. Instantiate Immutable Node
    Node* new_node = create_node(type, payload_start, NULL, 0);
    
    // Cleanup the temporary secure buffer
    free(pure_payload);
    
    return new_node;
}

// --- Persistence API ---
static int serialize_recursive_helper(const Node* node, FILE* fp, VisitedNode** visited_set) {
    if (!node) return 0;
    VisitedNode* found;
    HASH_FIND_STR(*visited_set, node->hash, found);
    if (found) return 0;
    VisitedNode* new_visited = malloc(sizeof(VisitedNode));
    if (!new_visited) return -1;
    strcpy(new_visited->hash, node->hash);
    HASH_ADD_STR(*visited_set, hash, new_visited);

    ChildEntry *child_entry, *tmp;
    HASH_ITER(hh, node->children, child_entry, tmp) {
        if (serialize_recursive_helper(child_entry->child_node, fp, visited_set) != 0) {
            return -1;
        }
    }
    fprintf(fp, "%s|%s|%zu:%s|",
            node->hash, node->type,
            node->payload ? strlen(node->payload) : 0, node->payload ? node->payload : "");
    Node* mutable_node = (Node*)node;
    HASH_SORT(mutable_node->children, child_sort_by_key);

    size_t i = 0;
    HASH_ITER(hh, node->children, child_entry, tmp) {
        fprintf(fp, "%s:%s%s", child_entry->key, child_entry->child_node->hash,
                (i < node->child_count - 1) ? "," : "");
        i++;
    }
    fprintf(fp, "\n");
    return 0;
}

int aast_serialize_to_file(const Node* root, const char* filename) {
    if (!root || !filename) return -1;
    FILE* fp = fopen(filename, "w");
    if (!fp) { perror("Failed to open file for serialization"); return -1; }
    
    // --- NEW: Write Formal Filetype Header ---
    fprintf(fp, "AAST_V1|%s\n", root->hash);

    VisitedNode* visited_set = NULL;
    int result = serialize_recursive_helper(root, fp, &visited_set);
    fclose(fp);
    VisitedNode *current_node, *tmp;
    HASH_ITER(hh, visited_set, current_node, tmp) {
        HASH_DEL(visited_set, current_node);
        free(current_node);
    }
    if (result != 0) { fprintf(stderr, "Serialization failed.\n"); return -1; }
    return 0;
}

Node* aast_deserialize_from_file(const char* filename) {
    FILE* fp = fopen(filename, "r");
    if (!fp) { perror("Failed to open file for deserialization"); return NULL; }

    NodeMapEntry* node_map = NULL;
    Node* root = NULL;
    char* line = NULL;
    size_t len = 0;
    ssize_t read;
    int error = 0;

    // --- Read and validate the Formal Filetype Header ---
    if ((read = getline(&line, &len, fp)) == -1) { free(line); fclose(fp); return NULL; }
    if (strncmp(line, "AAST_V1|", 8) != 0) {
        fprintf(stderr, "[A-AST Error] Invalid file signature. Not a valid .aast file.\n");
        free(line); fclose(fp); return NULL;
    }
    char expected_root_hash[65];
    strncpy(expected_root_hash, line + 8, 64);
    expected_root_hash[64] = '\0';

    while ((read = getline(&line, &len, fp)) != -1) {
        char* current = line;
        char* token_end;

        // HASH
        token_end = strchr(current, '|');
        if (!token_end) { error = 1; break; }
        char stored_hash[65];
        strncpy(stored_hash, current, token_end - current);
        stored_hash[token_end - current] = '\0';
        current = token_end + 1;

        // TYPE
        token_end = strchr(current, '|');
        if (!token_end) { error = 1; break; }
        char type[17];
        strncpy(type, current, token_end - current);
        type[token_end - current] = '\0';
        current = token_end + 1;
        
        // PAYLOAD (In-Place Null Termination Fix)
        token_end = strchr(current, '|');
        if (!token_end) { error = 1; break; }
        char* payload_len_str = current;
        char* payload_str = strchr(payload_len_str, ':');
        if (!payload_str || payload_str > token_end) { error = 1; break; }
        payload_str++; 
        
        size_t payload_len = token_end - payload_str;
        *token_end = '\0'; // In-place termination, NO stack array allocated!
        current = token_end + 1;

        // CHILDREN
        AastChildInput* children_inputs = NULL;
        size_t child_count = 0;
        char* children_field = current;
        if (strchr(children_field, '\n')) *strchr(children_field, '\n') = '\0'; 

        if (strlen(children_field) > 0) {
            char* children_copy = strdup(children_field);
            char* p_child = children_copy;
            char* child_token;
            while((child_token = strsep(&p_child, ",")) != NULL) {
                char* key = strsep(&child_token, ":");
                char* hash = child_token;
                if (!key || !hash) { error = 1; break; }

                NodeMapEntry* found_entry;
                HASH_FIND_STR(node_map, hash, found_entry);
                if (!found_entry) { error = 1; break; }
                
                void* temp = realloc(children_inputs, (child_count + 1) * sizeof(AastChildInput));
                if (!temp) { error = 1; break; }
                children_inputs = temp;
                
                children_inputs[child_count].key = strdup(key);
                children_inputs[child_count].child = found_entry->node;
                child_count++;
            }
            free(children_copy);
        }
        if (error) {
             for(size_t i = 0; i < child_count; i++) free((void*)children_inputs[i].key);
             free(children_inputs);
             break;
        }

        // CREATE NODE
        Node* new_node = create_node(type, payload_len > 0 ? payload_str : NULL, children_inputs, child_count);

        for(size_t i = 0; i < child_count; i++) free((void*)children_inputs[i].key);
        free(children_inputs);
        
        if (!new_node) { error = 1; break; }
        if (strcmp(new_node->hash, stored_hash) != 0) {
            aast_release(new_node); error = 1; break;
        }
        
        // MAP ADDITION
        NodeMapEntry* new_entry = malloc(sizeof(NodeMapEntry));
        if(!new_entry) { aast_release(new_node); error = 1; break; }
        strcpy(new_entry->hash, new_node->hash);
        new_entry->node = new_node;
        HASH_ADD_STR(node_map, hash, new_entry);
        
        root = new_node;
    }
    
    // --- Final Cleanup & Header Hash Enforcement ---
    if (!error && root) {
        if (strcmp(root->hash, expected_root_hash) != 0) {
            fprintf(stderr, "[A-AST Error] File corrupted: Expected root %s, but got %s\n", expected_root_hash, root->hash);
            error = 1;
        }
    }

    if (error) {
        aast_release(root); 
        root = NULL;
    }

    NodeMapEntry *current_entry, *tmp;
    if (root) {
        HASH_FIND_STR(node_map, root->hash, current_entry);
        if (current_entry) {
            HASH_DEL(node_map, current_entry);
            free(current_entry);
        }
    }
    
    HASH_ITER(hh, node_map, current_entry, tmp) {
        aast_release(current_entry->node);
        HASH_DEL(node_map, current_entry);
        free(current_entry);
    }
    
    fclose(fp);
    if(line) free(line);

    return root;
}

#ifdef DEBUG_PRINT
static void aast_print_tree_recursive(const char* key, const Node* node, int indent_level) {
    if (node == NULL) return;

    for (int i = 0; i < indent_level; ++i) printf("  ");

    // Print the key passed from the parent, and the node's own data
    printf("- Key: %-20s | Type: %-10s | Payload: %-25.25s | Hash: %.8s... | Refs: %zu\n",
           key, node->type,
           node->payload ? node->payload : "NULL", node->hash, node->ref_count);

    // Correctly iterate through the hash table of children
    ChildEntry *child_entry, *tmp;
    HASH_ITER(hh, node->children, child_entry, tmp) {
        // Pass the child's key and the child's node pointer to the recursive call
        aast_print_tree_recursive(child_entry->key, child_entry->child_node, indent_level + 1);
    }
}

void aast_print_tree(const Node* root) {
    if (root == NULL) { printf("A-AST is NULL.\n"); return; }
    printf("--- A-AST Tree View ---\n");
    // The root node has no key from a parent's perspective, so we pass a placeholder
    aast_print_tree_recursive("(root)", root, 0);
    printf("-----------------------\n");
}
#endif // DEBUG_PRINT
