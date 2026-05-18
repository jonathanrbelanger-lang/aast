#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/evp.h>

// 1. The Semantic Node Structure
// Upgraded to include top-down semantic pathing (keys) and string-based types.
typedef struct Node {
    char type[16];           // e.g., "ROOT", "HEADER", "TEXT"
    char *key;               // Semantic structural identifier (e.g., "document_root")
    char *payload;           // The actual data value
    struct Node **children;  // Array of pointers to child nodes
    size_t child_count;   
    size_t ref_count;   
    char hash[65];           // 64-char SHA-256 hex string + null terminator
} Node;

// 2. Cryptographic Anchor (Phase 3)
// Generates a deterministic SHA-256 hex string using OpenSSL's EVP API.
void compute_sha256_hex(const char* data, char outputBuffer[65]) {
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

// 3. Canonical Serialization (Phase 3.5 Hardened)
// Packs node data into a strict, length-prefixed C-string to guarantee hash parity
// and prevent parsing ambiguity or payload collision attacks.
// Grammar: [TYPE_LEN]:[TYPE]|[KEY_LEN]:[KEY]|[CHILD_COUNT]:[HASH1,HASH2...]|[PAYLOAD_LEN]:[PAYLOAD]
char* generate_canonical_buffer(Node* node) {
    if (node == NULL) {
        return NULL;
    }

    // Step 1: Pre-calculate the lengths of dynamic components.
    size_t type_len = strlen(node->type);
    size_t key_len = (node->key) ? strlen(node->key) : 0;
    size_t payload_len = (node->payload) ? strlen(node->payload) : 0;

    // Step 2: Generate the concatenated child hash string. This is the most complex part.
    char* hashes_str = NULL;
    size_t hashes_str_len = 0;
    if (node->child_count > 0) {
        // Calculate exact size: N hashes of 64 chars + (N-1) commas
        hashes_str_len = (node->child_count * 64) + (node->child_count - 1);
        hashes_str = (char*)malloc(hashes_str_len + 1);
        if (hashes_str == NULL) {
            return NULL; // Graceful Failure on allocation error.
        }

        // Use a running pointer for efficient string construction (avoids O(n^2) strcat)
        char* p = hashes_str;
        for (size_t i = 0; i < node->child_count; i++) {
            memcpy(p, node->children[i]->hash, 64);
            p += 64;
            if (i < node->child_count - 1) {
                *p = ',';
                p++;
            }
        }
        *p = '\0';
    } else {
        // If there are no children, use a static empty string to avoid malloc/free.
        hashes_str = "";
    }

    // Step 3: Calculate the exact size needed for the final canonical buffer.
    // The snprintf(NULL, 0, ...) pattern is a safe way to pre-calculate this.
    int required_size = snprintf(NULL, 0, "%zu:%s|%zu:%s|%zu:%s|%zu:%s",
        type_len, node->type,
        key_len, node->key ? node->key : "",
        node->child_count, hashes_str,
        payload_len, node->payload ? node->payload : ""
    );

    if (required_size < 0) {
        // Handle potential snprintf encoding error.
        if (node->child_count > 0) free(hashes_str);
        return NULL;
    }

    // Step 4: Allocate the final buffer and construct the string.
    char* buffer = (char*)malloc(required_size + 1);
    if (buffer == NULL) {
        // Graceful Failure: ensure the temporary hashes_str is freed.
        if (node->child_count > 0) free(hashes_str);
        return NULL;
    }

    sprintf(buffer, "%zu:%s|%zu:%s|%zu:%s|%zu:%s",
        type_len, node->type,
        key_len, node->key ? node->key : "",
        node->child_count, hashes_str,
        payload_len, node->payload ? node->payload : ""
    );

    // Step 5: Clean up the temporary hash string if it was allocated.
    if (node->child_count > 0) {
        free(hashes_str);
    }
    
    return buffer;
}
// 4. The Constructor (Phase 1 + 4 Hardened)
// Allocates memory, enforces immutability via deep copies, and anchors the hash.
// Hardened to gracefully return NULL on any allocation failure.
// Replace the old create_node with this version.
// The only change is initializing new_node->ref_count = 1;
Node* create_node(const char* type, const char* key, const char* payload, Node** children, size_t child_count) {
    Node* new_node = (Node*)malloc(sizeof(Node));
    if (new_node == NULL) {
        return NULL;
    }

    new_node->key = NULL;
    new_node->payload = NULL;
    new_node->children = NULL;
    new_node->ref_count = 1; // <-- ADD THIS LINE: The creator is the first owner.

    strncpy(new_node->type, type, 15);
    new_node->type[15] = '\0';
    new_node->child_count = child_count;

    if (key != NULL) {
        new_node->key = strdup(key);
        if (new_node->key == NULL) {
            free(new_node);
            return NULL;
        }
    }
    
    if (payload != NULL) {
        new_node->payload = strdup(payload);
        if (new_node->payload == NULL) {
            free(new_node->key);
            free(new_node);
            return NULL;
        }
    }

    if (child_count > 0 && children != NULL) {
        new_node->children = (Node**)malloc(child_count * sizeof(Node*));
        if (new_node->children == NULL) {
            free(new_node->key);
            free(new_node->payload);
            free(new_node);
            return NULL;
        }
        memcpy(new_node->children, children, child_count * sizeof(Node*));
    }

    char* canonical_buffer = generate_canonical_buffer(new_node);
    if (canonical_buffer == NULL) {
        free(new_node->children);
        free(new_node->key);
        free(new_node->payload);
        free(new_node);
        return NULL;
    }

    compute_sha256_hex(canonical_buffer, new_node->hash);
    free(canonical_buffer);

    return new_node;
}

// ----------------------------------------------------------------------------
// Phase 5: Reference-Counted Memory Management
// ----------------------------------------------------------------------------

/**
 * @brief Increases the reference count of a node.
 *
 * This function indicates that a new owner is now pointing to this node.
 * It includes a safety check to prevent integer overflow.
 *
 * @param node The node to retain.
 * @return 0 on success, -1 on failure (ref_count would overflow).
 */
int aast_retain(Node* node) {
    if (node == NULL) {
        return 0; // Retaining NULL is a no-op, not an error.
    }
    // Red Team Audit Mitigation: Prevent ref_count overflow.
    if (node->ref_count == SIZE_MAX) {
        fprintf(stderr, "CRITICAL ERROR: Reference count overflow for node with key '%s'. System is in an unrecoverable state.\n", node->key ? node->key : "NULL");
        return -1;
    }
    node->ref_count++;
    return 0;
}

/**
 * @brief Decreases the reference count of a node and frees it if the count reaches zero.
 *
 * API CONTRACT (CRITICAL): This function should only be called on a "root" node
 * that you are completely finished with. Calling this on a non-owning pointer
 * (e.g., a temporary pointer to a child node) will lead to premature
 * deallocation and heap corruption.
 *
 * @param node The node to release.
 */
void aast_release(Node* node) {
    if (node == NULL) {
        return;
    }

    // Decrement the reference count.
    node->ref_count--;

    // If the count drops to zero, the node has no more owners and must be destroyed.
    if (node->ref_count == 0) {
        // First, relinquish ownership of all children. This is the recursive part.
        if (node->child_count > 0 && node->children != NULL) {
            for (size_t i = 0; i < node->child_count; i++) {
                aast_release(node->children[i]);
            }
            free(node->children);
        }

        // Free the node's internal dynamic string data.
        if (node->key != NULL) free(node->key);
        if (node->payload != NULL) free(node->payload);

        // Finally, free the node struct itself.
        free(node);
    }
}

// 6. The Accretion Engine (Phase 4)
// Recursively creates a new version of the tree with an updated payload at a specific path.
// Replace the old accrete_recursive_helper with this ref-count-aware version.
Node* accrete_recursive_helper(Node* current_node, const char* const* path, size_t path_index, size_t path_len, const char* new_payload) {
    if (current_node == NULL || current_node->key == NULL || strcmp(current_node->key, path[path_index]) != 0) {
        return NULL;
    }

    if (path_index == path_len - 1) {
        return create_node(current_node->type, current_node->key, new_payload, current_node->children, current_node->child_count);
    }

    size_t target_child_index = (size_t)-1;
    const char* next_key = path[path_index + 1];
    for (size_t i = 0; i < current_node->child_count; i++) {
        if (current_node->children[i] != NULL && current_node->children[i]->key != NULL &&
            strcmp(current_node->children[i]->key, next_key) == 0) {
            target_child_index = i;
            break;
        }
    }
    if (target_child_index == (size_t)-1) {
        return NULL;
    }

    Node* new_child_node = accrete_recursive_helper(current_node->children[target_child_index], path, path_index + 1, path_len, new_payload);
    if (new_child_node == NULL) {
        return NULL;
    }

    Node** new_children_list = (Node**)malloc(current_node->child_count * sizeof(Node*));
    if (new_children_list == NULL) {
        aast_release(new_child_node); // Use new release function for cleanup
        return NULL;
    }
    memcpy(new_children_list, current_node->children, current_node->child_count * sizeof(Node*));
    new_children_list[target_child_index] = new_child_node;

    // --- REFERENCE COUNTING LOGIC ---
    // The new parent node becomes an additional owner of all its children.
    for (size_t i = 0; i < current_node->child_count; i++) {
        // The newly created child already has its ref_count of 1 for its new parent.
        // We only need to retain the *unmodified, shared* siblings.
        if (i != target_child_index) {
            if (aast_retain(new_children_list[i]) != 0) {
                // RETAIN FAILED! This is a catastrophic overflow. We must unwind.
                // 1. Release the child we just created.
                aast_release(new_child_node);
                // 2. Release all siblings we successfully retained so far.
                for (size_t j = 0; j < i; j++) {
                     if (j != target_child_index) aast_release(new_children_list[j]);
                }
                // 3. Free the temporary list and propagate failure.
                free(new_children_list);
                return NULL;
            }
        }
    }
    // --- END REFERENCE COUNTING LOGIC ---

    Node* new_parent_node = create_node(current_node->type, current_node->key, current_node->payload, new_children_list, current_node->child_count);

    if (new_parent_node == NULL) {
        // Parent creation failed. We must release all children in the temp list
        // to undo the retains and prevent memory leaks.
        for (size_t i = 0; i < current_node->child_count; i++) {
            aast_release(new_children_list[i]);
        }
    }
    
    // The old parent is not an owner of the new parent's children list.
    // However, the *new parent* is. create_node copied the list, but it now
    // owns the children pointers. We must release our temporary list's "ownership".
    // This is subtle: we don't own the nodes, but we created a list that points to them.
    // The new parent is now the owner, so we just free our list.
    free(new_children_list);
    
    return new_parent_node;
}


// Top-level API for the Accretion Engine.
Node* accrete_new_state(Node* root, const char* const* path, size_t path_len, const char* new_payload) {
    if (root == NULL || path == NULL || path_len == 0) {
        return NULL;
    }
    // Begin the recursive accretion from the root of the tree.
    return accrete_recursive_helper(root, path, 0, path_len, new_payload);
}

// 7. Execution Sandbox
int main() {
    // Accretion: Building from the leaves up to the root
    
    Node* text1 = create_node("TEXT", "concept_intro", "This is an A-AST concept.", NULL, 0);
    Node* text2 = create_node("TEXT", "language_spec", "It is built in C.", NULL, 0);

    Node* paragraph_children[] = {text1, text2};
    Node* paragraph = create_node("PARAGRAPH", "intro_paragraph", NULL, paragraph_children, 2);

    Node* header_text = create_node("TEXT", "header_text", "Architectural Overview", NULL, 0);
    Node* header_children[] = {header_text};
    Node* header = create_node("HEADER", "main_header", NULL, header_children, 1);

    Node* root_children[] = {header, paragraph};
    Node* root_v1 = create_node("ROOT", "document_root", NULL, root_children, 2);

    printf("========================================\n");
    printf("A-AST Sandbox Initialized (State v1)\n");
    printf("========================================\n");
    printf("Root v1 Key:  %s\n", root_v1->key);
    printf("Root v1 Hash: %s\n\n", root_v1->hash);

    // --- PHASE 4: ACCRETION DEMONSTRATION ---
    printf("========================================\n");
    printf("Initiating Accretion (State v2)\n");
    printf("========================================\n");

    const char* const path_to_change[] = {"document_root", "intro_paragraph", "concept_intro"};
    size_t path_len = sizeof(path_to_change) / sizeof(path_to_change[0]);
    const char* new_payload_v2 = "This is the NEW, updated payload.";

    // Generate the new state via structural sharing
    Node* root_v2 = accrete_new_state(root_v1, path_to_change, path_len, new_payload_v2);

    if (root_v2) {
        printf("Accretion successful.\n");
        printf("Root v2 Key:  %s\n", root_v2->key);
        printf("Root v2 Hash: %s\n\n", root_v2->hash);

        printf("--- Verification ---\n");
        printf("Original 'text1' payload is still: '%s'\n", text1->payload);
        
        // The 'header' branch was not on the mutation path.
        // Therefore, both v1 and v2 roots should point to the *exact same* memory address for it.
        if (root_v1->children[0] == root_v2->children[0]) {
            printf("PASSED: Unchanged 'header' branch is shared (same memory pointer).\n");
        } else {
            printf("FAILED: Unchanged 'header' branch was wastefully re-allocated.\n");
        }
        
        // The 'paragraph' branch WAS on the mutation path.
        // The pointers must be different.
        if (root_v1->children[1] != root_v2->children[1]) {
            printf("PASSED: Changed 'paragraph' branch is a new allocation (different pointers).\n");
        } else {
            printf("FAILED: Changed 'paragraph' branch was not re-allocated.\n");
        }
    } else {
        printf("Accretion failed. Path may be incorrect or memory allocation failed.\n");
    }
    
    // Replace the main function's cleanup section
    printf("\n--- Cleanup ---\n");
    
    // With reference counting, we simply release the roots we are holding.
    // The shared header node will be correctly freed only when the last root
    // pointing to it (root_v1) is released.
    printf("Releasing root_v2...\n");
    aast_release(root_v2); 
    printf("Releasing root_v1...\n");
    aast_release(root_v1);

    printf("Cleanup complete.\n");
    printf("========================================\n");

    return 0;
}
