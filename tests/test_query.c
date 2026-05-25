#include <stdio.h>
#include <stdlib.h>
#include "aast.h"

// --- The Callback Function for Discovery ---
void my_discovery_callback(const char* key, const Node* child, void* context) {
    int* counter = (int*)context; // Cast the void pointer back to an int pointer
    (*counter)++;
    printf("  -> Discovered Child %d | Key: '%s', Type: '%s'\n", *counter, key, child->type);
}

int main() {
    printf("Starting Query API Test...\n");

    // 1. Build a 3-level tree: Root -> User -> Role
    Node* role_child = create_node("Role", "Admin", NULL, 0);
    AastChildInput role_input = { .key = "role_data", .child = role_child };
    
    Node* user_child = create_node("User", "Alice", &role_input, 1);
    
    // Give the root multiple children to prove iteration works
    Node* settings_child = create_node("Settings", "Dark_Mode", NULL, 0);
    
    AastChildInput root_inputs[2] = {
        { .key = "user_1", .child = user_child },
        { .key = "app_config", .child = settings_child }
    };
    
    Node* root = create_node("Root", "Database", root_inputs, 2);
    
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

    // 4. Test Discovery (Iteration)
    printf("\n--- Testing Discovery API (Iterator) ---\n");
    int hit_count = 0;
    aast_iterate_children(root, my_discovery_callback, &hit_count);
    
    if (hit_count == 2) {
        printf("SUCCESS: Correctly discovered and iterated over 2 children.\n");
    } else {
        printf("FAILED: Expected 2 children, found %d.\n", hit_count);
    }

    // 5. Cleanup
    aast_release(root);
    aast_release(user_child);
    aast_release(role_child);
    aast_release(settings_child);

    printf("\nTest Complete. Memory ready for Valgrind inspection.\n");
    return 0;
}
