#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/evp.h>

// --- Minimal A-AST Implementation for Testing ---

typedef struct Node {
    char type[16];
    char *key;
    char *payload;
    struct Node **children;
    size_t child_count;
    size_t ref_count;
    char hash[65];
} Node;

// NOTE: These are simplified/copied versions for this self-contained test.
void compute_sha256_hex(const char* data, char outputBuffer[65]) {
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    const EVP_MD *md = EVP_sha256();
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int md_len;
    EVP_DigestInit_ex(mdctx, md, NULL);
    EVP_DigestUpdate(mdctx, data, strlen(data));
    EVP_DigestFinal_ex(mdctx, hash, &md_len);
    EVP_MD_CTX_free(mdctx);
    for(unsigned int i = 0; i < md_len; i++) sprintf(outputBuffer + (i * 2), "%02x", hash[i]);
    outputBuffer[64] = '\0';
}

char* generate_canonical_buffer(const Node* node) {
    char* buffer = malloc(256 + (node->child_count * 65)); // Simplified for test
    if(!buffer) return NULL;
    snprintf(buffer, 256, "%zu:%s|%zu:%s|%zu:|%zu:%s", strlen(node->type), node->type,
        node->key ? strlen(node->key):0, node->key ? node->key:"", node->child_count,
        node->payload ? strlen(node->payload):0, node->payload ? node->payload:"");
    return buffer;
}

Node* create_node(const char* type, const char* key, Node** children, size_t child_count) {
    Node* new_node = (Node*)malloc(sizeof(Node));
    if(!new_node) return NULL;
    new_node->ref_count = 1;
    strncpy(new_node->type, type, 15); new_node->type[15]='\0';
    new_node->key = strdup(key);
    new_node->payload = NULL;
    new_node->child_count = child_count;
    new_node->children = NULL;
    if(child_count > 0) {
        new_node->children = malloc(child_count * sizeof(Node*));
        memcpy(new_node->children, children, child_count * sizeof(Node*));
    }
    char* buf = generate_canonical_buffer(new_node);
    compute_sha256_hex(buf, new_node->hash);
    free(buf);
    return new_node;
}

void aast_release(Node* node) {
    if(!node) return;
    node->ref_count--;
    if(node->ref_count == 0) {
        for(size_t i = 0; i < node->child_count; i++) aast_release(node->children[i]);
        free(node->children);
        free(node->key);
        free(node->payload);
        free(node);
    }
}

// --- Boundary Test Harness ---

/**
 * @brief Generates a "stick" tree of a specified depth.
 * Each node has exactly one child, creating a deep chain.
 */
Node* generate_deep_tree(size_t depth) {
    if (depth == 0) return NULL;

    Node* current_leaf = create_node("LEAF", "leaf_node", NULL, 0);
    if (!current_leaf) {
        fprintf(stderr, "Failed to allocate initial leaf.\n");
        return NULL;
    }

    for (size_t i = 1; i < depth; ++i) {
        char key_buffer[64];
        snprintf(key_buffer, sizeof(key_buffer), "node_%zu", depth - i);
        Node* parent = create_node("NODE", key_buffer, &current_leaf, 1);
        if (!parent) {
            fprintf(stderr, "Allocation failed at depth %zu\n", i);
            aast_release(current_leaf); // Clean up what we've built
            return NULL;
        }
        current_leaf = parent;
    }
    return current_leaf;
}

int main() {
    printf("--- A-AST Boundary Test: Max Recursion Depth ---\n");
    printf("This test will incrementally increase tree depth until a crash (Segmentation Fault).\n");
    printf("The last successfully tested depth is the approximate stack limit.\n\n");

    // Most systems have a stack limit that allows for depths well over 10,000.
    // We will step by 1,000 to find the boundary faster.
    size_t start_depth = 10000;
    size_t step = 1000;
    size_t max_safe_depth = 0;

    for (size_t depth = start_depth; ; depth += step) {
        printf("Testing depth: %-10zu...", depth);
        fflush(stdout); // Force print before potential crash

        Node* deep_tree = generate_deep_tree(depth);
        if (!deep_tree) {
            printf("FAILED (Allocation)\n");
            break;
        }

        // The recursive call to aast_release is what we are testing.
        aast_release(deep_tree);

        printf("OK\n");
        max_safe_depth = depth;
    }

    printf("\n--- Test Halted ---\n");
    printf("The maximum safe depth before failure was approximately: %zu\n", max_safe_depth);

    return 0;
}
