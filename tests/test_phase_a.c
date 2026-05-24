#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../aast.h" 

#define TEST_MAX_KEY_LEN 256

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <max_children> <max_payload_bytes>\n", argv[0]);
        return 1;
    }

    int max_children = atoi(argv[1]);
    size_t max_payload = (size_t)atol(argv[2]);

    printf("Initializing Phase A Maximum Node Fill Test...\n");

    // 0. Create a single valid leaf node for structural sharing
    Node *dummy_leaf = create_node("leaf", "dummy_data", NULL, 0);
    if (!dummy_leaf) {
        fprintf(stderr, "Failed to initialize dummy leaf.\n");
        return 1;
    }

    // 1. Allocate payload
    char *massive_payload = malloc(max_payload);
    if (!massive_payload) {
        fprintf(stderr, "Failed to allocate payload buffer.\n");
        return 1;
    }
    memset(massive_payload, 'X', max_payload - 1);
    massive_payload[max_payload - 1] = '\0';

    // 2. Generate child inputs
    AastChildInput *children = malloc(sizeof(AastChildInput) * max_children);
    if (!children) {
        fprintf(stderr, "Failed to allocate children array.\n");
        free(massive_payload);
        return 1;
    }

    // Populate children, pointing ALL of them to the single dummy_leaf
    for (int i = 0; i < max_children; i++) {
        char key_buf[TEST_MAX_KEY_LEN];
        snprintf(key_buf, sizeof(key_buf), "child_key_%08d", i);
        
        size_t key_len = strlen(key_buf) + 1;
        char *safe_key = malloc(key_len);
        if (!safe_key) {
            fprintf(stderr, "Failed to allocate key string.\n");
            exit(1);
        }
        strcpy(safe_key, key_buf);
        
        children[i].key = safe_key; 
        children[i].child = dummy_leaf; // Provide a valid Node pointer
    }

    // 3. Construct the Mega-Node
    printf("Constructing the node...\n");
    Node *mega_node = create_node("mega_type", massive_payload, children, max_children);

    if (!mega_node) {
        fprintf(stderr, "Node creation failed.\n");
    } else {
        printf("Node created successfully. Root hash: %s\n", mega_node->hash);
    }

    // 4. Teardown & Release
    if (mega_node) {
        aast_release(mega_node);
    }
    // Also release our initial reference to the dummy leaf
    aast_release(dummy_leaf);
    
    for (int i = 0; i < max_children; i++) {
        free((void*)children[i].key);
    }
    free(children);
    free(massive_payload);

    printf("Phase A Test Complete.\n");
    return 0;
}
