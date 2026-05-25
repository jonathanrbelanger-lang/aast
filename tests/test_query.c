#include <stdio.h>
#include <stdlib.h>
#include "aast.h"

int main() {
    printf("Starting Query API Test...\n");

    // 1. Build a 3-level tree: Root -> User -> Role
    Node* role_child = create_node("Role", "Admin", NULL, 0);
    AastChildInput role_input = { .key = "role_data", .child = role_child };
    
    Node* user_child = create_node("User", "Alice", &role_input, 1);
    AastChildInput user_input = { .key = "user_1", .child = user_child };
    
    Node* root = create_node("Root", "Database", &user_input, 1);
    
    // 2. Test Shallow Query (O1 Lookup)
    printf("\n--- Testing Shallow Query ---\n");
    const Node* found1 = aast_find_child_by_key(root, "user_1");
    if (found1) printf("SUCCESS: Found child! Type: %s, Payload: %s\n", found1->type, found1->payload);
    else printf("FAILED: Could not find child.\n");

    // 3. Test Deep Path Query
    printf("\n--- Testing Deep Path Query ---\n");
    const char* target_path[] = {"user_1", "role_data"};
    const Node* found2 = aast_query_path(root, target_path, 2);
    
    if (found2) {
        printf("SUCCESS: Found deep child! Type: %s, Payload: %s\n", found2->type, found2->payload);
    } else {
        printf("FAILED: Could not find deep child.\n");
    }

    // 4. Test Deep Path Query (Failure Case)
    const char* bad_path[] = {"user_1", "invalid_key"};
    const Node* missing = aast_query_path(root, bad_path, 2);
    if (!missing) {
        printf("SUCCESS: Correctly returned NULL for broken path.\n");
    }

    // 5. Cleanup
    aast_release(root);
    aast_release(user_child);
    aast_release(role_child);

    printf("\nTest Complete. Memory ready for Valgrind inspection.\n");
    return 0;
}
