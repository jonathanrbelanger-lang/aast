#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 1. Type Definitions (Enum)
// This maps directly to an integer under the hood, making it highly efficient.
typedef enum {
    ROOT = 0,
    HEADER = 1,
    PARAGRAPH = 2,
    TEXT = 3
} NodeType;

// 2. The Semantic Node Structure
// This is the blueprint for our memory blocks.
typedef struct Node {
    char hash[65];             // 64 hex characters for SHA-256 + 1 for the null terminator ('\0')
    NodeType type;
    char* payload;             // Pointer to a character array (string)
    struct Node** children;    // Pointer to an array of Node pointers (the branches)
    size_t child_count;        // C needs to know exactly how many children exist in that array
} Node;

// 3. The Constructor
// This function allocates memory on the Heap and locks in the data.
Node* create_node(NodeType type, const char* payload, Node** children, size_t child_count) {
    
    // Ask the OS for exactly enough memory to hold one Node struct
    Node* new_node = (Node*)malloc(sizeof(Node));
    if (new_node == NULL) {
        fprintf(stderr, "Fatal: Memory allocation failed\n");
        exit(1);
    }

    new_node->type = type;
    new_node->child_count = child_count;

    // Handle the Payload (Immutability Step)
    // strdup allocates exact memory for the string and copies it, 
    // ensuring the node owns its data entirely.
    if (payload != NULL) {
        new_node->payload = strdup(payload); 
    } else {
        new_node->payload = NULL;
    }

    // Handle the Children Array
    if (child_count > 0 && children != NULL) {
        // Allocate memory for an array of pointers
        new_node->children = (Node**)malloc(child_count * sizeof(Node*));
        for (size_t i = 0; i < child_count; i++) {
            new_node->children[i] = children[i];
        }
    } else {
        new_node->children = NULL;
    }

    // Placeholder for Phase 2: Canonical Byte Packing and Hashing
    snprintf(new_node->hash, 65, "pending_hash_computation...");

    return new_node;
}

// Recursively free the A-AST memory (Post-Order Traversal)
void free_node(Node* node) {
    if (node == NULL) return;

    // 1. Free the children first
    if (node->child_count > 0 && node->children != NULL) {
        for (size_t i = 0; i < node->child_count; i++) {
            free_node(node->children[i]); // Recurse down to the leaves
        }
        free(node->children); // Free the array that held the child pointers
    }

    // 2. Free the payload (which we allocated with strdup)
    if (node->payload != NULL) {
        free(node->payload);
    }

    // 3. Free the node struct itself
    free(node);
}

// 4. Execution
int main() {
    // We build from the leaves up to the root (Accretion)
    
    // Step 1: Create leaf nodes (0 children)
    Node* text1 = create_node(TEXT, "This is an A-AST concept.", NULL, 0);
    Node* text2 = create_node(TEXT, " It is built in C.", NULL, 0);

    // Step 2: Create a paragraph node, passing an array of the leaf pointers
    Node* paragraph_children[] = {text1, text2};
    Node* paragraph = create_node(PARAGRAPH, NULL, paragraph_children, 2);

    // Step 3: Create a header node
    Node* header_text = create_node(TEXT, "Architectural Overview", NULL, 0);
    Node* header_children[] = {header_text};
    Node* header = create_node(HEADER, NULL, header_children, 1);

    // Step 4: Create the Root node
    Node* root_children[] = {header, paragraph};
    Node* root = create_node(ROOT, NULL, root_children, 2);

    printf("A-AST successfully built in C memory space.\n");
    printf("Root type: %d\n", root->type);
    printf("Root has %zu children.\n", root->child_count);

    // Note: In C, we must write a free_node() function to clean up this memory.
    // For this minimal toy step, the OS reclaims it when the program exits.

// Clean up the A-AST
    free_node(root);
    printf("Memory successfully reclaimed.\n");

    return 0;
}
