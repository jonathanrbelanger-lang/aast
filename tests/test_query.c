#include <stdio.h>
#include <stdlib.h>
// The Makefile will use -I. or -I../.. so this include finds the root header
#include "aast.h"

int main() {
    printf("Starting Query API Test...\n");

    // 1. Create a child node
    Node* child = create_node("User", "Alice", NULL, 0);
    AastChildInput input = { .key = "user_1", .child = child };
    
    // 2. Create a parent node
    Node* root = create_node("Root", "Database", &input, 1);
    
    // 3. Test the Query API
    printf("Searching for 'user_1'...\n");
    const Node* found = aast_find_child_by_key(root, "user_1");
    
    if (found) {
        printf("SUCCESS: Found child! Type: %s, Payload: %s\n", found->type, found->payload);
    } else {
        printf("FAILED: Could not find child.\n");
    }

    // 4. Test Failure Case
    const Node* missing = aast_find_child_by_key(root, "non_existent");
    if (!missing) {
        printf("SUCCESS: Correctly returned NULL for missing key.\n");
    }

    // 5. Cleanup
    // (We ONLY release the root and the initial child pointer, NEVER 'found' or 'missing')
    aast_release(root);
    aast_release(child);

    printf("Test Complete. Memory ready for Valgrind inspection.\n");
    return 0;
}
