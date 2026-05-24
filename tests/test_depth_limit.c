#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../aast.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <target_depth>\n", argv[0]);
        return 1;
    }

    int target_depth = atoi(argv[1]);
    printf("Executing Vertical Architecture Audit: Depth Axis\n");
    printf("Attempting to construct a linear tree of depth %d...\n", target_depth);

    // Start with a base leaf node
    Node *current_root = create_node("leaf", "baseline_depth_data", NULL, 0);
    if (!current_root) {
        fprintf(stderr, "Failed to initialize base leaf node.\n");
        return 1;
    }

    // Stack nodes vertically by continuously making the previous root the child of a new root
    for (int i = 0; i < target_depth; i++) {
        AastChildInput child_input;
        child_input.key = "next_depth_step";
        child_input.child = current_root;

        Node *new_root = create_node("spine_node", NULL, &child_input, 1);
        
        // Release our local ownership of the old root, since the new root now owns it
        aast_release(current_root);

        if (!new_root) {
            fprintf(stderr, "[CRASH] Tree construction ruptured at depth layer %d\n", i);
            return 1;
        }
        current_root = new_root;
    }

    printf("[SUCCESS] Built vertical tree of depth %d. Root Hash: %s\n", target_depth, current_root->hash);

    // Test recursive verification
    printf("Triggering recursive integrity check...\n");
    int valid = aast_verify_integrity(current_root);
    printf("Integrity result: %s\n", valid ? "VALID" : "INVALID/RECURSION_LIMIT_HIT");

    // Test recursive deallocation
    printf("Triggering recursive deallocation...\n");
    aast_release(current_root);

    printf("Vertical Depth Test Complete.\n");
    return 0;
}
