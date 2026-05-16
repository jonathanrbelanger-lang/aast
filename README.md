# A-AST (Accretive Abstract Syntax Tree)

A memory-safe, cryptographically verifiable, content-addressable data structure written in C. 

Maintained as an independent research initiative under Exorobourii LLC, the A-AST acts as a "Ledger of Intent." It is designed to replace linear, string-based parsers (such as YAML or HTML) by shifting structural validation from an $O(N)$ regex and linting problem to an $O(1)$ cryptographic checksum. This architecture is specifically optimized for deterministic LLM and Agentic ingestion, prioritizing architectural efficiency and strict context boundaries.

## The Problem: The Directionality Tax
Traditional data serialization forces multidimensional semantic relationships into flat, linear strings. When downstream systems (or AI agents) ingest this data, they pay a massive computational "Directionality Tax" to parse, validate, and reconstruct those relationships. 

The A-AST eliminates this tax. By maintaining data in a cryptographically anchored Merkle DAG (Directed Acyclic Graph), the structure *is* the validation.

## Core Architectural Principles

* **Immutability via Accretion:** Nodes cannot be altered once created in memory. Mutations to the tree require the accretion of new nodes, inherently preserving previous states.
* **The Merkle DAG:** Every parent node's ID is a deterministic cryptographic hash (SHA-256) of its own attributes and the hashes of its children, enforcing the Avalanche Effect.
* **Projection Layer Separation:** The A-AST strictly holds semantic data and relationships. Formats like Markdown, JSON, or HTML are treated merely as downstream projections (Views) of the tree, decoupling meaning from presentation.
* **Cryptographic Provenance (Future Work):** The canonical architecture natively supports "Signed Nodes." By injecting an originator's identifier (Agent ID or Public Key) into the pre-hashed buffer, the A-AST establishes a mathematically binding Chain of Custody. This shifts AI outputs from untrusted "black box" text generation to a strictly auditable, multi-agent ledger.

## Execution State & Verification

The project is currently advancing through discrete, verifiable phases:

* **Phase 1: Memory & Structure (Verified)**
  * Defined core C structs with absolute heap management.
  * Implemented deep-copy payload ownership (`strdup`) and post-order traversal cleanup (`free_node`).
  * **Status:** Passed Valgrind with zero memory leaks (13 allocs, 13 frees).
* **Phase 2: Canonical Serialization (Verified)**
  * Implemented a strict C-level byte-packer to bypass the non-determinism of high-level serializers (like JSON).
  * Formats node data into a rigid string buffer `(TYPE:X|CHILDREN:Y|hash1,hash2|PAYLOAD:Z)` to guarantee cross-platform hash parity.
* **Phase 3: The Cryptographic Anchor (Verified)**
  * Integrated OpenSSL (`<openssl/sha.h>`) for true mathematical collision resistance.
  * **Status:** Successfully linked `-lcrypto` and verified zero "still reachable" or lost blocks in the Valgrind memory sandbox.

## Build Instructions

To compile the A-AST locally, ensure you have the OpenSSL development headers installed (`libssl-dev` on Debian/Ubuntu).

```bash
# Compile and link the cryptography library
gcc -Wall -Wextra -g main.c -o aast -lcrypto

# Execute through the memory sandbox
valgrind --leak-check=full ./aast
