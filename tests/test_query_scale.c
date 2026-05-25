#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "aast.h"

// Dummy callback for iteration test
void dummy_callback(const char* key, const Node* child, void* context) {
    (void)key; (void)child;
    long* count = (long*)context;
    (*count)++;
}

void test_horizontal_scale(long child_count) {
    printf("--- Horizontal Scale Test: %ld Children ---\n", child_count);
    
    AastChildInput* inputs = malloc(child_count * sizeof(AastChildInput));
    Node* dummy_child = create_node("Leaf", "Data", NULL, 0);
    char key_buf[32];

    for (long i = 0; i < child_count; i++) {
        sprintf(key_buf, "key_%ld", i);
        inputs[i].key = strdup(key_buf);
        inputs[i].child = dummy_child;
    }

    // 1. Measure Construction
    clock_t start = clock();
    Node* root = create_node("Root", "Data", inputs, child_count);
    clock_t end = clock();
    double build_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("Build Time: %f seconds\n", build_time);

    // 2. Measure O(1) Lookup (Worst Case: Last Key)
    sprintf(key_buf, "key_%ld", child_count - 1);
    start = clock();
    const Node* found = aast_find_child_by_key(root, key_buf);
    end = clock();
    if (found) {
        printf("O(1) Lookup Time: %f seconds\n", ((double)(end - start)) / CLOCKS_PER_SEC);
    } else {
        printf("O(1) Lookup FAILED.\n");
    }

    // 3. Measure O(N) Iteration
    long hit_count = 0;
    start = clock();
    aast_iterate_children(root, dummy_callback, &hit_count);
    end = clock();
    printf("Iteration Time (%ld nodes): %f seconds\n", hit_count, ((double)(end - start)) / CLOCKS_PER_SEC);

    // Cleanup
    aast_release(root);
    aast_release(dummy_child);
    for (long i = 0; i < child_count; i++) free((void*)inputs[i].key);
    free(inputs);
}

void test_vertical_scale(long depth) {
    printf("--- Vertical Scale Test: %ld Depth ---\n", depth);
    
    Node* current = create_node("Leaf", "Data", NULL, 0);
    AastChildInput input;
    
    // Build bottom-up (Immutable style)
    for (long i = 0; i < depth - 1; i++) {
        input.key = "child";
        input.child = current;
        Node* parent = create_node("Node", NULL, &input, 1);
        aast_release(current); // Transfer ownership to parent
        current = parent;
    }
    Node* root = current;

    // Build the query path array
    const char** path = malloc((depth - 1) * sizeof(char*));
    for (long i = 0; i < depth - 1; i++) path[i] = "child";

    // Measure Deep Path Traversal
    clock_t start = clock();
    const Node* found = aast_query_path(root, path, depth - 1);
    clock_t end = clock();

    if (found) {
        printf("Deep Path Traversal Time: %f seconds\n", ((double)(end - start)) / CLOCKS_PER_SEC);
    } else {
        printf("Deep Path Traversal FAILED.\n");
    }

    // Cleanup
    aast_release(root);
    free(path);
}

int main(int argc, char** argv) {
    if (argc != 3) {
        printf("Usage: %s <horizontal|vertical> <count>\n", argv[0]);
        return 1;
    }
    
    long count = atol(argv[2]);
    if (strcmp(argv[1], "horizontal") == 0) {
        test_horizontal_scale(count);
    } else if (strcmp(argv[1], "vertical") == 0) {
        test_vertical_scale(count);
    } else {
        printf("Unknown mode.\n");
    }
    return 0;
}
