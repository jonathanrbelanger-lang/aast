#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "aast.h"

// The Agent's callback function
void agent_extraction_callback(const Node* loaded_root, void* context) {
    const Node* target = aast_find_child_by_key(loaded_root, "secret_data");
    if (target && target->payload) {
        // Agent copies the data it needs into its context
        char** out_buffer = (char**)context;
        *out_buffer = strdup(target->payload);
        printf("  [Agent Callback] Successfully extracted payload: '%s'\n", target->payload);
    } else {
        printf("  [Agent Callback] Target node not found in loaded chunk.\n");
    }
}

int main() {
    printf("--- AAST_LINK Cross-File Integrity Test ---\n");

    // 1. Create the Sub-Tree (The Chunk)
    Node* secret = create_node("Data", "Agent_Alpha_Clearance", NULL, 0);
    AastChildInput input = { .key = "secret_data", .child = secret };
    Node* sub_tree = create_node("ChunkRoot", NULL, &input, 1);
    
    // Save the chunk to disk using its hash as the filename
    char filename[128];
    sprintf(filename, "%s.aast", sub_tree->hash);
    aast_serialize_to_file(sub_tree, filename);
    
    char expected_hash[65];
    strcpy(expected_hash, sub_tree->hash);
    
    aast_release(secret);
    aast_release(sub_tree); // Flush it from RAM. It only exists on disk now.

    // 2. Create the Parent Tree (Holding the Link)
    Node* link_node = create_node(AAST_TYPE_LINK, expected_hash, NULL, 0);
    AastChildInput link_input = { .key = "external_chunk", .child = link_node };
    Node* master_root = create_node("Master", NULL, &link_input, 1);

    // 3. Test 1: Successful Link Resolution
    printf("\nTesting Valid Link Resolution...\n");
    char* extracted_string = NULL;
    int success = aast_execute_in_link_context(link_node, agent_extraction_callback, &extracted_string);
    
    if (success && extracted_string && strcmp(extracted_string, "Agent_Alpha_Clearance") == 0) {
        printf("SUCCESS: Link resolved, callback executed, and memory auto-flushed.\n");
        free(extracted_string); // Agent frees its own copy
    } else {
        printf("FAILED valid link resolution.\n");
        if (extracted_string) free(extracted_string);
    }

    // 4. Test 2: The Iron Gate (Tampering)
    printf("\nTesting The Iron Gate (Hash Mismatch Rejection)...\n");
    // We modify the Master Tree so the link points to a fake hash, but the file on disk remains the same.
    // This simulates the file on disk being silently swapped for a corrupted version relative to the commit.
    Node* corrupted_link = create_node(AAST_TYPE_LINK, "BOGUS_HASH_00000000000000000000000000000000000000000000000000000", NULL, 0);
    
    // Rename the valid file to match the bogus hash so the loader finds it, but the contents are wrong
    rename(filename, "BOGUS_HASH_00000000000000000000000000000000000000000000000000000.aast");

    int rejected = !aast_execute_in_link_context(corrupted_link, agent_extraction_callback, NULL);
    if (rejected) {
        printf("SUCCESS: Engine safely blocked the mismatched chunk! Iron Gate holds.\n");
    } else {
        printf("FAILED: Engine allowed a corrupted chunk through the boundary!\n");
    }

    // Cleanup
    aast_release(master_root);
    aast_release(link_node);
    aast_release(corrupted_link);
    remove("BOGUS_HASH_00000000000000000000000000000000000000000000000000000.aast");

    printf("\nTest Complete. Memory ready for Valgrind.\n");
    return 0;
}
