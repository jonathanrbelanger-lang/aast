import caast
import time
import hashlib

def run_benchmark(filepath):
    print("==================================================")
    print("--- Full-Stack Agentic Pipeline Benchmark --------")
    print("==================================================\n")
    
    # 1. Read Target File
    with open(filepath, 'r') as f:
        original_code = f.read()
        
    file_size_kb = len(original_code) / 1024
    print(f"Target File: {filepath} ({file_size_kb:.2f} KB)")
    
    # Calculate native Python SHA-256 for ground-truth fidelity checking
    original_hash = hashlib.sha256(original_code.encode('utf-8')).hexdigest()
    
    # ==========================================
    # BENCHMARK 1: Ingestion & C-Boundary Crossing
    # ==========================================
    start = time.perf_counter()
    
    # This handles NFC Normalization, Opaque Wrapping, C-Parsing, and Merkle Hashing
    tree = caast.ingest_from_string(original_code, wrap_opaque=True)
    
    end = time.perf_counter()
    ingest_time = end - start
    print(f"\n[Metric] Python->C Ingestion & Merkle Hash: {ingest_time:.6f} seconds")
    print(f"         Root Node C-Hash: {tree.hash}")
    
    # ==========================================
    # BENCHMARK 2: Deep Query & Pointer Factory
    # ==========================================
    start = time.perf_counter()
    
    # Ask the C-Core to find the payload, elevate the pointer, and wrap it in Python
    extracted_node = tree.query_path(["payload"])
    
    end = time.perf_counter()
    query_time = end - start
    print(f"[Metric] C->Python Deep Query & Elevation:  {query_time:.6f} seconds")
    
    if not extracted_node:
        print("FAILED: Could not query payload back from C-Core.")
        return
        
    # ==========================================
    # BENCHMARK 3: Payload Extraction & Stripping
    # ==========================================
    start = time.perf_counter()
    
    # The property accessor automatically strips the \xC0\xC1\xFF markers
    extracted_code = extracted_node.payload
    
    end = time.perf_counter()
    extract_time = end - start
    print(f"[Metric] Payload Decoding & Marker Strip:   {extract_time:.6f} seconds\n")
    
    # ==========================================
    # FIDELITY CHECK
    # ==========================================
    extracted_hash = hashlib.sha256(extracted_code.encode('utf-8')).hexdigest()
    
    if original_hash == extracted_hash:
        print("SUCCESS: 100% Data Fidelity Preserved Across C/Python Boundary.")
        print(f"         Original SHA-256:  {original_hash}")
        print(f"         Extracted SHA-256: {extracted_hash}")
    else:
        print("FAILED: Data Corruption detected at the boundary!")

if __name__ == "__main__":
    import sys
    if len(sys.argv) != 2:
        print("Usage: python3 test_agent_pipeline.py <path_to_python_file>")
    else:
        run_benchmark(sys.argv[1])
