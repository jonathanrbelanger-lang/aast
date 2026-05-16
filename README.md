# A-AST (Accretive Abstract Syntax Tree)

An experimental, content-addressable abstract syntax tree written in C. 

Developed as an independent research initiative under Exorobourii LLC, the A-AST explores the concept of a "ledger of intent." It aims to offer an alternative to linear, string-based parsers (like YAML or HTML) by shifting structural validation from $O(N)$ regex parsing to an $O(1)$ cryptographic checksum. The goal is to build a structure optimized for deterministic LLM and agentic ingestion, prioritizing memory safety and clear context boundaries.

## The directionality overhead
Traditional data serialization often forces multidimensional relationships into flat strings. When downstream systems or AI agents ingest this data, they spend computational cycles parsing, validating, and reconstructing those relationships. 

The A-AST attempts to bypass this overhead. By organizing data as a Merkle DAG (directed acyclic graph), the cryptographic hashes inherently validate the structure upon ingestion.

## Core architectural principles

* **Immutability via accretion:** Nodes cannot be altered once created in memory. Modifying the tree requires accreting new nodes, which naturally preserves previous states.
* **Merkle DAG structure:** Each parent node's ID is a deterministic SHA-256 hash of its own attributes and the hashes of its children.
* **Projection separation:** The A-AST holds purely semantic data and relationships. Formats like Markdown, JSON, or HTML are treated as downstream projections (views) rather than the source of truth.
* **Cryptographic provenance (future work):** The canonical architecture can theoretically support signed nodes. By including an agent ID or public key in the pre-hashed buffer, the tree could establish a basic chain of custody, moving AI outputs toward a more auditable ledger format.

## Execution state & verification

The project is being developed in discrete phases:

* **Phase 1: Memory & structure**
  * Defined core C structs with manual heap management.
  * Implemented deep-copy payload ownership (`strdup`) and post-order traversal cleanup.
  * *Current state:* Passes Valgrind with zero memory leaks on baseline traversals.
* **Phase 2: Canonical serialization**
  * Implemented a C-level byte-packer to handle the non-determinism of high-level serializers.
  * Formats node data into a predictable string buffer `(type:X|children:Y|hash1,hash2|payload:Z)` to maintain hash parity.
* **Phase 3: Cryptographic anchor**
  * Integrated OpenSSL (`<openssl/sha.h>`) for SHA-256 hashing.
  * *Current state:* Successfully linked `-lcrypto` and verified clean memory handling during hash generation in the Valgrind sandbox.

## Build instructions

To compile the A-AST locally, ensure you have the OpenSSL development headers installed (e.g., `libssl-dev` on Debian/Ubuntu).

```bash
# Compile and link the cryptography library
gcc -Wall -Wextra -g main.c -o aast -lcrypto

# Execute through the memory sandbox
valgrind --leak-check=full ./aast
