# cython: language_level=3

import unicodedata
from libc cimport stdlib
from aast cimport Node, aast_retain, aast_release, aast_find_child_by_key, aast_query_path, aast_iterate_children, aast_deserialize_from_file, aast_ingest_from_text, aast_ingest_opaque_node
# --- Constants for Opaque Transport ---
cdef bytes OPAQUE_START = b"\xC0\xC1\xFF"
cdef bytes OPAQUE_END   = b"\xFF\xC1\xC0"

# =============================================================================
# 1. The Pointer Factory (Internal Helper)
# =============================================================================
cdef AASTNode _wrap_weak_pointer(const Node* weak_node):
    if weak_node == NULL:
        return None
    
    aast_retain(<Node*>weak_node)
    
    cdef AASTNode py_obj = AASTNode.__new__(AASTNode)
    py_obj._c_node = <Node*>weak_node
    
    return py_obj

# =============================================================================
# 2. C-to-Python Callbacks (GIL Managed)
# =============================================================================
# FIXED: Added 'noexcept' to satisfy the strict C function pointer signature
cdef void _list_population_callback(const char* key, const Node* child, void* context) noexcept with gil:
    cdef list results = <list>context
    try:
        py_key = key.decode('utf-8')
        py_child = _wrap_weak_pointer(child)
        results.append((py_key, py_child))
    except Exception as e:
        print(f"[Cython Bridge Error] Callback exception: {e}")

# =============================================================================
# 3. The Python API Class
# =============================================================================
cdef class AASTNode:
    cdef Node* _c_node

    def __cinit__(self):
        self._c_node = NULL

    def __dealloc__(self):
        if self._c_node != NULL:
            aast_release(self._c_node)
            self._c_node = NULL

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
        if self._c_node == NULL or self._c_node.payload == NULL:
            return None
            
        cdef bytes raw_payload = self._c_node.payload
        
        if raw_payload.startswith(OPAQUE_START) and raw_payload.endswith(OPAQUE_END):
            raw_payload = raw_payload[len(OPAQUE_START) : -len(OPAQUE_END)]
            
        return raw_payload.decode('utf-8')

    def find_child(self, str key):
        if self._c_node == NULL: return None
        
        key_nfc = unicodedata.normalize('NFC', key)
        cdef bytes key_bytes = key_nfc.encode('utf-8')
        
        # FIXED: Extract the raw C-pointer BEFORE releasing the GIL
        cdef const char* c_key = <const char*>key_bytes
        cdef const Node* weak_result
        
        with nogil:
            weak_result = aast_find_child_by_key(self._c_node, c_key)
            
        return _wrap_weak_pointer(weak_result)

    def query_path(self, list path_keys):
        if self._c_node == NULL or not path_keys: return None
        
        cdef list encoded_keys = [unicodedata.normalize('NFC', k).encode('utf-8') for k in path_keys]
        cdef size_t path_len = len(encoded_keys)
        
        cdef const char** c_path = <const char**>stdlib.malloc(path_len * sizeof(char*))
        if not c_path:
            raise MemoryError("Failed to allocate path array in Cython bridge.")
            
        for i in range(path_len):
            c_path[i] = encoded_keys[i]
            
        cdef const Node* weak_result
        with nogil:
            weak_result = aast_query_path(self._c_node, c_path, path_len)
            
        stdlib.free(c_path)
        return _wrap_weak_pointer(weak_result)

    def get_children(self):
        if self._c_node == NULL: return []
        
        cdef list results = []
        with nogil:
            aast_iterate_children(self._c_node, _list_population_callback, <void*>results)
            
        return results

    def __setattr__(self, name, value):
        raise TypeError("A-AST nodes are strictly immutable. Write operations are disabled.")

# --- System Deserialization Factory ---
def load_from_file(str filepath):
    cdef bytes b_filepath = filepath.encode('utf-8')
    
    # FIXED: Extract the raw C-pointer BEFORE releasing the GIL
    cdef const char* c_filepath = <const char*>b_filepath
    cdef Node* loaded_root
    
    with nogil:
        loaded_root = aast_deserialize_from_file(c_filepath)
        
    if loaded_root == NULL:
        raise ValueError(f"Failed to load A-AST from {filepath}. File may be corrupted or missing.")
        
    cdef AASTNode py_obj = AASTNode.__new__(AASTNode)
    py_obj._c_node = loaded_root
    return py_obj
# --- System Ingestion Factory ---
def ingest_from_string(str text_data, wrap_opaque=False):
    """
    Ingests a raw Python string into the C-Core A-AST engine.
    
    If wrap_opaque=True, the bridge automatically normalizes the text to NFC,
    encodes it, and wraps it in the illegal UTF-8 out-of-band markers 
    (\xC0\xC1\xFF) before passing it to C. This makes the string safe from 
    structural parsing collisions.
    """
    cdef bytes b_text
    
    if wrap_opaque:
        # 1. Normalize to NFC (Strict Encoding Boundary)
        text_nfc = unicodedata.normalize('NFC', text_data)
        
        # 2. Package into A-AST structural format with Opaque Wrappers
        # We format it as a root node containing a 'payload' child
        packaged_text = f"root:System:\n  payload:Data:\xC0\xC1\xFF{text_nfc}\xFF\xC1\xC0\n"
        b_text = packaged_text.encode('utf-8')
    else:
        # Standard structural ingestion
        b_text = text_data.encode('utf-8')

    cdef const char* c_text = <const char*>b_text
    cdef Node* loaded_root
    
    # Execute C-Core Parser
    with nogil:
        # Passing NULL for the validator tree for this benchmark
        loaded_root = aast_ingest_from_text(c_text, NULL)
        
    if loaded_root == NULL:
        raise ValueError("A-AST C-Core rejected the text. Check formatting or NFC compliance.")
        
    # Route through Pointer Factory
    cdef AASTNode py_obj = AASTNode.__new__(AASTNode)
    py_obj._c_node = loaded_root
    return py_obj
# --- System Ingestion Factory ---
def ingest_from_string(str text_data, wrap_opaque=False):
    cdef bytes b_text
    cdef bytes b_wrapped
    cdef const char* c_text
    cdef Node* loaded_root
    
    if wrap_opaque:
        # 1. Strip trailing newlines/whitespace
        clean_text = text_data.strip()
        
        # 2. Normalize to NFC (Strict Encoding Boundary)
        text_nfc = unicodedata.normalize('NFC', clean_text)
        
        # 3. Encode the valid text to bytes FIRST
        b_text = text_nfc.encode('utf-8')
        
        # 4. Concatenate raw bytes (Preventing Python from altering the illegal markers)
        b_wrapped = b"\xC0\xC1\xFF" + b_text + b"\xFF\xC1\xC0"
        c_text = <const char*>b_wrapped
        
        # Execute C-Core Fast-Path
        with nogil:
            loaded_root = aast_ingest_opaque_node("Code", c_text, NULL)
            
    else:
        b_text = text_data.encode('utf-8')
        c_text = <const char*>b_text
        
        with nogil:
            loaded_root = aast_ingest_from_text(c_text, NULL)
        
    if loaded_root == NULL:
        raise ValueError("A-AST C-Core rejected the text. Check formatting or NFC compliance.")
        
    cdef AASTNode py_obj = AASTNode.__new__(AASTNode)
    py_obj._c_node = loaded_root
    return py_obj
