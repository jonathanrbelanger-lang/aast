#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "aast.h"

int main() {
    printf("--- Code Payload Parsing Test ---\n");

    // Construct a raw text blob containing \xFF wrappers (using \xFF in C string literal)
    const char* raw_text = 
        "root:System:\n"
        "  config:JSON:\xFF{\n"
        "  \"target\": \"x86_64\",\n"
        "  \"options\": [1, 2, 3]\n"
        "}\xFF\n"
        "  status:String:Active\n";

    printf("Ingesting wrapped multi-line text block...\n");
    
    // We pass NULL for the validator in this test just to test the parser isolation
    Node* root = aast_ingest_from_text(raw_text, NULL);
    
    if (!root) {
        printf("FAILED: Parser rejected the wrapped payload.\n");
        return 1;
    }
    
    printf("SUCCESS: Parser successfully built the tree.\n");
    
    // Validate the parsed data using our Query API!
    const Node* config_node = aast_find_child_by_key(root, "config");
    if (config_node) {
        printf("\nExtracted Opaque Payload:\n");
        printf("-------------------------\n");
        printf("%s\n", config_node->payload);
        printf("-------------------------\n");
        
        // Verify the \xFF bytes were successfully stripped
        if (strchr(config_node->payload, 0xFF) == NULL) {
            printf("SUCCESS: Out-of-band \\xFF wrappers were cleanly stripped.\n");
        } else {
            printf("FAILED: Payload still contains \\xFF wrappers.\n");
        }
    } else {
        printf("FAILED: Could not find 'config' node.\n");
    }

    aast_release(root);
    printf("\nTest Complete. Memory ready for Valgrind.\n");
    return 0;
}
