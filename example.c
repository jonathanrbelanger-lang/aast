/**
 * @file example.c
 * @brief Example usage and test harness for the A-AST library.
 *
 * This program demonstrates the full round-trip functionality of the A-AST:
 * 1. Ingests a tree from a text file.
 * 2. Serializes the in-memory tree to a binary data file.
 * 3. Releases the original tree from memory.
 * 4. Deserializes the tree from the data file into a new in-memory structure.
 * 5. Verifies the integrity and hash-soundness of the loaded tree.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "aast.h" // Include the public API of our library

/**
 * @brief Utility function to read an entire file into a heap-allocated string.
 *
 * The caller is responsible for freeing the returned buffer.
 *
 * @param filename The path to the file to read.
 * @return A new string containing the file's contents, or NULL on failure.
 */
static char* read_file_to_string(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    char* buffer = malloc(length + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }
    fread(buffer, 1, length, file);
    buffer[length] = '\0';
    fclose(file);
    return buffer;
}


int main() {
    printf("========================================\n");
    printf("A-AST Full Round-Trip Test\n");
    printf("========================================\n\n");

    // --- STEP 1: Ingest Original Tree from Text ---
    printf("--- 1. Ingesting from ingest_data.txt ---\n");
    char* file_content = read_file_to_string("ingest_data.txt");
    if (!file_content) return 1;

    Node* original_root = aast_ingest_from_text(file_content);
    free(file_content);
    if (!original_root) {
        fprintf(stderr, "Ingestion failed.\n");
        return 1;
    }

    char original_hash[65];
    strcpy(original_hash, original_root->hash);
    printf("Ingestion successful. Original Root Hash: %s\n\n", original_hash);

    // --- STEP 2: Serialize Original Tree to Data File ---
    const char* output_filename = "aast.dat";
    printf("--- 2. Serializing tree to %s ---\n", output_filename);
    if (aast_serialize_to_file(original_root, output_filename) != 0) {
        fprintf(stderr, "Serialization failed.\n");
        aast_release(original_root);
        return 1;
    }
    printf("Serialization successful.\n\n");

    // --- STEP 3: Release the Original Tree from Memory ---
    printf("--- 3. Releasing original tree from memory ---\n");
    aast_release(original_root);
    printf("Original tree released.\n\n");

    // --- STEP 4: Deserialize Tree from Data File ---
    printf("--- 4. Deserializing tree from %s ---\n", output_filename);
    Node* loaded_root = aast_deserialize_from_file(output_filename);
    if (!loaded_root) {
        fprintf(stderr, "Deserialization failed.\n");
        return 1;
    }
    printf("Deserialization successful. Loaded Root Hash: %s\n\n", loaded_root->hash);

    // --- STEP 5: Verify the Loaded Tree ---
    printf("--- 5. Verifying loaded tree ---\n");
    int hashes_match = (strcmp(original_hash, loaded_root->hash) == 0);
    printf("Hash comparison with original: %s\n", hashes_match ? "PASSED" : "FAILED");

    int integrity_ok = aast_verify_integrity(loaded_root);
    printf("Full integrity verification: %s\n", integrity_ok ? "PASSED" : "FAILED");

#ifdef DEBUG_PRINT
    aast_print_tree(loaded_root);
#endif

    // --- STEP 6: Final Cleanup ---
    printf("\n--- 6. Final Cleanup ---\n");
    aast_release(loaded_root);
    printf("Cleanup complete.\n");
    printf("========================================\n");

    if (!hashes_match || !integrity_ok) {
        return 1; // Exit with error code if verification failed
    }
    return 0;
}
