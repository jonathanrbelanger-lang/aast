#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

    printf("--- Blind Code Round-Trip Test ---\n");
    printf("Target File: %s\n", input_file);

    // 1. Read the blind file
    char* raw_code = read_file(input_file);
    if (!raw_code) {
        printf("FAILED: Could not read input file.\n");
        return 1;
    }

    // 2. Package it into A-AST format with 0xFF Opaque Wrappers
    // Format: 
    // root:File:
    //   payload:Code:\xFF[RAW_CODE]\xFF\n
    
    const char* header = "root:File:\n  payload:Code:\xFF";
    const char* footer = "\xFF\n";
    
    size_t wrapped_len = strlen(header) + strlen(raw_code) + strlen(footer);
    char* wrapped_aast_text = malloc(wrapped_len + 1);
    
    strcpy(wrapped_aast_text, header);
    strcat(wrapped_aast_text, raw_code);
    strcat(wrapped_aast_text, footer);

    // 3. Ingest the wrapped text into the A-AST Engine
    // (Passing NULL for validator to isolate the parsing mechanism test)
    printf("Ingesting into A-AST engine...\n");
    Node* root = aast_ingest_from_text(wrapped_aast_text, NULL);
    
    free(raw_code);
    free(wrapped_aast_text);

    if (!root) {
        printf("FAILED: A-AST Parser rejected the wrapped payload.\n");
        return 1;
    }
    printf("SUCCESS: A-AST instantiated. Root Hash: %s\n", root->hash);

    // 4. Query the engine to extract the payload
    const char* path[] = {"payload"};
    const Node* extracted_node = aast_query_path(root, path, 1);
    
    if (!extracted_node || !extracted_node->payload) {
        printf("FAILED: Could not query payload from A-AST.\n");
        aast_release(root);
        return 1;
    }

    // 5. Write the extracted payload back to disk
    printf("Extracting payload to: %s\n", output_file);
    if (!write_file(output_file, extracted_node->payload)) {
        printf("FAILED: Could not write output file.\n");
        aast_release(root);
        return 1;
    }

    printf("SUCCESS: Round-trip complete. Run 'diff' to verify absolute fidelity.\n");
    
    aast_release(root);
    return 0;
}
