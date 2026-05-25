#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "aast.h"

int main() {
    printf("--- UTF-8 NFC Strict Validation Test ---\n");

    // 1. Load the rulebook we generated
    const char* filename = "utf8_nfc.aast";
    Node* validator = aast_deserialize_from_file(filename);
    if (!validator) {
        printf("FAILED: Could not load %s. Did you run 'make build_ucd' first?\n", filename);
        return 1;
    }
    printf("Loaded Verifiable Rulebook. Root Hash: %s\n", validator->hash);

    // 2. Test Strings
    // A) Standard ASCII (Valid)
    const char* valid_ascii = "Agentic data is clean data.";
    
    // B) Pre-composed 'é' (U+00E9) in UTF-8 is C3 A9. (Valid NFC)
    const char* valid_composed = "Caf\xC3\xA9"; 
    
    // C) Banned NFC 'N': U+0340 (Combining Grave Tone Mark). UTF-8 is CD 80.
    const char* banned_n = "Tone\xCD\x80"; 

    // D) Banned NFC 'M': U+0300 (Combining Grave Accent). UTF-8 is CC 80.
    const char* banned_m = "Accent\xCC\x80";

    // 3. Execute Validations
    printf("\nTesting Valid Strings...\n");
    if (aast_validate_utf8_nfc(validator, valid_ascii)) printf("SUCCESS: ASCII passed.\n");
    else printf("FAILED on ASCII.\n");

    if (aast_validate_utf8_nfc(validator, valid_composed)) printf("SUCCESS: Pre-composed 'é' passed.\n");
    else printf("FAILED on Pre-composed.\n");

    printf("\nTesting Invalid Strings (Strict Hygiene Boundary)...\n");
    if (!aast_validate_utf8_nfc(validator, banned_n)) printf("SUCCESS: Explicitly rejected 'N' rule (U+0340).\n");
    else printf("FAILED: Allowed banned 'N' character.\n");

    if (!aast_validate_utf8_nfc(validator, banned_m)) printf("SUCCESS: Explicitly rejected 'M' rule (U+0300).\n");
    else printf("FAILED: Allowed banned 'M' character.\n");

    // Cleanup
    aast_release(validator);
    printf("\nTest Complete. Memory ready for Valgrind.\n");
    return 0;
}
