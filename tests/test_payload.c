#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "aast.h"

int main() {
    printf("--- Code Payload Parsing Test (Directional Markers) ---\n");

    // Construct a raw text blob containing \xC0\xC1\xFF (Start) and \xFF\xC1\xC0 (End)
    const char* raw_text = 
        "root:System:\n"
        "  config:JSON:\xC0\xC1\xFF{\n"
        "  \"target\": \"x86_64\",\n"
        "  \"options\": [1, 2, 3]\n"
        "}\xFF\xC1\xC0\n"
        "  status:String:Active\n";

    printf("Ingesting wrapped multi-line text block...\n");
    
    // We pass NULL for the validator to isolate the parsing mechanism test
    Node* root = aast_ingest_from_text(raw_text, NULL);
    
    if (!root) {
        printf("FAILED: Parser rejected the wrapped payload.\n");
        return 1;
    }
    
    printf("SUCCESS: Parser successfully built the tree.\n");
    
    // Validate the parsed data using our Query API
    const Node* config_node = aast_find_child_by_key(root, "config");
    if (config_node) {
        printf("\nExtracted Opaque Payload:\n");
        printf("-------------------------\n");
        printf("%s\n", config_node->payload);
        printf("-------------------------\n");
        
        // Verify the 3-byte markers were successfully stripped
        if (strstr(config_node->payload, "\xC0\xC1\xFF") == NULL && 
            strstr(config_node->payload, "\xFF\xC1\xC0") == NULL) {
            printf("SUCCESS: Out-of-band directional markers were cleanly stripped.\n");
        } else {
            printf("FAILED: Payload still contains transport markers.\n");
        }
    } else {
        printf("FAILED: Could not find 'config' node.\n");
    }

    aast_release(root);
    printf("\nTest Complete. Memory ready for Valgrind.\n");
    return 0;
}
