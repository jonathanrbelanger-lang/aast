#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "aast.h"

// Helper to read entire file into memory
char* read_file(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buffer = malloc(length + 1);
    if (buffer) {
        fread(buffer, 1, length, f);
        buffer[length] = '\0';
    }
    fclose(f);
    return buffer;
}

// Helper to write string to file
int write_file(const char* filename, const char* data) {
    FILE* f = fopen(filename, "wb");
    if (!f) return 0;
    size_t len = strlen(data);
    size_t written = fwrite(data, 1, len, f);
    fclose(f);
    return written == len;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        printf("Usage: %s <input_file> <output_file>\n", argv[0]);
        return 1;
    }

    const char* input_file = argv[1];
    const char* output_file = argv[2];
    clock_t start, end;
    double time_taken;

    printf("--- Blind Code Benchmark Test ---\n");
    printf("Target File: %s\n", input_file);

    char* raw_code = read_file(input_file);
    if (!raw_code) { printf("FAILED: Could not read input file.\n"); return 1; }

    const char* header = "root:File:\n  payload:Code:\xFF";
    const char* footer = "\xFF\n";
    size_t wrapped_len = strlen(header) + strlen(raw_code) + strlen(footer);
    char* wrapped_aast_text = malloc(wrapped_len + 1);
    strcpy(wrapped_aast_text, header);
    strcat(wrapped_aast_text, raw_code);
    strcat(wrapped_aast_text, footer);

    // --- BENCHMARK 1: Ingestion & Hashing ---
    start = clock();
    Node* root = aast_ingest_from_text(wrapped_aast_text, NULL);
    end = clock();
    
    free(raw_code);
    free(wrapped_aast_text);

    if (!root) { printf("FAILED: Parser rejected the payload.\n"); return 1; }
    
    time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("\n[Metric] Ingestion, Parsing, & Merkle Hashing: %f seconds\n", time_taken);
    printf("         Root Hash: %s\n", root->hash);

    // --- BENCHMARK 2: Deep Path Query ---
    start = clock();
    const char* path[] = {"payload"};
    const Node* extracted_node = aast_query_path(root, path, 1);
    end = clock();
    
    if (!extracted_node || !extracted_node->payload) {
        printf("FAILED: Could not query payload.\n");
        aast_release(root); return 1;
    }
    
    time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("[Metric] O(1) Payload Query & Retrieval:       %f seconds\n", time_taken);

    // --- BENCHMARK 3: Extraction to Disk ---
    start = clock();
    int write_success = write_file(output_file, extracted_node->payload);
    end = clock();

    if (!write_success) {
        printf("FAILED: Could not write output file.\n");
        aast_release(root); return 1;
    }

    time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("[Metric] Extraction & Disk Write IO:           %f seconds\n\n", time_taken);

    printf("SUCCESS: Benchmark complete. Run 'sha256sum' to verify.\n");
    
    aast_release(root);
    return 0;
}
