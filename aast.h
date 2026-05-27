#ifndef AAST_H
#define AAST_H
#include <stddef.h> // For size_t
#include "uthash.h"

// ----------------------------------------------------------------------------
// Architectural Operational Constraints (Empirically Mapped)
// ----------------------------------------------------------------------------
#define AAST_MAX_KEY_LEN     256   // Prevents O(N) string hashing degradation
#define AAST_MAX_TYPE_LEN    16    // Standard fixed-width semantic tag limit
#define AAST_MAX_PAYLOAD_SIZE    536870912   // 512 MB chunking threshold for heap stability

// --- Reserved Structural Types ---
#define AAST_TYPE_LINK "AAST_LINK" // Denotes a node whose payload is a hash pointing to another .aast file

// ----------------------------------------------------------------------------
// Public Data Structure Definition
// ----------------------------------------------------------------------------

// Forward declare the Node struct so it can be used in other struct definitions.
struct Node;

/**
 * @brief Represents a key-value entry in the children hash table of a Node.
 */
typedef struct ChildEntry {
    char* key;
    struct Node* child_node; // Use 'struct Node*' here
    UT_hash_handle hh;
} ChildEntry;

/**
 * @brief A structure for providing child information to the create_node function.
 */
typedef struct {
    const char* key;
    struct Node* child; // And use 'struct Node*' here
} AastChildInput;

/**
 * @brief The core data structure for the Accretive-Abstract-State-Tree.
 */
typedef struct Node {
    char type[16];
    char *payload;
    ChildEntry *children;
    size_t child_count;
    size_t ref_count;
    char hash[65];
} Node;
// ----------------------------------------------------------------------------
// Public API Function Prototypes
// ----------------------------------------------------------------------------
/**
 * @brief Creates a new, immutable A-AST node.
 *
 * This is the primary constructor. It performs a deep copy of the payload,
 * creates a hash table for its children, and computes the final SHA-256 hash.
 * On success, the returned node has a reference count of 1. The children
 * provided will be retained (their ref_count will be incremented).
 *
 * @param type The semantic type for the node.
 * @param payload The string data for the node.
 * @param children_input An array of AastChildInput structs.
 * @param child_count The number of children.
 ** @return A pointer to the newly created Node, or NULL on allocation failure.
 */
Node* create_node(const char* type, const char* payload, const AastChildInput* children_input, size_t child_count);
const Node* aast_find_child_by_key(const Node* parent, const char* key);
const Node* aast_query_path(const Node* root, const char* const* path, size_t path_len);
typedef void (*AastChildCallback)(const char* key, const Node* child, void* context);
void aast_iterate_children(const Node* parent, AastChildCallback callback, void* context);
/**
 * @brief Validates a string against a loaded UTF-8 NFC Validator A-AST.
 * 
 * Strictly rejects any characters flagged as "N" (No) or "M" (Maybe) in the 
 * Unicode Consortium database, enforcing a mathematically perfect hygiene boundary.
 * 
 * @param validator_root The root node of the utf8_nfc.aast tree.
 * @param text The null-terminated string to validate.
 * @return 1 if the string is perfectly valid UTF-8 NFC, 0 if it is invalid.
 */
int aast_validate_utf8_nfc(const Node* validator_root, const char* text);

/**
 * @brief O(1) query to find a child node by its string key.
 *
 * @param parent The parent node to search within.
 * @param key The exact string key to look for.
 * @return A non-owning (weak) const pointer to the child node, or NULL if not found.
 * @warning Do NOT call aast_release() on the returned pointer. Lifecycle is managed by the tree.
 */
const Node* aast_query_path(const Node* root, const char* const* path, size_t path_len);

/**
 * @brief Function pointer type for iterating over child nodes.
 * @param key The string key of the current child.
 * @param child The child node itself (non-owning pointer).
 * @param context Opaque user data passed through from aast_iterate_children.
 */
typedef void (*AastChildCallback)(const char* key, const Node* child, void* context);

/**
 * @brief Iterates over all children of a node, executing a callback for each.
 * 
 * This function performs ZERO memory allocations, making it mathematically 
 * immune to memory leaks. It is the architectural standard for discovery.
 * 
 * @param parent The node whose children will be iterated.
 * @param callback The function to execute for each child.
 * @param context An optional pointer to user-defined data (can be NULL).
 */
void aast_iterate_children(const Node* parent, AastChildCallback callback, void* context);

/**
 * @brief Decreases the reference count of a node and frees it if the count reaches zero.
 *
 * This is the primary memory management function. It should be called on any root
 * node that you are finished with. It will recursively release its children.
 *
 * @warning This function has a hard-coded depth limit (AAST_MAX_DEPTH) to prevent
 * stack overflows. Trees exceeding this depth will not be fully deallocated.
 */
void aast_release(Node* node);
/**
 * @brief Manually increases the reference count of a node.
 *
 * For advanced use cases where you need to manually manage the lifetime of a node
 * pointer. Every call to aast_retain() should be balanced by a later call to
 * aast_release().
 *
 * @return 0 on success, -1 on failure (if ref_count would overflow).
 */
int aast_retain(Node* node);
/**
 * @brief Creates a new tree state by "mutating" a node at a specified path.
 *
 * This is the core function for state change. It performs a "copy-on-write"
 * operation via structural sharing, creating a new root while reusing all
 * unmodified nodes from the original tree.
 *
 * @param root The root of the original, immutable tree.
 * @param path An array of strings representing the key-based path to the target node.
 * @param path_len The number of elements in the path array.
 * @param new_payload The new payload for the target node.
 * @return A pointer to the new root node of the modified tree, or NULL on failure.
 */
Node* accrete_new_state(const Node* root, const char* const* path, size_t path_len, const char* new_payload);
/**
 * @brief Cryptographically verifies the integrity of the entire A-AST.
 *
 * Performs a full, recursive traversal and re-computes the hash of every node,
 * comparing it against the stored hash.
 *
 * @param root The root node of the tree to verify.
 * @return 1 if the tree is valid, 0 otherwise or if max depth is exceeded.
 */
int aast_verify_integrity(const Node* root);

/**
 * @brief Constructs an A-AST from a structured, indented text block.
 *
 * @param text_data A string containing the text representation to parse.
 * @param nfc_validator Optional root node of the utf8_nfc.aast validation tree. If provided, enforces strict NFC hygiene.
 * @return A pointer to the root node of the newly ingested tree, or NULL on failure.
 */
Node* aast_ingest_from_text(const char* text_data, const Node* nfc_validator);

/**
 * @brief Serializes an entire A-AST to a file in a leaves-first format.
 * @param root The root node of the tree to serialize.
 * @param filename The path of the file to write to.
 * @return 0 on success, -1 on failure.
 */
int aast_serialize_to_file(const Node* root, const char* filename);
/**
 * @brief Deserializes an A-AST from a file.
 *
 * Assumes the file was created with aast_serialize_to_file() and is in the
 * correct leaves-first format. Verifies the integrity of every node during load.
 *
 * @param filename The path of the file to read from.
 * @return A pointer to the root node of the loaded tree, or NULL on failure.
 */
Node* aast_deserialize_from_file(const char* filename);
// --- Conditionally Compiled Debugging Utilities ---
#ifdef DEBUG_PRINT
/**
 * @brief (DEBUG ONLY) Prints a human-readable representation of the tree.
 *
 * This function is only available if the code is compiled with the -DDEBUG_PRINT flag.
 */
void aast_print_tree(const Node* root);
#endif // DEBUG_PRINT
#endif // AAST_H
