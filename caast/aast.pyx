# cython: language_level=3

import unicodedata
from aast cimport Node, aast_retain, aast_release, aast_find_child_by_key, aast_query_path, aast_iterate_children

# --- Constants for Opaque Transport ---
cdef bytes OPAQUE_START = b"\xC0\xC1\xFF"
cdef bytes OPAQUE_END   = b"\xFF\xC1\xC0"

# =============================================================================
# 1. The Pointer Factory (Internal Helper)
# =============================================================================
cdef AASTNode _wrap_weak_pointer(const Node* weak_node):
    """
    The Pointer Factory. Intercepts a weak C-pointer, elevates it to an owning 
    pointer via aast_retain, and encapsulates it safely in a Python object.
    """
    if weak_node == NULL:
        return None
    
    # 1. Elevate to owning pointer (C-level)
    # We must cast away const to retain it, as the Python object now claims ownership.
    aast_retain(<Node*>weak_node)
    
    # 2. Instantiate the Python wrapper without calling __init__
    cdef AASTNode py_obj = AASTNode.__new__(AASTNode)
    py_obj._c_node = <Node*>weak_node
    
    return py_obj

# =============================================================================
# 2. C-to-Python Callbacks (GIL Managed)
# =============================================================================
cdef void _list_population_callback(const char* key, const Node* child, void* context) with gil:
    """
    A static C-callback that acquires the GIL, elevates the child pointer, 
    and appends the (key, wrapper) tuple to a Python list.
    """
    # The context is a Python list cast to void*
    cdef list results = <list>context
    
    try:
        py_key = key.decode('utf-8')
        py_child = _wrap_weak_pointer(child)
        results.append((py_key, py_child))
    except Exception as e:
        # In a more complex bridge, we would store this exception in a struct 
        # to re-raise later. For eager list population, standard appends rarely fail.
        print(f"[Cython Bridge Error] Callback exception: {e}")

# =============================================================================
# 3. The Python API Class
# =============================================================================
cdef class AASTNode:
    """
    Explicit, Read-Only Python interface for the A-AST C-Core.
    """
    cdef Node* _c_node

    def __cinit__(self):
        # We explicitly prevent manual instantiation from Python like `AASTNode()`.
        # Nodes must be created via factory functions or loaded from C.
        self._c_node = NULL

    def __dealloc__(self):
        """
        Ties the Python Garbage Collector to the C-Core Reference Counter.
        When this Python object dies, we release our claim on the C-node.
        """
        if self._c_node != NULL:
            aast_release(self._c_node)
            self._c_node = NULL

    # --- Property Accessors (Read-Only) ---

    @property
    def type(self):
        if self._c_node == NULL: return None
        return self._c_node.type.decode('utf-8')

    @property
    def hash(self):
        if self._c_node == NULL: return None
        return self._c_node.hash.decode('utf-8')

    @property
    def child_count(self):
        if self._c_node == NULL: return 0
        return self._c_node.child_count

    @property
    def payload(self):
        """
        Retrieves the payload, invisibly stripping the out-of-band opaque 
        markers (\xC0\xC1\xFF) if they exist.
        """
        if self._c_node == NULL or self._c_node.payload == NULL:
            return None
            
        cdef bytes raw_payload = self._c_node.payload
        
        # Invisible Data Boundary Unwrapping
        if raw_payload.startswith(OPAQUE_START) and raw_payload.endswith(OPAQUE_END):
            raw_payload = raw_payload[len(OPAQUE_START) : -len(OPAQUE_END)]
            
        return raw_payload.decode('utf-8')

    # --- Explicit Read-Only Interface ---

    def find_child(self, str key):
        """
        O(1) child lookup via explicit named method.
        """
        if self._c_node == NULL: return None
        
        # Enforce NFC Normalization at the boundary
        key_nfc = unicodedata.normalize('NFC', key)
        cdef bytes key_bytes = key_nfc.encode('utf-8')
        
        # Execute C-Core Query (Releasing GIL for pure C execution)
        cdef const Node* weak_result
        with nogil:
            weak_result = aast_find_child_by_key(self._c_node, <const char*>key_bytes)
            
        # Route through Pointer Factory
        return _wrap_weak_pointer(weak_result)

    def query_path(self, list path_keys):
        """
        Deep path traversal via explicit named method.
        """
        if self._c_node == NULL or not path_keys: return None
        
        # Normalize and encode the entire path
        cdef list encoded_keys = [unicodedata.normalize('NFC', k).encode('utf-8') for k in path_keys]
        cdef size_t path_len = len(encoded_keys)
        
        # Allocate a temporary C-array of char pointers
        cdef const char** c_path = <const char**>stdlib.malloc(path_len * sizeof(char*))
        if not c_path:
            raise MemoryError("Failed to allocate path array in Cython bridge.")
            
        for i in range(path_len):
            c_path[i] = encoded_keys[i]
            
        # Execute C-Core Query
        cdef const Node* weak_result
        with nogil:
            weak_result = aast_query_path(self._c_node, c_path, path_len)
            
        stdlib.free(c_path)
        
        return _wrap_weak_pointer(weak_result)

    def get_children(self):
        """
        Eagerly populates a list of all children via C-Core Inversion of Control.
        Returns a list of tuples: [(key, AASTNode), ...]
        """
        if self._c_node == NULL: return []
        
        cdef list results = []
        
        # Pass the Python list as the void* context. The callback will acquire 
        # the GIL and append to it.
        with nogil:
            aast_iterate_children(self._c_node, _list_population_callback, <void*>results)
            
        return results

    # --- Strict Immutability Enforcement ---
    def __setattr__(self, name, value):
        raise TypeError("A-AST nodes are strictly immutable. Write operations are disabled.")

# --- System Deserialization Factory ---
def load_from_file(str filepath):
    """
    Instantiates an A-AST from a formal .aast file on disk.
    """
    cdef bytes b_filepath = filepath.encode('utf-8')
    
    cdef Node* loaded_root
    with nogil:
        loaded_root = aast_deserialize_from_file(<const char*>b_filepath)
        
    if loaded_root == NULL:
        raise ValueError(f"Failed to load A-AST from {filepath}. File may be corrupted or missing.")
        
    cdef AASTNode py_obj = AASTNode.__new__(AASTNode)
    py_obj._c_node = loaded_root
    return py_obj
