# Accretive-Abstract-State-Tree (A-AST)

An experimental, content-addressable data structure written in C.

Developed as an independent research initiative, the A-AST is an engineering exercise aimed at building a "ledger of intent." It explores an alternative to linear, string-based parsers by shifting structural validation from O(N) parsing to an O(1) cryptographic checksum. The goal is to build a structure optimized for deterministic LLM and agentic ingestion, prioritizing memory safety and clear context boundaries.

## Conceptual Foundations & Prior Art

The A-AST is fundamentally derivative work, combining well-established concepts from computer science into a specific implementation. It is not a novel invention but stands on the shoulders of giants.

*   **Merkle Trees:** The core structure is a **Merkle DAG** (Directed Acyclic Graph), where each node's identity is a cryptographic hash of its own content and its children's identities. This provides the foundation for verifiable integrity.
*   **Git's Object Model:** Its state management is directly inspired by Git. Like commits, new states are "accreted" by creating new nodes that point to a mix of new and existing, unmodified nodes. This preserves the full history of previous states efficiently.
*   **Purely Functional Data Structures:** The principle of **structural sharing** is borrowed from the study of immutable data structures, notably as described in the work of Chris Okasaki. This allows for efficient "mutations" without the high cost of deep copying the entire tree for every change.

## Design Philosophy

*   **Immutability via Accretion:** Nodes cannot be altered once created. Modifying the tree requires accreting a new state, naturally preserving previous versions.
*   **Cryptographic Provenance:** Each parent node's ID is a deterministic SHA-256 hash of its attributes and its children's hashes. This makes the entire structure content-addressable and verifiable.
*   **Agent-First, Human-Readable Second:** Traditional data formats often prioritize human readability. The A-AST prioritizes mathematical determinism for machine ingestion. The goal is to provide a structure that an AI agent can trust and verify with a single cryptographic check, minimizing ambiguity and parsing overhead. Human-debugging utilities, like the tree pretty-printer, are considered secondary and are compiled conditionally, ensuring they add **zero overhead** to a production build.
*   For a fuller breadown, refer to main/Design.md, which breaks down the design and architecture decisions and core philosophy in detail.
*   main/CONTRIBUTING.md contains the direction on contributing to the A-AST project. The project is open sourced and as such, no bounties or contracts are available at this time. We appreciate your understanding.

## Execution State & Verification

The project is being developed in discrete, verifiable phases.

*   **Phases 1-3.5: Core Structure & Hardened Serialization**
    *   Implemented the core `Node` struct, cryptographic anchoring with OpenSSL's EVP API, and a hardened, length-prefixed canonical buffer format.
*   **Phase 4: Accretion Engine**
    *   Implemented `accrete_new_state` to enable "mutations" via structural sharing, inspired by functional programming principles.
*   **Phase 5: Memory Management**
    *   Built a robust reference counting system (`aast_retain`, `aast_release`) to prevent memory leaks and double-free corruption when multiple tree states share common nodes.
*   **Phase 6-6.5: Verification & Debugging**
    *   Implemented `aast_verify_integrity` to validate the Merkle DAG and detect in-memory tampering.
    *   Added a conditionally-compiled pretty-printer for development and debugging.
*   **Phase 6.8: Real-World Ingestion & Stress Testing**
    *   Implemented `aast_ingest_from_text` to parse a structured, indented text file.
    *   This served as the first stress test of the core engine with non-synthetic, externally-loaded data.
*   **Phases 7-8: Library Refactoring**
    *   Formally separated the codebase into a reusable library (`aast.h`, `aast.c`) and a consumer application (`example.c`), governed by a `Makefile` with distinct debug and release targets.
*   **Phase 8.5: Performance Hardening & Stabilization**
    *   Replaced the O(N) child array with a `uthash`-based hash table (`ChildEntry*`), enabling near-constant-time child lookups by key and making the structure genuinely scalable for nodes with large child sets.
    *   Updated the public API (`create_node`) to accept an array of `AastChildInput` structs, making the parent-child key relationship explicit.
    *   Rewrote all core functions to be fully compatible with the hash-based child structure.
    *   Resolved a use-after-free bug in the ingestion engine and a memory leak in `aast_deserialize_from_file` related to temporary node-lookup map ownership.
    *   *Current state:* The full round-trip (ingest → serialize → deserialize → verify) passes Valgrind with zero memory errors and zero leaks, validating the robustness of the memory model under realistic load.

## Build Instructions

### Dependencies

*   **OpenSSL:** Provides SHA-256 cryptographic primitives via the EVP API. Install the development headers with `sudo apt install libssl-dev` on Debian/Ubuntu systems.
*   **uthash:** A header-only hash table library used for the internal child-node map. No installation required — place `uthash.h` in the project root directory. Available at [https://troydhanson.github.io/uthash/](https://troydhanson.github.io/uthash/).

---

### Compile & Run

The project is built via `make`. The `Makefile` produces two targets: a lean production binary and a debug binary with the conditionally-compiled pretty-printer enabled.

```bash
# Build both targets
make

# Build only the production binary
make example_aast

# Build only the debug binary (enables DEBUG_PRINT)
make example_aast_debug

# Run the production binary through the memory sandbox
valgrind --leak-check=full ./example_aast

# Run the debug binary
valgrind --leak-check=full ./example_aast_debug

# Remove all build artifacts
make clean
```
