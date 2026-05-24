#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../aast.h" // Relative path since this is in tests/

#define TEST_MAX_KEY_LEN 256

int main(int argc, char *argv[]) {
    // Enforce CLI arguments
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <max_children> <max_payload_bytes>\n", argv[0]);
        return 1;
    }

    int max_children = atoi(argv[1]);
    size_t max_payload = (size_t)atol(argv[2]);

    printf("Initializing Phase A Maximum Node Fill Test...\n");
    printf("Target: %d children, %zu bytes payload\n", max_children, max_payload);

    // 1. Allocate massive synthetic payload
    char *massive_payload = malloc(max_payload);
    if (!massive_payload) {
        fprintf(stderr, "Failed to allocate payload buffer.\n");
        return 1;
    }
    memset(massive_payload, 'X', max_payload - 1);
    massive_payload[max_payload - 1] = '\0';

    // 2. Generate maximum child inputs
    AastChildInput *children = malloc(sizeof(AastChildInput) * max_children);
    if (!children) {
        fprintf(stderr, "Failed to allocate children array.\n");
        free(massive_payload);
        return 1;
    }

    // Populate children with generic data
    for (int i = 0; i < max_children; i++) {
        char key_buf[TEST_MAX_KEY_LEN];
        snprintf(key_buf, sizeof(key_buf), "child_key_%08d", i);
        
        children[i].key = strdup(key_buf); 
        children[i].child = NULL; // Explicitly NULL for dummy leaves
    }

    // 3. Construct the Mega-Node
    printf("Constructing the node...\n");
    Node *mega_node = create_node("mega_type", massive_payload, children, max_children);

    if (!mega_node) {
        fprintf(stderr, "Node creation failed (Likely stack/heap exhaustion).\n");
    } else {
        printf("Node created successfully. Root hash: %s\n", mega_node->hash);
    }

    // 4. Teardown & Release
    if (mega_node) {
        aast_release(mega_node);
    }
    
    // create_node performs a deep copy of the keys, so we must free our local allocations
    for (int i = 0; i < max_children; i++) {
        free((void*)children[i].key);
    }
    free(children);
    free(massive_payload);

    printf("Phase A Test Complete.\n");
    return 0;
}
