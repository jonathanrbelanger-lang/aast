#ifndef AAST_H
#define AAST_H

#include <stddef.h> // For size_t

// ----------------------------------------------------------------------------
// Public Data Structure Definition
// ----------------------------------------------------------------------------

/**
 * @brief Represents a key-value entry in the children hash table of a Node.
 *
 * This structure makes the children of a node searchable by key in O(1) time.
 * It is managed internally by the A-AST library.
 */
typedef struct ChildEntry {
    char* key;              // The key of the child node (used for hashing).
    struct Node* child_node;   // A pointer to the actual child node.
    UT_hash_handle hh;      // Provided by uthash.h to make this struct hashable.
} ChildEntry;

/**
 * @brief The core data structure for the Accretive-Abstract-State-Tree.
 *
 * An immutable, content-addressable node within a Merkle DAG. Its state is
 * cryptographically anchored by a SHA-256 hash upon creation.
 */
typedef struct Node {
    char type[16];           // Semantic type of the node (e.g., "ROOT", "TEXT").
    char *payload;           // Owned by the Node. The actual string data value.
    ChildEntry *children;    // A hash table of children, keyed by the child's 'key'. NULL if no children.
    size_t child_count;      // The number of children.
    size_t ref_count;        // The reference count for memory management.
    char hash[65];           // The SHA-256 hex string that uniquely identifies this node's state.
} Node;

// ----------------------------------------------------------------------------
// Public API Function Prototypes
// ----------------------------------------------------------------------------

/**
 * @brief Creates a new, immutable A-AST node.
 *
 * This is the primary constructor. It performs deep copies of the key and payload,
 * shallow copies the children pointer list, and computes the final SHA-256 hash.
 * On success, the returned node has a reference count of 1.
 *
 * @param type The semantic type for the node.
 * @param key The semantic key for the node.
 * @param payload The string data for the node.
 * @param children An array of Node pointers for the children.
 * @param child_count The number of children.
 * @return A pointer to the newly created Node, or NULL on allocation failure.
 */
Node* create_node(const char* type, const char* key, const char* payload, Node** children, size_t child_count);

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
 * @return A pointer to the root node of the newly ingested tree, or NULL on failure.
 */
Node* aast_ingest_from_text(const char* text_data);

/**
 * @brief Serializes an entire A-AST to a file in a leaves-first format.
 *
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
