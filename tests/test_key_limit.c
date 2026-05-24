#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../aast.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <key_byte_length>\n", argv[0]);
        return 1;
    }

    size_t target_key_len = (size_t)atol(argv[1]);
    int test_child_count = 2000; // Keep child count stable to isolate key length

    printf("Executing Internal Container Audit: Key Length Axis\n");
    printf("Testing 2000 children with a key length of %zu bytes each...\n", target_key_len);

    Node *dummy_leaf = create_node("leaf", "data", NULL, 0);
    AastChildInput *children = malloc(sizeof(AastChildInput) * test_child_count);
    
    // 1. Generate massive keys
    for (int i = 0; i < test_child_count; i++) {
        char *long_key = malloc(target_key_len + 1);
        if (!long_key) {
            fprintf(stderr, "Failed to allocate memory for stress key.\n");
            return 1;
        }
        
        // Fill the key with predictable padding, appending the loop index to ensure uniqueness
        memset(long_key, 'K', target_key_len);
        snprintf(long_key + (target_key_len - 12), 12, "_%09d", i);
        long_key[target_key_len] = '\0';
        
        children[i].key = long_key;
        children[i].child = dummy_leaf;
    }

    // 2. Profile construction latency
    clock_t start_time = clock();
    Node *mega_node = create_node("stress_type", "nominal_payload", children, test_child_count);
    clock_t end_time = clock();
    
    double elapsed_ms = ((double)(end_time - start_time) / CLOCKS_PER_SEC) * 1000.0;

    if (!mega_node) {
        fprintf(stderr, "[CRASH] Node creation failed at key length %zu\n", target_key_len);
    } else {
        printf("[SUCCESS] Node created in %.4f ms. Hash: %s\n", elapsed_ms, mega_node->hash);
        aast_release(mega_node);
    }

    // 3. Clean up
    aast_release(dummy_leaf);
    for (int i = 0; i < test_child_count; i++) {
        free((void*)children[i].key);
    }
    free(children);

    return mega_node ? 0 : 1;
}
