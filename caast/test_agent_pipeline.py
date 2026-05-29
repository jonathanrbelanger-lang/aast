import caast
import time
import hashlib
import sys
import os

def run_benchmark(filepath):
    print("==================================================")
    print("--- Full-Stack Agentic Pipeline Benchmark --------")
    print("==================================================\n")
    
    # 1. Read Target File
    if not os.path.exists(filepath):
        print(f"FAILED: File not found -> {filepath}")
        return

    with open(filepath, 'r', encoding='utf-8') as f:
        original_code = f.read()
        
    file_size_kb = len(original_code) / 1024
    print(f"Target File: {filepath} ({file_size_kb:.2f} KB)")
    
    # Calculate native Python SHA-256 for ground-truth fidelity checking
    original_hash = hashlib.sha256(original_code.encode('utf-8')).hexdigest()
    
    # ==========================================
    # BENCHMARK 1: Ingestion & C-Boundary Crossing
    # ==========================================
    start = time.perf_counter()
    
    # Force Python to flush its output buffers so any C-level printf/fprintf 
    # executes immediately and appears in the correct chronological order.
    sys.stdout.flush()
    sys.stderr.flush()
    
    try:
        # This handles NFC Normalization, Opaque Wrapping, C-Parsing, and Merkle Hashing
        tree = caast.ingest_from_string(original_code, wrap_opaque=True)
    except Exception as e:
        print(f"\n[!] CYTHON EXCEPTION CAUGHT: {e}")
        print("-> The C-Core parser (aast_ingest_from_text) returned NULL.")
        print("-> Look directly above this message for any [A-AST Error] printed by C.")
        return

    end = time.perf_counter()
    ingest_time = end - start
    print(f"\n[Metric] Python->C Ingestion & Merkle Hash: {ingest_time:.6f} seconds")
    print(f"         Root Node C-Hash: {tree.hash}")
    
    # ==========================================
    # BENCHMARK 2: Payload Extraction & Stripping
    # ==========================================
    start = time.perf_counter()
    
    # Because we used wrap_opaque=True, the returned tree IS the opaque node.
    # The property accessor automatically strips the opaque markers invisibly!
    extracted_code = tree.payload
    
    end = time.perf_counter()
    extract_time = end - start
    print(f"[Metric] Payload Decoding & Marker Strip:   {extract_time:.6f} seconds\n")
    
    if not extracted_node:
        print("FAILED: Could not query payload back from C-Core.")
        return
        
    # ==========================================
    # BENCHMARK 3: Payload Extraction & Stripping
    # ==========================================
    start = time.perf_counter()
    
    # The property accessor automatically strips the opaque markers
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
        print(f"         Expected: {original_hash}")
        print(f"         Got:      {extracted_hash}")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python3 test_agent_pipeline.py <path_to_python_file>")
    else:
        run_benchmark(sys.argv[1])    
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
