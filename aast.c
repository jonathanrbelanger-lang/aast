#define _GNU_SOURCE // Enable POSIX extensions like strdup, strsep, and getline

#include "aast.h" // Must be the first include

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/evp.h>
#include "uthash.h"

// ----------------------------------------------------------------------------
// Internal Constants and Definitions
// ----------------------------------------------------------------------------

#define AAST_MAX_DEPTH 150000 // Safe recursion limit based on empirical tests.

// --- Structs for Ingestion Engine ---
typedef struct TempNode {
    char* key;
    char* type;
    char* payload;
    struct TempNode* parent;
    struct TempNode** children;
    size_t child_count;
    size_t child_capacity;
} TempNode;

// --- Structs for Persistence API ---
typedef struct VisitedNode {
    char hash[65];
    UT_hash_handle hh;
} VisitedNode;

typedef struct NodeMapEntry {
    char hash[65];
    Node* node;
    UT_hash_handle hh;
} NodeMapEntry;


// ----------------------------------------------------------------------------
// Static Forward Declarations for Internal Helper Functions
// ----------------------------------------------------------------------------

static void compute_sha256_hex(const char* data, char outputBuffer[65]);
static char* generate_canonical_buffer(const Node* node);
static void aast_release_recursive(Node* node, int current_depth);
static int aast_verify_integrity_recursive(const Node* root, int current_depth);
static Node* accrete_recursive_helper(const Node* current_node, const char* const* path, size_t path_index, size_t path_len, const char* new_payload);
static Node* convert_temp_to_aast(TempNode* t_node);
#ifdef DEBUG_PRINT
static void aast_print_tree_recursive(const Node* node, int indent_level);
#endif

// ----------------------------------------------------------------------------
// Cryptographic & Serialization Primitives (Internal)
// ----------------------------------------------------------------------------

static void compute_sha256_hex(const char* data, char outputBuffer[65]) {
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    const EVP_MD *md = EVP_sha256();
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int md_len;
    EVP_DigestInit_ex(mdctx, md, NULL);
    EVP_DigestUpdate(mdctx, data, strlen(data));
    EVP_DigestFinal_ex(mdctx, hash, &md_len);
    EVP_MD_CTX_free(mdctx);
    for(unsigned int i = 0; i < md_len; i++) {
        sprintf(outputBuffer + (i * 2), "%02x", hash[i]);
    }
    outputBuffer[64] = '\0';
}

static char* generate_canonical_buffer(const Node* node) {
    if (node == NULL) return NULL;
    size_t type_len = strlen(node->type);
    size_t key_len = (node->key) ? strlen(node->key) : 0;
    size_t payload_len = (node->payload) ? strlen(node->payload) : 0;
    char* hashes_str = NULL;
    if (node->child_count > 0) {
        size_t hashes_str_len = (node->child_count * 64) + (node->child_count - 1);
        hashes_str = (char*)malloc(hashes_str_len + 1);
        if (hashes_str == NULL) return NULL;
        char* p = hashes_str;
        for (size_t i = 0; i < node->child_count; i++) {
            memcpy(p, node->children[i]->hash, 64);
            p += 64;
            if (i < node->child_count - 1) { *p = ','; p++; }
        }
        *p = '\0';
    } else {
        hashes_str = "";
    }
    int required_size = snprintf(NULL, 0, "%zu:%s|%zu:%s|%zu:%s|%zu:%s",
        type_len, node->type, key_len, node->key ? node->key : "",
        node->child_count, hashes_str, payload_len, node->payload ? node->payload : "");
    if (required_size < 0) {
        if (node->child_count > 0) free(hashes_str);
        return NULL;
    }
    char* buffer = (char*)malloc(required_size + 1);
    if (buffer == NULL) {
        if (node->child_count > 0) free(hashes_str);
        return NULL;
    }
    sprintf(buffer, "%zu:%s|%zu:%s|%zu:%s|%zu:%s",
        type_len, node->type, key_len, node->key ? node->key : "",
        node->child_count, hashes_str, payload_len, node->payload ? node->payload : "");
    if (node->child_count > 0) free(hashes_str);
    return buffer;
}


// ----------------------------------------------------------------------------
// Public API Implementations
// ----------------------------------------------------------------------------

Node* create_node(const char* type, const char* key, const char* payload, Node** children, size_t child_count) {
    Node* new_node = (Node*)malloc(sizeof(Node));
    if (new_node == NULL) return NULL;

    new_node->key = NULL;
    new_node->payload = NULL;
    new_node->children = NULL;
    new_node->ref_count = 1;
    strncpy(new_node->type, type, 15);
    new_node->type[15] = '\0';
    new_node->child_count = child_count;

    if (key != NULL) {
        new_node->key = strdup(key);
        if (new_node->key == NULL) { free(new_node); return NULL; }
    }
    if (payload != NULL) {
        new_node->payload = strdup(payload);
        if (new_node->payload == NULL) { free(new_node->key); free(new_node); return NULL; }
    }
    if (child_count > 0 && children != NULL) {
        new_node->children = (Node**)malloc(child_count * sizeof(Node*));
        if (new_node->children == NULL) { free(new_node->key); free(new_node->payload); free(new_node); return NULL; }
        memcpy(new_node->children, children, child_count * sizeof(Node*));
    }

    char* canonical_buffer = generate_canonical_buffer(new_node);
    if (canonical_buffer == NULL) {
        free(new_node->children); free(new_node->key); free(new_node->payload); free(new_node);
        return NULL;
    }
    compute_sha256_hex(canonical_buffer, new_node->hash);
    free(canonical_buffer);
    return new_node;
}

void aast_release(Node* node) {
    aast_release_recursive(node, 0);
}

int aast_retain(Node* node) {
    if (node == NULL) return 0;
    if (node->ref_count == SIZE_MAX) return -1;
    node->ref_count++;
    return 0;
}

Node* accrete_new_state(const Node* root, const char* const* path, size_t path_len, const char* new_payload) {
    if (root == NULL || path == NULL || path_len == 0) return NULL;
    return accrete_recursive_helper(root, path, 0, path_len, new_payload);
}

int aast_verify_integrity(const Node* root) {
    return aast_verify_integrity_recursive(root, 0);
}

// ----------------------------------------------------------------------------
// Internal Helper Implementations
// ----------------------------------------------------------------------------

static void aast_release_recursive(Node* node, int current_depth) {
    if (node == NULL) return;
    if (current_depth >= AAST_MAX_DEPTH) {
        fprintf(stderr, "WARNING: A-AST maximum recursion depth reached during release. Halting traversal to prevent stack overflow. This will result in a memory leak for this branch.\n");
        return;
    }
    node->ref_count--;
    if (node->ref_count == 0) {
        if (node->child_count > 0 && node->children != NULL) {
            for (size_t i = 0; i < node->child_count; i++) {
                aast_release_recursive(node->children[i], current_depth + 1);
            }
            free(node->children);
        }
        if (node->key != NULL) free(node->key);
        if (node->payload != NULL) free(node->payload);
        free(node);
    }
}

static int aast_verify_integrity_recursive(const Node* root, int current_depth) {
    if (root == NULL) return 1;
    if (current_depth >= AAST_MAX_DEPTH) {
        fprintf(stderr, "ERROR: A-AST maximum recursion depth reached during verification. Halting traversal to prevent stack overflow. Integrity cannot be confirmed.\n");
        return 0;
    }
    for (size_t i = 0; i < root->child_count; i++) {
        if (aast_verify_integrity_recursive(root->children[i], current_depth + 1) == 0) {
            return 0;
        }
    }
    char* canonical_buffer = generate_canonical_buffer(root);
    if (canonical_buffer == NULL) return 0;
    char fresh_hash[65];
    compute_sha256_hex(canonical_buffer, fresh_hash);
    free(canonical_buffer);
    return (strcmp(root->hash, fresh_hash) == 0);
}

static Node* accrete_recursive_helper(const Node* current_node, const char* const* path, size_t path_index, size_t path_len, const char* new_payload) {
    if (current_node == NULL || current_node->key == NULL || strcmp(current_node->key, path[path_index]) != 0) return NULL;
    if (path_index == path_len - 1) return create_node(current_node->type, current_node->key, new_payload, current_node->children, current_node->child_count);
    size_t target_child_index = (size_t)-1;
    for (size_t i = 0; i < current_node->child_count; i++) {
        if (current_node->children[i] != NULL && current_node->children[i]->key != NULL && strcmp(current_node->children[i]->key, path[path_index + 1]) == 0) {
            target_child_index = i; break;
        }
    }
    if (target_child_index == (size_t)-1) return NULL;
    Node* new_child_node = accrete_recursive_helper(current_node->children[target_child_index], path, path_index + 1, path_len, new_payload);
    if (new_child_node == NULL) return NULL;
    Node** new_children_list = (Node**)malloc(current_node->child_count * sizeof(Node*));
    if (new_children_list == NULL) { aast_release(new_child_node); return NULL; }
    memcpy(new_children_list, current_node->children, current_node->child_count * sizeof(Node*));
    new_children_list[target_child_index] = new_child_node;
    for (size_t i = 0; i < current_node->child_count; i++) {
        if (i != target_child_index) {
            if (aast_retain(new_children_list[i]) != 0) {
                aast_release(new_child_node);
                for (size_t j = 0; j < i; j++) { if (j != target_child_index) aast_release(new_children_list[j]); }
                free(new_children_list); return NULL;
            }
        }
    }
    Node* new_parent_node = create_node(current_node->type, current_node->key, current_node->payload, new_children_list, current_node->child_count);
    if (new_parent_node == NULL) {
        for (size_t i = 0; i < current_node->child_count; i++) aast_release(new_children_list[i]);
    }
    free(new_children_list);
    return new_parent_node;
}


// --- Functions from here down were previously in main.c and are now part of the library implementation ---

// --- Ingestion Engine ---
static int add_child_to_temp_node(TempNode* parent, TempNode* child) {
    if (parent->child_count >= parent->child_capacity) {
        size_t new_capacity = (parent->child_capacity == 0) ? 8 : parent->child_capacity * 2;
        void* new_children = realloc(parent->children, new_capacity * sizeof(TempNode*));
        if (new_children == NULL) return -1;
        parent->children = new_children;
        parent->child_capacity = new_capacity;
    }
    parent->children[parent->child_count++] = child;
    child->parent = parent;
    return 0;
}

static void free_temp_tree(TempNode* t_node) {
    if (t_node == NULL) return;
    for (size_t i = 0; i < t_node->child_count; i++) free_temp_tree(t_node->children[i]);
    free(t_node->key); free(t_node->type); free(t_node->payload); free(t_node->children); free(t_node);
}

static Node* convert_temp_to_aast(TempNode* t_node) {
    if (t_node == NULL) return NULL;
    Node** children_nodes = NULL;
    if (t_node->child_count > 0) {
        children_nodes = malloc(t_node->child_count * sizeof(Node*));
        if (children_nodes == NULL) return NULL;
        for (size_t i = 0; i < t_node->child_count; i++) {
            children_nodes[i] = convert_temp_to_aast(t_node->children[i]);
            if (children_nodes[i] == NULL) {
                for (size_t j = 0; j < i; j++) aast_release(children_nodes[j]);
                free(children_nodes); return NULL;
            }
        }
    }
    Node* new_node = create_node(t_node->type, t_node->key, t_node->payload, children_nodes, t_node->child_count);
     if (children_nodes != NULL) {
        free(children_nodes);
    }
    return new_node;
}

Node* aast_ingest_from_text(const char* text_data) {
    TempNode* temp_root = calloc(1, sizeof(TempNode));
    if (!temp_root) return NULL;
    TempNode* current_parent = temp_root;
    int prev_indent = -2;
    char* text_copy = strdup(text_data);
    if(!text_copy) { free(temp_root); return NULL; }
    char* to_free = text_copy;
    char* line;
    while ((line = strsep(&text_copy, "\n")) != NULL) {
        char* line_start = line;
        while (*line_start == ' ') line_start++;
        if (*line_start == '\0') continue; // Skip empty lines
        int current_indent = line_start - line;
        if (current_indent % 2 != 0) { free_temp_tree(temp_root); free(to_free); return NULL; }
        if (current_indent > prev_indent + 2) { free_temp_tree(temp_root); free(to_free); return NULL; }
        while (current_indent <= prev_indent) {
            current_parent = current_parent->parent;
            prev_indent -= 2;
        }
        char* key = strsep(&line_start, ":");
        char* type = strsep(&line_start, ":");
        char* payload = line_start;
        if (!key || !type) { free_temp_tree(temp_root); free(to_free); return NULL; }
        TempNode* new_temp_node = calloc(1, sizeof(TempNode));
        if(!new_temp_node) { free_temp_tree(temp_root); free(to_free); return NULL; }
        new_temp_node->key = strdup(key);
        new_temp_node->type = strdup(type);
        if (payload) new_temp_node->payload = strdup(payload);
        if(!new_temp_node->key || !new_temp_node->type || (payload && !new_temp_node->payload)) {
             free_temp_tree(new_temp_node); free_temp_tree(temp_root); free(to_free); return NULL;
        }
        add_child_to_temp_node(current_parent, new_temp_node);
        current_parent = new_temp_node;
        prev_indent = current_indent;
    }
    free(to_free);
    Node* final_root = NULL;
    if (temp_root->child_count == 1) {
        final_root = convert_temp_to_aast(temp_root->children[0]);
    }
    free_temp_tree(temp_root);
    return final_root;
}

// --- Persistence API ---
static int serialize_recursive_helper(const Node* node, FILE* fp, VisitedNode** visited_set) {
    if (node == NULL) return 0;
    VisitedNode* found;
    HASH_FIND_STR(*visited_set, node->hash, found);
    if (found) return 0;
    VisitedNode* new_visited = malloc(sizeof(VisitedNode));
    if (!new_visited) return -1;
    strcpy(new_visited->hash, node->hash);
    HASH_ADD_STR(*visited_set, hash, new_visited);
    for (size_t i = 0; i < node->child_count; i++) {
        if (serialize_recursive_helper(node->children[i], fp, visited_set) != 0) return -1;
    }
    fprintf(fp, "%s|%s|%zu:%s|%zu:%s|", node->hash, node->type,
        node->key ? strlen(node->key) : 0, node->key ? node->key : "",
        node->payload ? strlen(node->payload) : 0, node->payload ? node->payload : "");
    for (size_t i = 0; i < node->child_count; i++) {
        fprintf(fp, "%s%s", node->children[i]->hash, (i < node->child_count - 1) ? "," : "");
    }
    fprintf(fp, "\n");
    return 0;
}

int aast_serialize_to_file(const Node* root, const char* filename) {
    if (!root || !filename) return -1;
    FILE* fp = fopen(filename, "w");
    if (!fp) { perror("Failed to open file for serialization"); return -1; }
    VisitedNode* visited_set = NULL;
    int result = serialize_recursive_helper(root, fp, &visited_set);
    fclose(fp);
    VisitedNode *current_node, *tmp;
    HASH_ITER(hh, visited_set, current_node, tmp) {
        HASH_DEL(visited_set, current_node);
        free(current_node);
    }
    if (result != 0) { fprintf(stderr, "Serialization failed.\n"); return -1; }
    return 0;
}

Node* aast_deserialize_from_file(const char* filename) {
    FILE* fp = fopen(filename, "r");
    if (!fp) { perror("Failed to open file for deserialization"); return NULL; }
    NodeMapEntry* node_map = NULL;
    Node* root = NULL;
    char* line = NULL;
    size_t len = 0;
    ssize_t read;
    int error = 0;
    while ((read = getline(&line, &len, fp)) != -1) {
        char* original_line = strdup(line);
        if(!original_line) { error = 1; break; }
        char* line_ptr = line;
        char* stored_hash = strsep(&line_ptr, "|");
        char* type = strsep(&line_ptr, "|");
        char* key_field = strsep(&line_ptr, "|");
        char* payload_field = strsep(&line_ptr, "|");
        char* child_hashes_str = strsep(&line_ptr, "\n");
        if (!stored_hash || !type || !key_field || !payload_field || !child_hashes_str) { free(original_line); error = 1; break; }
        char* key = strchr(key_field, ':') + 1;
        char* payload = strchr(payload_field, ':') + 1;
        Node** children = NULL;
        size_t child_count = 0;
        if (strlen(child_hashes_str) > 0) {
            char* hash_token = strtok(child_hashes_str, ",");
            while (hash_token) {
                NodeMapEntry* found_entry;
                HASH_FIND_STR(node_map, hash_token, found_entry);
                if (!found_entry) { error = 1; break; }
                void* new_children = realloc(children, (child_count + 1) * sizeof(Node*));
                if(!new_children) { error=1; break; }
                children = new_children;
                children[child_count++] = found_entry->node;
                hash_token = strtok(NULL, ",");
            }
        }
        if (error) { free(children); free(original_line); break; }
        Node* new_node = create_node(type, key[0] ? key : NULL, payload[0] ? payload : NULL, children, child_count);
        if (!new_node) { error = 1; free(children); free(original_line); break; }
        if (strcmp(new_node->hash, stored_hash) != 0) {
            fprintf(stderr, "ERROR: Hash mismatch on deserialization! File is corrupt.\n");
            aast_release(new_node); error = 1; free(children); free(original_line); break;
        }
        NodeMapEntry* new_entry = malloc(sizeof(NodeMapEntry));
        if(!new_entry) { aast_release(new_node); error = 1; free(children); free(original_line); break; }
        strcpy(new_entry->hash, new_node->hash);
        new_entry->node = new_node;
        HASH_ADD_STR(node_map, hash, new_entry);
        root = new_node;
        free(children);
        free(original_line);
    }
    if (error) {
        if(line) free(line);
        aast_release(root); root = NULL;
    }
    NodeMapEntry *current_entry, *tmp;
    HASH_ITER(hh, node_map, current_entry, tmp) {
        HASH_DEL(node_map, current_entry);
        free(current_entry);
    }
    fclose(fp);
    if(line) free(line);
    return root;
}


#ifdef DEBUG_PRINT
static void aast_print_tree_recursive(const Node* node, int indent_level) {
    if (node == NULL) return;
    for (int i = 0; i < indent_level; ++i) printf("  ");
    printf("- Key: %-20s | Type: %-10s | Payload: %-25.25s | Hash: %.8s... | Refs: %zu\n",
           node->key ? node->key : "NULL", node->type,
           node->payload ? node->payload : "NULL", node->hash, node->ref_count);
    for (size_t i = 0; i < node->child_count; ++i) {
        aast_print_tree_recursive(node->children[i], indent_level + 1);
    }
}
void aast_print_tree(const Node* root) {
    if (root == NULL) { printf("A-AST is NULL.\n"); return; }
    printf("--- A-AST Tree View ---\n");
    aast_print_tree_recursive(root, 0);
    printf("-----------------------\n");
}
#endif // DEBUG_PRINT
