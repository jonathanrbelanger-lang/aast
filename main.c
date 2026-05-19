/**
 * @file main.c
 * @brief A-AST (Accretive Abstract Syntax Tree) Core Implementation.
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
// Phase 7: Execution Sandbox & Test Harness
// ----------------------------------------------------------------------------

int main() {
    printf("========================================\n");
    printf("A-AST Test Harness Initialized\n");
    printf("========================================\n\n");

    // --- STEP 1: Build Initial State (v1) ---
    Node* text1 = create_node("TEXT", "concept_intro", "This is an A-AST concept.", NULL, 0);
    Node* text2 = create_node("TEXT", "language_spec", "It is built in C.", NULL, 0);
    Node* paragraph_children[] = {text1, text2};
    Node* paragraph = create_node("PARAGRAPH", "intro_paragraph", NULL, paragraph_children, 2);
    Node* header_text = create_node("TEXT", "header_text", "Architectural Overview", NULL, 0);
    Node* header_children[] = {header_text};
    Node* header = create_node("HEADER", "main_header", NULL, header_children, 1);
    Node* root_children[] = {header, paragraph};
    Node* root_v1 = create_node("ROOT", "document_root", NULL, root_children, 2);
    
    printf("--- State v1 Initialized ---\n");
    printf("Root v1 Hash: %s\n", root_v1->hash);
    printf("Verifying v1 integrity... %s\n", aast_verify_integrity(root_v1) ? "PASSED" : "FAILED");

#ifdef DEBUG_PRINT // <-- INSERTED FOR PHASE 6.5 (START)
    aast_print_tree(root_v1);
    printf("\n");
#endif // <-- INSERTED FOR PHASE 6.5 (END)

    // --- STEP 2: Accrete New State (v2) ---
    const char* const path[] = {"document_root", "intro_paragraph", "concept_intro"};
    const char* new_payload = "This is the NEW, updated payload.";
    Node* root_v2 = accrete_new_state(root_v1, path, 3, new_payload);

    printf("--- State v2 Accreted ---\n");
    if (root_v2) {
        printf("Root v2 Hash: %s\n", root_v2->hash);
        printf("Verifying v2 integrity... %s\n", aast_verify_integrity(root_v2) ? "PASSED" : "FAILED");
        if (root_v1->children[0] == root_v2->children[0]) {
             printf("Structural sharing... PASSED (unmodified branch is shared)\n");
        } else {
             printf("Structural sharing... FAILED (unmodified branch was re-allocated)\n");
        }

#ifdef DEBUG_PRINT // <-- INSERTED FOR PHASE 6.5 (START)
        aast_print_tree(root_v2);
        printf("\n");
#endif // <-- INSERTED FOR PHASE 6.5 (END)

    } else {
        printf("Accretion failed.\n\n");
    }

    // --- STEP 3: Demonstrate Tampering Detection ---
    printf("--- Tampering Simulation ---\n");
    printf("Manually corrupting 'text2' payload without re-hashing...\n");
    // Direct memory manipulation like this is what the A-AST is designed to detect.
    strcpy(text2->payload, "CORRUPTED DATA!");
    printf("Verifying v1 integrity after tamper... %s\n", aast_verify_integrity(root_v1) ? "FAILED (UNEXPECTED PASS)" : "PASSED (Tampering Detected)");
    printf("Verifying v2 integrity after tamper... %s\n\n", aast_verify_integrity(root_v2) ? "FAILED (UNEXPECTED PASS)" : "PASSED (Tampering Detected)");


    // --- STEP 4: Safe Cleanup ---
    printf("--- Cleanup Phase ---\n");
    printf("Releasing root_v2...\n");
    aast_release(root_v2);
    printf("Releasing root_v1...\n");
    aast_release(root_v1);
    printf("Cleanup complete.\n");
    printf("========================================\n");

    return 0;
}
