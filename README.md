# Accretive-Abstract-State-Tree (A-AST)

An experimental, content-addressable data structure written in C.

Developed as an independent research initiative, the A-AST is an engineering exercise aimed at building a "ledger of intent." It explores an alternative to linear, string-based parsers by shifting structural validation from O(N) parsing to an O(1) cryptographic checksum. The goal is to build a structure optimized for deterministic LLM and agentic ingestion, prioritizing memory safety and clear context boundaries.

## Conceptual Foundations & Prior Art

The A-AST is fundamentally derivative work, combining well-established concepts from computer science into a specific implementation. It is not a novel invention but stands on the shoulders of giants.

* **Merkle Trees:** The core structure is a **Merkle DAG** (Directed Acyclic Graph), where each node's identity is a cryptographic hash of its own content and its children's identities. This provides the foundation for verifiable integrity.
* **Git's Object Model:** Its state management is directly inspired by Git. Like commits, new states are "accreted" by creating new nodes that point to a mix of new and existing, unmodified nodes. This preserves the full history of previous states efficiently.
* **Purely Functional Data Structures:** The principle of **structural sharing** is borrowed from the study of immutable data structures, notably as described in the work of Chris Okasaki. This allows for efficient "mutations" without the high cost of deep copying the entire tree for every change.

## Design Philosophy

* **Immutability via Accretion:** Nodes cannot be altered once created. Modifying the tree requires accreting a new state, naturally preserving previous versions.
* **Cryptographic Provenance:** Each parent node's ID is a deterministic SHA-256 hash of its attributes and its children's hashes. This makes the entire structure content-addressable and verifiable.
* **Agent-First, Human-Readable Second:** Traditional data formats often prioritize human readability. The A-AST prioritizes mathematical determinism for machine ingestion. The goal is to provide a structure that an AI agent can trust and verify with a single cryptographic check, minimizing ambiguity and parsing overhead. Human-debugging utilities, like the tree pretty-printer, are considered secondary and are compiled conditionally, ensuring they add **zero overhead** to a production build.
* For a fuller breakdown, refer to `DESIGN.md`, which breaks down the design and architecture decisions and core philosophy in detail.
* `CONTRIBUTING.md` contains the direction on contributing to the A-AST project. The project is open sourced and as such, no bounties or contracts are available at this time. We appreciate your understanding.

## Execution State & Verification

The project is being developed in discrete, verifiable phases.

* **Phases 1-3.5: Core Structure & Hardened Serialization**
    * Implemented the core `Node` struct, cryptographic anchoring with OpenSSL's EVP API, and a hardened, length-prefixed canonical buffer format.
* **Phase 4: Accretion Engine**
    * Implemented `accrete_new_state` to enable "mutations" via structural sharing.
* **Phase 5: Memory Management**
    * Built a robust reference counting system (`aast_retain`, `aast_release`) to prevent memory leaks and double-free corruption.
* **Phases 6-6.8: Verification & Real-World Ingestion**
    * Implemented `aast_verify_integrity` to validate the Merkle DAG.
    * Implemented `aast_ingest_from_text` as the first stress test of the core engine with non-synthetic data.
* **Phases 7-8.5: Library Refactoring & Performance Hardening**
    * Separated the codebase into a reusable library (`aast.h`, `aast.c`).
    * Replaced the O(N) child array with a `uthash`-based hash table (`ChildEntry*`), enabling near-constant-time child lookups by key.
* **Phase 9.0: Empirical Boundary Mapping**
    * Established an automated isolation test suite (`tests/`) to map the structural boundaries and physical limits.
    * Integrated strict operational constraint validation directly into the `create_node` constructor.
* **Phase 9.1: The Query API & Discovery**
    * Implemented `aast_find_child_by_key` (O(1) shallow lookup) and `aast_query_path` (stack-safe deep traversal).
    * Implemented `aast_iterate_children`, a zero-allocation, Inversion-of-Control callback pattern for Agentic discovery.
* **Phase 9.2: Formal `.aast` Filetype & Bootstrapped Validation**
    * Formalized the `.aast` format, anchoring it with a cryptographic header (`AAST_V1|[ROOT_HASH]`) that instantly rejects file tampering upon load.
    * Built a Python extractor to download the Unicode Consortium's UTF-8 NFC ruleset and compiled it into a 14,000-rule A-AST Trie (`utf8_nfc.aast`). The C-engine uses its own data structure to strictly validate incoming text hygiene in O(1) time.
* **Phase 9.3: Opaque Payload Transport (Current State)**
    * Solved the "Code Payload" parser collision problem using out-of-band signaling. Raw source code (Python, C, JSON) is wrapped in the illegal UTF-8 byte (`0xFF`). The parser suspends structural rules, safely buffers the complex code, and strips the wrappers before hashing, guaranteeing zero-collision ingestion.

## Empirical Constraints & Performance Metrics

To guarantee sub-millisecond execution times and absolute system runtime stability, the A-AST explicitly enforces the following empirically mapped boundary rules. Input parameters violating these thresholds are rejected instantly before any heap or stack space is allocated.

| Constraint Vector | Hard Target Limit | Failure Mode Preempted | Testing Methodology |
| :--- | :--- | :--- | :--- |
| **Max Key Length** | `256 Bytes` | $O(N)$ string hashing creep in `uthash` routines. | Linear scale sweeps measuring microsecond latency under `HASH_FIND_STR` load. |
| **Max Type Name** | `15 Bytes` | Buffer overruns / string truncation in fixed-width `char type[16]` arrays. | Fixed-width cache line alignment optimization. |
| **Max Contiguous Payload** | `512 MB` | Linux Kernel Out-Of-Memory (OOM) tracking drops or sequential hashing exhaustion. | Monolithic allocation steps climbing up to a 1GB contiguous payload benchmark pass. |
| **Max Traversal Depth** | `35,000 Layers` | Stack frame overflow and uncatchable `SIGSEGV` core dumps. | High-density vertical spine accretion tests mapping the 208-byte x86_64 stack frame ceiling. |

### Real-World Data Benchmarks (The Blind Code Test)
To verify the fidelity of the `0xFF` opaque payload transport mechanism, a real-world Python script (`sw_attention_core.py`) containing deep indentation, newlines, and unescaped colons was fed through the Universal Round-Trip Harness.
* **Payload Size:** 6.2 KB *(Note: While physically small, this size is sufficient to prove the parser state-machine correctly navigates hundreds of illegal delimiters without triggering false node-breaks. Volumetric bounds up to 512MB are tested separately).*
* **Ingestion, Edge-Validation, & SHA-256 Hashing:** `~3.2 milliseconds`
* **Deep Path Query & Retrieval:** `~1.0 microsecond`
* **Extraction Fidelity:** 100% Mathematical Match. `sha256sum` verified the extracted bytes were identical to the source file, proving the out-of-band wrapping leaves zero footprint on the core data.

### Payload Curvature Analysis (The 512MB Ceiling)
To verify that the ingestion and hashing algorithms degrade linearly $O(N)$ without hidden exponential memory fragmentation bottlenecks, a synthetic volumetric sweep was conducted entirely in RAM, pushing the engine to its hard 512MB contiguous payload limit.

| Payload Volume | Ingestion & SHA-256 Hashing | Deep Path Query (O1) | Fidelity |
| :--- | :--- | :--- | :--- |
| **10 MB** | `0.036 seconds` | `~1 microsecond` | 100% |
| **50 MB** | `0.166 seconds` | `~1 microsecond` | 100% |
| **100 MB** | `0.327 seconds` | `~1 microsecond` | 100% |
| **250 MB** | `0.803 seconds` | `~1 microsecond` | 100% |
| **512 MB** | `1.647 seconds` | `~2 microseconds` | 100% |

**Conclusion:** The engine maintains $O(N)$ linear ingestion scaling up to the half-gigabyte hard ceiling. Furthermore, the read-path latency $O(1)$ remains entirely decoupled from the payload volume, guaranteeing microsecond retrieval times regardless of node size.

### Distributed Chunking & Lazy-Loading (`AAST_LINK`)
To bypass OS file size limits and physical RAM constraints, the A-AST supports native decentralized storage via `AAST_LINK` nodes, which point to external `.aast` files. To prevent AI Agents from accidentally triggering Out-Of-Memory (OOM) crashes during full-table scans across thousands of linked files, resolution is strictly handled via an Inversion-of-Control (IoC) Context-Managed Callback, ensuring active RAM is automatically flushed the instant a chunk query completes.

**Bare-Metal Benchmark (Non-Valgrind):**
A synthetic test simulating an Agent resolving and querying a 50 MB chunk, followed by a sequential full-table scan thrashing 100 separate 50 MB chunks.
* **50MB Single Link Resolution:** `~147 milliseconds` *(Includes Disk I/O, Parsing, and the "Iron Gate" SHA-256 cryptographic verification).*
* **5 Gigabyte Sequential Full-Table Scan (100 Iterations):** `~14.9 seconds`.
* **Peak RAM Usage During 5GB Scan:** `< 60 MB`.

**Conclusion:** The Context-Managed Callback architecture structurally prevents memory bleed. The engine can sequentially parse, hash, and query Terabytes of cryptographically verified, distributed `.aast` files at a rate of roughly `~335 MB/s` on standard consumer hardware, within the host system's RAM constraints regardless of total dataset size.

## Build Instructions

### Dependencies

* **OpenSSL:** Provides SHA-256 cryptographic primitives via the EVP API. Install the development headers with `sudo apt install libssl-dev` on Debian/Ubuntu systems.
* **uthash:** A header-only hash table library used for the internal child-node map. Bundled directly within this repository to guarantee deterministic builds.
* uthash is available at:  [https://troydhanson.github.io/uthash/](https://troydhanson.github.io/uthash/).

---

### Compile & Run

The project is managed via a comprehensive `Makefile` encompassing production builds, conditionally-compiled debug targets, and automated Valgrind suites.

```bash
# Build production and debug targets
make

# Run the standard production binary through the memory sandbox
valgrind --leak-check=full ./example_aast

# --- Specialized Stress-Testing Suites ---

# Run the Query API tests (O1 lookups and iterators)
make test_query

# Run the horizontal/vertical query scaling sweeps (Up to 100k children/depth)
make test_query_vertical
make test_query_horizontal

# Run the Filetype Formalization test (Header tampering checks)
make test_filetype

# Compile the UTF-8 NFC ruleset from the Unicode Consortium via Python/C
make build_ucd

# Run the NFC validation engine test against the generated ruleset
make test_nfc

# Run the Opaque Code Payload test (0xFF out-of-band parsing)
make test_payload

# Run the Blind Code Round-Trip Harness against a target file
make tests/test_roundtrip
./tests/test_roundtrip <input_file> <output_file>

# Remove all build artifacts, binary targets, and Valgrind logs
make clean
```
