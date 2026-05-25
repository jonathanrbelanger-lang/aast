#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "aast.h"

int main() {
    printf("--- Formal .aast Filetype Test ---\n");

    // 1. Create a tree and save it
    Node* leaf = create_node("Rule", "NFC_YES", NULL, 0);
    AastChildInput input = { .key = "0x0041", .child = leaf };
    Node* root = create_node("Validator", "Root", &input, 1);

    const char* filename = "test_validator.aast";
    
    if (aast_serialize_to_file(root, filename) == 0) {
        printf("SUCCESS: Serialized to %s\n", filename);
    } else {
        printf("FAILED to serialize.\n");
        return 1;
    }
    
    // We don't need the original tree anymore
    aast_release(root);
    aast_release(leaf);

    // 2. Load it back and verify the header check worked
    Node* loaded_root = aast_deserialize_from_file(filename);
    if (loaded_root) {
        printf("SUCCESS: Deserialized tree via AAST_V1 Header!\n");
        printf("Loaded Root Hash: %s\n", loaded_root->hash);
        aast_release(loaded_root);
    } else {
        printf("FAILED to deserialize valid file.\n");
    }

    // 3. Tamper with the file (Break the Magic Header)
    FILE* fp = fopen(filename, "r+");
    fputs("BOGUS_V2", fp); // Overwrite AAST_V1
    fclose(fp);

    printf("\nAttempting to load tampered file...\n");
    Node* tampered_root = aast_deserialize_from_file(filename);
    if (!tampered_root) {
        printf("SUCCESS: Engine safely rejected the tampered file!\n");
    } else {
        printf("FAILED: Engine loaded a corrupted file!\n");
        aast_release(tampered_root);
    }

    // Cleanup disk
    remove(filename);
    printf("Test Complete. Memory ready for Valgrind.\n");
    return 0;
}
