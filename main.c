/**
 * @file main.c
 * @brief A-AST (Accretive Abstract State Tree) Core Implementation.
 *
 * This file contains the complete implementation of an experimental, memory-safe,
 * and cryptographically verifiable data structure.
 *
 * Core Principles:
 * 1. Immutability via Accretion: Nodes are never modified after creation.
 *    Changes result in a new tree root via structural sharing.
 * 2. Cryptographic Verifiability: Every node is anchored by a SHA-256 hash
 *    of its contents and its children's hashes, creating a Merkle DAG.
 * 3. Memory Safety: A reference counting system prevents both memory leaks
 *    and double-free corruption in shared-node scenarios.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/evp.h>
#include "uthash.h"

// ----------------------------------------------------------------------------
// Phase 1: Core Data Structure
// ----------------------------------------------------------------------------

typedef struct Node {
    char type[16];           // e.g., "ROOT", "HEADER", "TEXT"
    char *key;               // Owned by the Node. Must be freed on destruction.
    char *payload;           // Owned by the Node. Must be freed on destruction.
    struct Node **children;  // Owned by the Node. The array itself must be freed.
    size_t child_count;      
    size_t ref_count;        // Tracks the number of owners for shared nodes.
    char hash[65];           // 64-char SHA-256 hex string + null terminator
} Node;

// ----------------------------------------------------------------------------
// Phase 3: Cryptographic & Serialization Primitives
// ----------------------------------------------------------------------------

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

char* generate_canonical_buffer(const Node* node) {
    if (node == NULL) return NULL;

    size_t type_len = strlen(node->type);
    size_t key_len = (node->key) ? strlen(node->key) : 0;
    size_t payload_len = (node->payload) ? strlen(node->payload) : 0;
    
    char* hashes_str = NULL;
    if (node->child_count > 0) {
        size_t hashes_str_len = (node->child_count * 64) + (node->child_count - 1);
        hashes_str = (char*)malloc(hashes_str_len + 1);
        if (hashes_str == NULL) return NULL;
        char* p = hashes_str;
        for (size_t i = 0; i < node->child_count; i++) {
            memcpy(p, node->children[i]->hash, 64);
            p += 64;
            if (i < node->child_count - 1) { *p = ','; p++; }
        }
        *p = '\0';
    } else {
        hashes_str = "";
    }

    int required_size = snprintf(NULL, 0, "%zu:%s|%zu:%s|%zu:%s|%zu:%s",
        type_len, node->type, key_len, node->key ? node->key : "",
        node->child_count, hashes_str, payload_len, node->payload ? node->payload : "");

    if (required_size < 0) {
        if (node->child_count > 0) free(hashes_str);
        return NULL;
    }

    char* buffer = (char*)malloc(required_size + 1);
    if (buffer == NULL) {
        if (node->child_count > 0) free(hashes_str);
        return NULL;
    }

    sprintf(buffer, "%zu:%s|%zu:%s|%zu:%s|%zu:%s",
        type_len, node->type, key_len, node->key ? node->key : "",
        node->child_count, hashes_str, payload_len, node->payload ? node->payload : "");

    if (node->child_count > 0) free(hashes_str);
    
    return buffer;
}

// Forward declaration
int aast_verify_integrity(const Node* root);

// ----------------------------------------------------------------------------
// Forward Declarations for API Functions
// ----------------------------------------------------------------------------
int aast_retain(Node* node);
void aast_release(Node* node);
Node* accrete_recursive_helper(const Node* current_node, const char* const* path, size_t path_index, size_t path_len, const char* new_payload);
Node* aast_ingest_from_text(const char* text_data);
// ----------------------------------------------------------------------------
// Phase 4: Core A-AST API
// ----------------------------------------------------------------------------


Node* create_node(const char* type, const char* key, const char* payload, Node** children, size_t child_count) {
    Node* new_node = (Node*)malloc(sizeof(Node));
    if (new_node == NULL) return NULL;

    new_node->key = NULL;
    new_node->payload = NULL;
    new_node->children = NULL;
    new_node->ref_count = 1;

    strncpy(new_node->type, type, 15);
    new_node->type[15] = '\0';
    new_node->child_count = child_count;

    if (key != NULL) {
        new_node->key = strdup(key);
        if (new_node->key == NULL) { free(new_node); return NULL; }
    }
    if (payload != NULL) {
        new_node->payload = strdup(payload);
        if (new_node->payload == NULL) { free(new_node->key); free(new_node); return NULL; }
    }

    if (child_count > 0 && children != NULL) {
        new_node->children = (Node**)malloc(child_count * sizeof(Node*));
        if (new_node->children == NULL) { free(new_node->key); free(new_node->payload); free(new_node); return NULL; }
        memcpy(new_node->children, children, child_count * sizeof(Node*));
    }

    char* canonical_buffer = generate_canonical_buffer(new_node);
    if (canonical_buffer == NULL) {
        free(new_node->children); free(new_node->key); free(new_node->payload); free(new_node);
        return NULL;
    }

    compute_sha256_hex(canonical_buffer, new_node->hash);
    free(canonical_buffer);

    return new_node;
}

Node* accrete_recursive_helper(const Node* current_node, const char* const* path, size_t path_index, size_t path_len, const char* new_payload) {
    if (current_node == NULL || current_node->key == NULL || strcmp(current_node->key, path[path_index]) != 0) return NULL;
    if (path_index == path_len - 1) return create_node(current_node->type, current_node->key, new_payload, current_node->children, current_node->child_count);

    size_t target_child_index = (size_t)-1;
    for (size_t i = 0; i < current_node->child_count; i++) {
        if (current_node->children[i] != NULL && current_node->children[i]->key != NULL && strcmp(current_node->children[i]->key, path[path_index + 1]) == 0) {
            target_child_index = i; break;
        }
    }
    if (target_child_index == (size_t)-1) return NULL;

    Node* new_child_node = accrete_recursive_helper(current_node->children[target_child_index], path, path_index + 1, path_len, new_payload);
    if (new_child_node == NULL) return NULL;

    Node** new_children_list = (Node**)malloc(current_node->child_count * sizeof(Node*));
    if (new_children_list == NULL) { aast_release(new_child_node); return NULL; }
    
    memcpy(new_children_list, current_node->children, current_node->child_count * sizeof(Node*));
    new_children_list[target_child_index] = new_child_node;

    for (size_t i = 0; i < current_node->child_count; i++) {
        if (i != target_child_index) {
            if (aast_retain(new_children_list[i]) != 0) {
                aast_release(new_child_node);
                for (size_t j = 0; j < i; j++) { if (j != target_child_index) aast_release(new_children_list[j]); }
                free(new_children_list); return NULL;
            }
        }
    }

    Node* new_parent_node = create_node(current_node->type, current_node->key, current_node->payload, new_children_list, current_node->child_count);
    if (new_parent_node == NULL) {
        for (size_t i = 0; i < current_node->child_count; i++) aast_release(new_children_list[i]);
    }
    
    free(new_children_list);
    return new_parent_node;
}

Node* accrete_new_state(const Node* root, const char* const* path, size_t path_len, const char* new_payload) {
    if (root == NULL || path == NULL || path_len == 0) return NULL;
    return accrete_recursive_helper(root, path, 0, path_len, new_payload);
}

// ----------------------------------------------------------------------------
// Phase 5: Memory Management API
// ----------------------------------------------------------------------------

int aast_retain(Node* node) {
    if (node == NULL) return 0;
    if (node->ref_count == SIZE_MAX) return -1;
    node->ref_count++;
    return 0;
}

/**
 * @warning This function uses recursion and is vulnerable to a stack overflow
 * if called on a tree with extreme depth. A production-ready version should
 * refactor this to an iterative approach using a heap-allocated stack.
 */
void aast_release(Node* node) {
    if (node == NULL) return;

    node->ref_count--;
    if (node->ref_count == 0) {
        if (node->child_count > 0 && node->children != NULL) {
            for (size_t i = 0; i < node->child_count; i++) aast_release(node->children[i]);
            free(node->children);
        }
        if (node->key != NULL) free(node->key);
        if (node->payload != NULL) free(node->payload);
        free(node);
    }
}

// ----------------------------------------------------------------------------
// Phase 6: Verification API
// ----------------------------------------------------------------------------

int aast_verify_integrity(const Node* root) {
    if (root == NULL) return 1;

    for (size_t i = 0; i < root->child_count; i++) {
        if (aast_verify_integrity(root->children[i]) == 0) return 0;
    }

    char* canonical_buffer = generate_canonical_buffer(root);
    if (canonical_buffer == NULL) return 0;

    char fresh_hash[65];
    compute_sha256_hex(canonical_buffer, fresh_hash);
    free(canonical_buffer);

    if (strcmp(root->hash, fresh_hash) != 0) return 0;

    return 1;
}

// ----------------------------------------------------------------------------
// Phase 6.5: Debugging Utilities (Conditional Compilation)
// ----------------------------------------------------------------------------
#ifdef DEBUG_PRINT

/**
 * @brief (DEBUG ONLY) Prints a human-readable representation of the tree.
 *
 * This is a recursive, pre-order traversal utility for debugging. It is only
 * compiled into the binary if the DEBUG_PRINT macro is defined at compile time
 * (e.g., gcc -DDEBUG_PRINT).
 *
 * @param node The root node of the tree/subtree to print.
 * @param indent_level The current indentation level for formatting.
 */
void aast_print_tree_recursive(const Node* node, int indent_level) {
    if (node == NULL) {
        return;
    }

    // Print indentation
    for (int i = 0; i < indent_level; ++i) {
        printf("  ");
    }

    // Print node details
    printf("- Key: %-20s | Type: %-10s | Payload: %-25.25s | Hash: %.8s... | Refs: %zu\n",
           node->key ? node->key : "NULL",
           node->type,
           node->payload ? node->payload : "NULL",
           node->hash,
           node->ref_count);

    // Recurse for children
    for (size_t i = 0; i < node->child_count; ++i) {
        aast_print_tree_recursive(node->children[i], indent_level + 1);
    }
}

void aast_print_tree(const Node* root) {
    if (root == NULL) {
        printf("A-AST is NULL.\n");
        return;
    }
    printf("--- A-AST Tree View ---\n");
    aast_print_tree_recursive(root, 0);
    printf("-----------------------\n");
}

#endif // DEBUG_PRINT

// ----------------------------------------------------------------------------
// Phase 6.8: Ingestion Engine
// ----------------------------------------------------------------------------

// A temporary, mutable node structure used only during parsing.
typedef struct TempNode {
    char* key;
    char* type;
    char* payload;
    struct TempNode* parent;
    struct TempNode** children;
    size_t child_count;
    size_t child_capacity;
} TempNode;

// Helper to add a child to a TempNode, handling dynamic array resizing.
int add_child_to_temp_node(TempNode* parent, TempNode* child) {
    if (parent->child_count >= parent->child_capacity) {
        size_t new_capacity = (parent->child_capacity == 0) ? 8 : parent->child_capacity * 2;
        Node** new_children = realloc(parent->children, new_capacity * sizeof(TempNode*));
        if (new_children == NULL) return -1;
        parent->children = (TempNode**)new_children;
        parent->child_capacity = new_capacity;
    }
    parent->children[parent->child_count++] = child;
    child->parent = parent;
    return 0;
}

// Recursively frees the entire temporary tree.
void free_temp_tree(TempNode* t_node) {
    if (t_node == NULL) return;
    for (size_t i = 0; i < t_node->child_count; i++) {
        free_temp_tree(t_node->children[i]);
    }
    free(t_node->key);
    free(t_node->type);
    free(t_node->payload);
    free(t_node->children);
    free(t_node);
}

// Recursively converts the mutable TempNode tree to an immutable A-AST (post-order).
Node* convert_temp_to_aast(TempNode* t_node) {
    if (t_node == NULL) return NULL;

    Node** children_nodes = NULL;
    if (t_node->child_count > 0) {
        children_nodes = malloc(t_node->child_count * sizeof(Node*));
        if (children_nodes == NULL) return NULL;

        for (size_t i = 0; i < t_node->child_count; i++) {
            children_nodes[i] = convert_temp_to_aast(t_node->children[i]);
            if (children_nodes[i] == NULL) {
                // Cleanup on failure: release all previously created children
                for (size_t j = 0; j < i; j++) aast_release(children_nodes[j]);
                free(children_nodes);
                return NULL;
            }
        }
    }

    Node* new_node = create_node(t_node->type, t_node->key, t_node->payload, children_nodes, t_node->child_count);
    
    // After create_node, new_node owns the children. We must release our temporary hold.
    if (children_nodes != NULL) {
        for (size_t i = 0; i < t_node->child_count; i++) {
             // Retain is needed because the new parent owns them, and if we create a new state, we need to add a ref count
            if(new_node) aast_retain(children_nodes[i]);
            aast_release(children_nodes[i]);
        }
        free(children_nodes);
    }


    return new_node;
}

// Main ingestion function: parses indented text into a final, valid A-AST.
Node* aast_ingest_from_text(const char* text_data) {
    TempNode* temp_root = calloc(1, sizeof(TempNode));
    if (!temp_root) return NULL;

    TempNode* current_parent = temp_root;
    int prev_indent = -2; // Start at a value that allows 0 indent

    char* text_copy = strdup(text_data);
    char* line = strtok(text_copy, "\n");

    while (line != NULL) {
        char* line_start = line;
        while (*line_start == ' ') line_start++;
        
        int current_indent = line_start - line;
        if (current_indent % 2 != 0) {
            fprintf(stderr, "ERROR: Invalid indentation of %d spaces.\n", current_indent);
            free_temp_tree(temp_root); free(text_copy); return NULL;
        }

        if (current_indent > prev_indent + 2) {
            fprintf(stderr, "ERROR: Indentation jumped more than one level.\n");
            free_temp_tree(temp_root); free(text_copy); return NULL;
        }
        
        while (current_indent <= prev_indent) {
            current_parent = current_parent->parent;
            prev_indent -= 2;
        }

        // Parse key:TYPE:payload
        char* key = strsep(&line_start, ":");
        char* type = strsep(&line_start, ":");
        char* payload = line_start; // Whatever is left

        if (!key || !type) {
             fprintf(stderr, "ERROR: Malformed line. Expected key:TYPE.\n");
             free_temp_tree(temp_root); free(text_copy); return NULL;
        }

        TempNode* new_temp_node = calloc(1, sizeof(TempNode));
        new_temp_node->key = strdup(key);
        new_temp_node->type = strdup(type);
        if (payload) new_temp_node->payload = strdup(payload);

        add_child_to_temp_node(current_parent, new_temp_node);
        current_parent = new_temp_node;
        prev_indent = current_indent;
        
        line = strtok(NULL, "\n");
    }
    
    free(text_copy);

    // Convert the single child of the dummy root to the final A-AST
    Node* final_root = NULL;
    if (temp_root->child_count == 1) {
        final_root = convert_temp_to_aast(temp_root->children[0]);
    }
    
    free_temp_tree(temp_root);
    return final_root;
}

// Utility to read an entire file into a string.
char* read_file_to_string(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    char* buffer = malloc(length + 1);
    if (!buffer) return NULL;
    fread(buffer, 1, length, file);
    buffer[length] = '\0';
    fclose(file);
    return buffer;
}

// ----------------------------------------------------------------------------
// Phase 7: Persistence API
// ----------------------------------------------------------------------------

// A struct used by uthash to track nodes that have already been visited/serialized.
typedef struct VisitedNode {
    char hash[65];      // The key for the hash set.
    UT_hash_handle hh;  // This makes the structure hashable.
} VisitedNode;

/**
 * @brief (Internal) Recursive helper to serialize the A-AST in post-order.
 *
 * @param node The current node to process.
 * @param fp The file pointer to write to.
 * @param visited_set The uthash set of already-serialized nodes.
 * @return 0 on success, -1 on failure.
 */
int serialize_recursive_helper(const Node* node, FILE* fp, VisitedNode** visited_set) {
    if (node == NULL) return 0;

    // Check if we have already serialized this node (for DAGs).
    VisitedNode* found;
    HASH_FIND_STR(*visited_set, node->hash, found);
    if (found) {
        return 0; // Already processed, do not continue.
    }

    // Mark the current node as visited BEFORE recursing to prevent cycles.
    VisitedNode* new_visited = malloc(sizeof(VisitedNode));
    if (!new_visited) return -1; // Allocation failure
    strcpy(new_visited->hash, node->hash);
    HASH_ADD_STR(*visited_set, hash, new_visited);

    // --- Post-Order Traversal ---
    // First, recurse on all children. This ensures they are written to the file
    // before their parent.
    for (size_t i = 0; i < node->child_count; i++) {
        if (serialize_recursive_helper(node->children[i], fp, visited_set) != 0) {
            return -1; // Propagate failure
        }
    }

    // --- Write Current Node ---
    // Now that all children are guaranteed to be in the file, write this node.
    // Format: [HASH]|[TYPE]|[KEY_LEN]:[KEY]|[PAYLOAD_LEN]:[PAYLOAD]|[CHILD_HASH_1],...
    fprintf(fp, "%s|%s|%zu:%s|%zu:%s|",
            node->hash,
            node->type,
            node->key ? strlen(node->key) : 0, node->key ? node->key : "",
            node->payload ? strlen(node->payload) : 0, node->payload ? node->payload : ""
    );

    // Append child hashes
    for (size_t i = 0; i < node->child_count; i++) {
        fprintf(fp, "%s%s", node->children[i]->hash, (i < node->child_count - 1) ? "," : "");
    }
    fprintf(fp, "\n");

    return 0;
}

/**
 * @brief Serializes an entire A-AST to a file.
 *
 * The serialization is done in post-order (leaves-first) to enable
 * single-pass deserialization.
 *
 * @param root The root node of the tree to serialize.
 * @param filename The path of the file to write to.
 * @return 0 on success, -1 on failure.
 */
int aast_serialize_to_file(const Node* root, const char* filename) {
    if (!root || !filename) return -1;

    FILE* fp = fopen(filename, "w");
    if (!fp) {
        perror("Failed to open file for serialization");
        return -1;
    }

    // The visited_set is used to handle DAGs and avoid writing nodes multiple times.
    VisitedNode* visited_set = NULL;
    int result = serialize_recursive_helper(root, fp, &visited_set);
    fclose(fp);

    // Clean up the uthash set to prevent memory leaks.
    VisitedNode *current_node, *tmp;
    HASH_ITER(hh, visited_set, current_node, tmp) {
        HASH_DEL(visited_set, current_node);
        free(current_node);
    }

    if (result != 0) {
        fprintf(stderr, "Serialization failed.\n");
        return -1;
    }

    return 0;
}

// ----------------------------------------------------------------------------
// Execution Sandbox & Test Harness
// ----------------------------------------------------------------------------
int main() {
    printf("========================================\n");
    printf("A-AST Ingestion & Serialization Test\n");
    printf("========================================\n\n");

    // --- STEP 1: Read and Ingest data from file ---
    printf("--- Ingesting data from ingest_data.txt ---\n");
    char* file_content = read_file_to_string("ingest_data.txt");
    if (!file_content) return 1;

    Node* root = aast_ingest_from_text(file_content);
    free(file_content);

    if (!root) {
        fprintf(stderr, "A-AST ingestion failed.\n");
        return 1;
    }
    printf("Ingestion successful. Root Hash: %s\n\n", root->hash);

    // --- STEP 2: Verify the in-memory tree ---
    printf("--- Verifying ingested tree ---\n");
    if (aast_verify_integrity(root)) {
        printf("Integrity verification... PASSED\n");
    } else {
        printf("Integrity verification... FAILED\n");
    }

#ifdef DEBUG_PRINT
    aast_print_tree(root);
#endif

    // --- STEP 3: Serialize the A-AST to a new file ---
    const char* output_filename = "aast.dat";
    printf("\n--- Serializing tree to %s ---\n", output_filename);
    if (aast_serialize_to_file(root, output_filename) == 0) {
        printf("Serialization successful.\n");
        printf("Check the contents of the '%s' file.\n", output_filename);
    } else {
        printf("Serialization failed.\n");
    }

    // --- STEP 4: Safe Cleanup ---
    printf("\n--- Cleanup Phase ---\n");
    aast_release(root);
    printf("Cleanup complete.\n");
    printf("========================================\n");

    return 0;
}
