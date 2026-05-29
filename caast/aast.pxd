# cython: language_level=3

# Import standard C types
from libc.stddef cimport size_t
from libc.stdint cimport uint8_t

# Tell Cython to look in the parent directory for the actual C header
cdef extern from "../aast.h":

    # --- Macros & Constants ---
    cdef int AAST_MAX_KEY_LEN
    cdef int AAST_MAX_TYPE_LEN
    cdef int AAST_MAX_PAYLOAD_SIZE
    cdef const char* AAST_TYPE_LINK

    # --- Struct Definitions ---
    
    # We define Node as a complete struct so Cython can access its fields directly.
    # Note: We omit ChildEntry and UT_hash_handle because Cython should never 
    # touch the internal uthash mechanics directly.
    cdef struct Node:
        char type[16]
        char *payload
        size_t child_count
        size_t ref_count
        char hash[65]
        # We don't need to expose 'children' to Python, we keep it hidden.

    cdef struct AastChildInput:
        const char* key
        Node* child

    # --- Memory Lifecycle API ---
    void aast_release(Node* node)
    int aast_retain(Node* node)

    # --- Constructor & Mutation API ---
    Node* create_node(const char* type, const char* payload, const AastChildInput* children_input, size_t child_count)
    Node* accrete_new_state(const Node* root, const char* const* path, size_t path_len, const char* new_payload)

    # --- Query API (Returning Weak Pointers) ---
    const Node* aast_find_child_by_key(const Node* parent, const char* key)
    const Node* aast_query_path(const Node* root, const char* const* path, size_t path_len)

    # --- Discovery & IoC Callbacks ---
    ctypedef void (*AastChildCallback)(const char* key, const Node* child, void* context)
    void aast_iterate_children(const Node* parent, AastChildCallback callback, void* context)
    
    int aast_execute_in_link_context(const Node* link_node, void (*callback)(const Node* loaded_root, void* context), void* context)

    # --- Persistence API ---
    int aast_serialize_to_file(const Node* root, const char* filename)
    Node* aast_deserialize_from_file(const char* filename)

    # --- Ingestion & Validation API ---
    int aast_validate_utf8_nfc(const Node* validator_root, const char* text)
    Node* aast_ingest_from_text(const char* text_data, const Node* nfc_validator)

    # --- Integrity API ---
    int aast_verify_integrity(const Node* root)
