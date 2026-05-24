#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "aast.h" // Assuming your core library header

// Define the absolute physical limits you want to test
// Inside test_phase_a.c main()
if (argc != 3) {
    fprintf(stderr, "Usage: %s <max_children> <max_payload_bytes>\n", argv[0]);
    return 1;
}

int max_children = atoi(argv[1]);
size_t max_payload = (size_t)atol(argv[2]);
int main() {
    printf("Initializing Phase A Maximum Node Fill Test...\n");

    // 1. Allocate massive synthetic payload
    char *massive_payload = malloc(TEST_MAX_PAYLOAD_SIZE);
    if (!massive_payload) {
        fprintf(stderr, "Failed to allocate payload buffer.\n");
        return 1;
    }
    memset(massive_payload, 'X', TEST_MAX_PAYLOAD_SIZE - 1);
    massive_payload[TEST_MAX_PAYLOAD_SIZE - 1] = '\0';

    // 2. Generate maximum child inputs
    AastChildInput *children = malloc(sizeof(AastChildInput) * TEST_MAX_CHILDREN);
    if (!children) {
        fprintf(stderr, "Failed to allocate children array.\n");
        free(massive_payload);
        return 1;
    }

    // Populate children with generic data to trigger uthash hashing
    for (int i = 0; i < TEST_MAX_CHILDREN; i++) {
        char key_buf[TEST_MAX_KEY_LEN];
        snprintf(key_buf, sizeof(key_buf), "child_key_%08d", i);
        
        // Assuming your child struct has key and a dummy hash/node reference
        children[i].key = strdup(key_buf); 
        // children[i].node = ... (create dummy leaf nodes if required by your API)
    }

    // 3. Construct the Mega-Node
    printf("Constructing node with %d children and %d byte payload...\n", TEST_MAX_CHILDREN, TEST_MAX_PAYLOAD_SIZE);
    
    // Execute core function
    // Node *mega_node = create_node("mega_type", massive_payload, children, TEST_MAX_CHILDREN);

    // 4. Teardown & Release
    // aast_release(mega_node);
    
    for (int i = 0; i < TEST_MAX_CHILDREN; i++) {
        free((void*)children[i].key);
    }
    free(children);
    free(massive_payload);

    printf("Phase A Test Complete.\n");
    return 0;
}
