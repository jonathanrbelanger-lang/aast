import caast
import time
import hashlib
import sys
import os

def run_benchmark(filepath):
    print("==================================================")
    print("--- Full-Stack Agentic Pipeline Benchmark --------")
    print("==================================================\n")
    
    if not os.path.exists(filepath):
        print(f"FAILED: File not found -> {filepath}")
        return

    with open(filepath, 'r', encoding='utf-8') as f:
        original_code = f.read().strip() # Strip here to match the C-Core's expected fidelity
        
    file_size_kb = len(original_code) / 1024
    print(f"Target File: {filepath} ({file_size_kb:.2f} KB)")
    
    original_hash = hashlib.sha256(original_code.encode('utf-8')).hexdigest()
    
    # ==========================================
    # BENCHMARK 1: Ingestion & C-Boundary Crossing
    # ==========================================
    start = time.perf_counter()
    
    sys.stdout.flush()
    sys.stderr.flush()
    
    try:
        tree = caast.ingest_from_string(original_code, wrap_opaque=True)
    except Exception as e:
        print(f"\n[!] CYTHON EXCEPTION CAUGHT: {e}")
        return

    end = time.perf_counter()
    ingest_time = end - start
    print(f"\n[Metric] Python->C Fast-Path Ingestion: {ingest_time:.6f} seconds")
    print(f"         Root Node C-Hash: {tree.hash}")
    
    # ==========================================
    # BENCHMARK 2: Payload Extraction & Stripping
    # ==========================================
    start = time.perf_counter()
    
    extracted_code = tree.payload
    
    end = time.perf_counter()
    extract_time = end - start
    print(f"[Metric] Payload Decoding & Marker Strip: {extract_time:.6f} seconds\n")
    
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
