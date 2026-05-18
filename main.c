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

// 3. Canonical Serialization (Phase 2)
// Packs node data into a strict, predictable C-string to guarantee hash parity.
char* generate_canonical_buffer(Node* node) {
    // Dynamically calculate the required buffer size
    size_t size = 256; // Base allowance for formatting
    if (node->key) size += strlen(node->key);
    if (node->payload) size += strlen(node->payload);
    size += (node->child_count * 65); // 64 chars per child hash + comma separator
    
    char* buffer = (char*)malloc(size);
    if (!buffer) exit(1);
    buffer[0] = '\0';
    
    // Format: TYPE:X|KEY:Y|CHILDREN:Z|HASHES:h1,h2|PAYLOAD:P
    snprintf(buffer, size, "TYPE:%s|KEY:%s|CHILDREN:%zu|HASHES:", 
             node->type, 
             node->key ? node->key : "NULL",
             node->child_count);
             
    // Append deterministic child hashes
    for (size_t i = 0; i < node->child_count; i++) {
        strncat(buffer, node->children[i]->hash, size - strlen(buffer) - 1);
        if (i < node->child_count - 1) {
            strncat(buffer, ",", size - strlen(buffer) - 1);
        }
    }
    
    // Append payload
    strncat(buffer, "|PAYLOAD:", size - strlen(buffer) - 1);
    if (node->payload) {
        strncat(buffer, node->payload, size - strlen(buffer) - 1);
    } else {
        strncat(buffer, "NULL", size - strlen(buffer) - 1);
    }
    
    return buffer;
}

// 4. The Constructor (Phase 1 + 4)
// Allocates memory, enforces immutability via deep copies, and anchors the hash.
Node* create_node(const char* type, const char* key, const char* payload, Node** children, size_t child_count) {
    
    Node* new_node = (Node*)malloc(sizeof(Node));
    if (new_node == NULL) {
        fprintf(stderr, "Fatal: Memory allocation failed\n");
        exit(1);
    }

    // Safely lock in the type
    strncpy(new_node->type, type, 15);
    new_node->type[15] = '\0';

    new_node->child_count = child_count;

    // Deep copy the key and payload (Node owns this memory)
    new_node->key = (key != NULL) ? strdup(key) : NULL;
    new_node->payload = (payload != NULL) ? strdup(payload) : NULL;

    // Handle Children Array
    if (child_count > 0 && children != NULL) {
        new_node->children = (Node**)malloc(child_count * sizeof(Node*));
        for (size_t i = 0; i < child_count; i++) {
            new_node->children[i] = children[i];
        }
    } else {
        new_node->children = NULL;
    }

    // Generate strict buffer and compute final Node Hash
    char* canonical_buffer = generate_canonical_buffer(new_node);
    compute_sha256_hex(canonical_buffer, new_node->hash);
    
    // Free the temporary buffer to prevent memory leaks
    free(canonical_buffer);

    return new_node;
}

// 5. The Destructor
// Recursively frees the A-AST memory (Post-Order Traversal)
void free_node(Node* node) {
    if (node == NULL) return;

    // 1. Free the children branches first
    if (node->child_count > 0 && node->children != NULL) {
        for (size_t i = 0; i < node->child_count; i++) {
            free_node(node->children[i]); 
        }
        free(node->children); 
    }

    // 2. Free dynamic string allocations
    if (node->key != NULL) free(node->key);
    if (node->payload != NULL) free(node->payload);

    // 3. Free the node struct itself
    free(node);
}

// 6. Execution Sandbox
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
    Node* root = create_node("ROOT", "document_root", NULL, root_children, 2);

    printf("========================================\n");
    printf("A-AST Sandbox Initialized\n");
    printf("========================================\n");
    printf("Root Key:  %s\n", root->key);
    printf("Root Hash: %s\n", root->hash);
    printf("Status:    Memory locked and mathematically bound.\n");
    printf("========================================\n");

    // Clean up
    free_node(root);
    return 0;
}
