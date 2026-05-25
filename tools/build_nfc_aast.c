#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "aast.h"

// --- Temporary Mutable Trie for Building ---
typedef struct TrieNode {
    char key[8];
    char payload[4];
    struct TrieNode** children;
    size_t child_count;
    size_t child_cap;
} TrieNode;

TrieNode* create_trie_node(const char* key) {
    TrieNode* node = calloc(1, sizeof(TrieNode));
    if (key) strcpy(node->key, key);
    return node;
}

void add_path(TrieNode* current, char** hex_tokens, size_t len, const char* payload) {
    for (size_t i = 0; i < len; i++) {
        char* token = hex_tokens[i];
        TrieNode* next = NULL;
        
        // Find existing child
        for (size_t j = 0; j < current->child_count; j++) {
            if (strcmp(current->children[j]->key, token) == 0) {
                next = current->children[j];
                break;
            }
        }
        
        // Create if missing
        if (!next) {
            next = create_trie_node(token);
            if (current->child_count == current->child_cap) {
                current->child_cap = current->child_cap == 0 ? 4 : current->child_cap * 2;
                current->children = realloc(current->children, current->child_cap * sizeof(TrieNode*));
            }
            current->children[current->child_count++] = next;
        }
        current = next;
    }
    strcpy(current->payload, payload);
}

void free_trie(TrieNode* node) {
    if (!node) return;
    for (size_t i = 0; i < node->child_count; i++) {
        free_trie(node->children[i]);
    }
    free(node->children);
    free(node);
}

// --- Convert Mutable Trie to Immutable A-AST ---
Node* convert_to_aast(TrieNode* t) {
    AastChildInput* inputs = NULL;
    if (t->child_count > 0) {
        inputs = malloc(t->child_count * sizeof(AastChildInput));
        for (size_t i = 0; i < t->child_count; i++) {
            inputs[i].key = t->children[i]->key;
            // Recursively build children first (Bottom-Up)
            inputs[i].child = convert_to_aast(t->children[i]); 
        }
    }
    
    // Create the immutable A-AST node
    Node* n = create_node(
        t->child_count == 0 ? "Rule" : "Byte", 
        t->payload[0] != '\0' ? t->payload : NULL, 
        inputs, 
        t->child_count
    );
    
    // Release our temporary hold on the children (parent now owns them)
    if (inputs) {
        for (size_t i = 0; i < t->child_count; i++) aast_release(inputs[i].child);
        free(inputs);
    }
    return n;
}

int main() {
    printf("Starting NFC A-AST Compiler...\n");
    FILE* fp = fopen("nfc_rules.txt", "r");
    if (!fp) { perror("Could not open nfc_rules.txt"); return 1; }

    TrieNode* temp_root = create_trie_node(NULL);

    char* line = NULL;
    size_t len = 0;
    while (getline(&line, &len, fp) != -1) {
        char* current = line;
        char* hex_part = strsep(&current, ":");
        char* val_part = current;
        if (!hex_part || !val_part) continue;

        // Clean strings
        val_part[strcspn(val_part, "\n")] = 0;
        while(*val_part == ' ') val_part++;
        
        char* tokens[4]; // Max 4 bytes in UTF-8
        size_t token_count = 0;
        char* token;
        while ((token = strsep(&hex_part, " ")) != NULL) {
            if (strlen(token) > 0) tokens[token_count++] = token;
        }

        add_path(temp_root, tokens, token_count, val_part);
    }
    free(line);
    fclose(fp);

    printf("Parsed text file into mutable Trie. Converting to Immutable A-AST...\n");
    Node* final_aast = convert_to_aast(temp_root);
    free_trie(temp_root);

    if (!final_aast) { printf("Failed to build A-AST.\n"); return 1; }

    printf("Generated A-AST Root Hash: %s\n", final_aast->hash);
    printf("Serializing to utf8_nfc.aast...\n");
    
    if (aast_serialize_to_file(final_aast, "utf8_nfc.aast") == 0) {
        printf("SUCCESS! Verifiable ruleset saved to utf8_nfc.aast\n");
    }

    aast_release(final_aast);
    return 0;
}
