#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "aast.h"

// Generates a massive, deterministic block of "code"
char* generate_massive_payload(size_t target_mb) {
    size_t target_bytes = target_mb * 1024 * 1024;
    char* buffer = malloc(target_bytes + 1);
    if (!buffer) return NULL;

    const char* pattern = "def process_data_block(id):\n    return f'Processing block {id} with extensive string data and colons : : :'\n\n";
    size_t pat_len = strlen(pattern);
    
    char* p = buffer;
    size_t written = 0;
    while (written + pat_len < target_bytes) {
        memcpy(p, pattern, pat_len);
        p += pat_len;
        written += pat_len;
    }
    // Pad the rest with spaces and null terminate
    memset(p, ' ', target_bytes - written);
    buffer[target_bytes] = '\0';
    
    return buffer;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: %s <target_size_in_mb>\n", argv[0]);
        return 1;
    }

    size_t target_mb = (size_t)atol(argv[1]);
    if (target_mb > 512) {
        printf("Requested size %zu MB exceeds AAST_MAX_PAYLOAD_SIZE (512 MB).\n", target_mb);
        return 1;
    }

    printf("--- Payload Curvature Analysis: %zu MB ---\n", target_mb);

    // 1. Generate the raw payload
    char* raw_code = generate_massive_payload(target_mb);
    if (!raw_code) { printf("OOM: Failed to allocate %zu MB test buffer.\n", target_mb); return 1; }

    // 2. Wrap it for A-AST Ingestion
    const char* header = "root:System:\n  heavy_payload:Code:\xFF";
    const char* footer = "\xFF\n";
    
    size_t wrapped_len = strlen(header) + strlen(raw_code) + strlen(footer);
    char* wrapped_aast_text = malloc(wrapped_len + 1);
    if (!wrapped_aast_text) { printf("OOM: Failed to allocate wrapper buffer.\n"); free(raw_code); return 1; }
    
    strcpy(wrapped_aast_text, header);
    strcat(wrapped_aast_text, raw_code);
    strcat(wrapped_aast_text, footer);

    // --- BENCHMARK 1: Ingestion, Wrapping, and Hashing ---
    clock_t start = clock();
    Node* root = aast_ingest_from_text(wrapped_aast_text, NULL);
    clock_t end = clock();
    
    free(wrapped_aast_text); // Free early to keep peak RAM down

    if (!root) {
        printf("FAILED: Parser rejected the %zu MB payload.\n", target_mb);
        free(raw_code); return 1;
    }
    
    double ingest_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    // --- BENCHMARK 2: Deep Path Query ---
    start = clock();
    const char* path[] = {"heavy_payload"};
    const Node* extracted = aast_query_path(root, path, 1);
    end = clock();
    
    if (!extracted || !extracted->payload) {
        printf("FAILED: Could not query payload.\n");
        aast_release(root); free(raw_code); return 1;
    }
    double query_time = ((double)(end - start)) / CLOCKS_PER_SEC;

    // --- VERIFICATION: Fidelity Check ---
    start = clock();
    int match = (strcmp(raw_code, extracted->payload) == 0);
    end = clock();
    double verify_time = ((double)(end - start)) / CLOCKS_PER_SEC;

    if (match) {
        printf("[Metric] Ingestion & Hashing:  %f seconds\n", ingest_time);
        printf("[Metric] O(1) Query Retrieval: %f seconds\n", query_time);
        printf("[Metric] Memcmp Verification:  %f seconds\n", verify_time);
        printf("SUCCESS: %zu MB payload preserved with 100%% fidelity. Root: %s\n", target_mb, root->hash);
    } else {
        printf("FAILED: Data corruption detected in payload extraction!\n");
    }

    aast_release(root);
    free(raw_code);
    return 0;
}
