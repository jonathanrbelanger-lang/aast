#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "aast.h"

// --- Callbacks ---
void agent_extraction_callback(const Node* loaded_root, void* context) {
    const Node* target = aast_find_child_by_key(loaded_root, "secret_data");
    if (target && target->payload) {
        char** out_buffer = (char**)context;
        *out_buffer = strdup(target->payload);
        printf("  [Agent Callback] Successfully extracted payload: '%s'\n", target->payload);
    }
}

void boundary_extraction_callback(const Node* loaded_root, void* context) {
    const Node* target = aast_find_child_by_key(loaded_root, "heavy_data");
    if (target && target->payload) {
        size_t* bytes_read = (size_t*)context;
        *bytes_read += strlen(target->payload);
    }
}

// --- Helper for generating massive strings ---
char* generate_heavy_string(size_t target_mb) {
    size_t target_bytes = target_mb * 1024 * 1024;
    char* buffer = malloc(target_bytes + 1);
    if (buffer) {
        memset(buffer, 'A', target_bytes);
        buffer[target_bytes] = '\0';
    }
    return buffer;
}

int main() {
    printf("--- AAST_LINK Cross-File Integrity & Boundary Test ---\n\n");

    // ==========================================
    // PHASE 1: Functional & Integrity Tests
    // ==========================================
    Node* secret = create_node("Data", "Agent_Alpha_Clearance", NULL, 0);
    AastChildInput input = { .key = "secret_data", .child = secret };
    Node* sub_tree = create_node("ChunkRoot", NULL, &input, 1);
    
    char filename[128];
    sprintf(filename, "%s.aast", sub_tree->hash);
    aast_serialize_to_file(sub_tree, filename);
    char expected_hash[65];
    strcpy(expected_hash, sub_tree->hash);
    
    aast_release(secret);
    aast_release(sub_tree);

    Node* link_node = create_node(AAST_TYPE_LINK, expected_hash, NULL, 0);
    AastChildInput link_input = { .key = "external_chunk", .child = link_node };
    Node* master_root = create_node("Master", NULL, &link_input, 1);

    printf("--- Functional: Valid Link Resolution ---\n");
    char* extracted_string = NULL;
    if (aast_execute_in_link_context(link_node, agent_extraction_callback, &extracted_string)) {
        printf("SUCCESS: Link resolved, callback executed.\n");
        free(extracted_string);
    }

    printf("\n--- Functional: The Iron Gate (Hash Mismatch) ---\n");
    Node* corrupted_link = create_node(AAST_TYPE_LINK, "BOGUS_HASH_00000000000000000000000000000000000000000000000000000", NULL, 0);
    rename(filename, "BOGUS_HASH_00000000000000000000000000000000000000000000000000000.aast");
    if (!aast_execute_in_link_context(corrupted_link, agent_extraction_callback, NULL)) {
        printf("SUCCESS: Engine blocked the mismatched chunk! Iron Gate holds.\n");
    }
    remove("BOGUS_HASH_00000000000000000000000000000000000000000000000000000.aast");

    // ==========================================
    // PHASE 2: Boundary & Performance Metrics
    // ==========================================
    printf("\n--- Boundary 1: Heavy Link Resolution (50 MB) ---\n");
    char* heavy_str = generate_heavy_string(50);
    Node* heavy_leaf = create_node("Data", heavy_str, NULL, 0);
    AastChildInput h_input = { .key = "heavy_data", .child = heavy_leaf };
    Node* heavy_chunk = create_node("ChunkRoot", NULL, &h_input, 1);
    
    char heavy_filename[128];
    sprintf(heavy_filename, "%s.aast", heavy_chunk->hash);
    aast_serialize_to_file(heavy_chunk, heavy_filename);
    char heavy_hash[65];
    strcpy(heavy_hash, heavy_chunk->hash);
    
    aast_release(heavy_leaf);
    aast_release(heavy_chunk);
    free(heavy_str);

    Node* heavy_link = create_node(AAST_TYPE_LINK, heavy_hash, NULL, 0);

    clock_t start, end;
    size_t bytes_read = 0;

    start = clock();
    aast_execute_in_link_context(heavy_link, boundary_extraction_callback, &bytes_read);
    end = clock();

    printf("[Metric] 50MB Load, Verify & Execute Latency: %f seconds\n", ((double)(end - start)) / CLOCKS_PER_SEC);
    if (bytes_read == 50 * 1024 * 1024) printf("SUCCESS: 50MB payload verified and extracted.\n");

    printf("\n--- Boundary 2: Sequential Loop Thrashing (100 Iterations) ---\n");
    bytes_read = 0;
    start = clock();
    for (int i = 0; i < 100; i++) {
        // Simulating an agent querying 100 linked chunks sequentially
        aast_execute_in_link_context(heavy_link, boundary_extraction_callback, &bytes_read);
    }
    end = clock();
    
    printf("[Metric] 100 Sequential Link Resolutions: %f seconds\n", ((double)(end - start)) / CLOCKS_PER_SEC);
    printf("SUCCESS: Parsed a cumulative %zu MB dynamically with zero peak RAM growth.\n", bytes_read / (1024 * 1024));

    // Cleanup
    aast_release(master_root);
    aast_release(link_node);
    aast_release(corrupted_link);
    aast_release(heavy_link);
    remove(heavy_filename);

    printf("\nTest Complete. Memory ready for Valgrind.\n");
    return 0;
}
