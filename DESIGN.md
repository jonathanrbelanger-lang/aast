# A-AST Design Philosophy and Research Program

## Who This Document Is For

This document exists for contributors who want to understand not just what the
A-AST does, but why every significant decision was made the way it was, and
what demands the project makes on anyone who wants to extend it.

The project prioritizes long-term architectural stability over rapid feature accumulation. 
Because cryptographic guarantees leave no room for approximation, design decisions are made 
deliberately, and structural changes are deferred until their downstream impacts are fully 
quantified. Core design contracts are strictly maintained; convenience features that introduce 
boundary violations or hidden overhead are stepped back, ensuring the core remains optimized 
solely for its intended workload. Open problems are documented transparently rather than 
obscured behind speculative implementations. This measured pace reflects the weight of the 
immutability promises the library makes.

For engineers interested in systems programming, low-level memory management, or content-addressable 
architectures, this documentation and the accompanying codebase are designed to serve as a clear, 
verifiable map of the terrain. Reviewing these foundational principles before writing code ensures 
that all contributions align smoothly with the library’s design constraints.

To maintain the rigorous standards established throughout the codebase, contributions are evaluated 
based on technical depth, mathematical consistency, and empirical backing. The boundary lines 
enforced during code review are a reflection of architectural necessity, ensuring the structural 
integrity of the data structure remains protected as the ecosystem evolves.

---

## 1. The Problem Statement

### The Consumer Has Changed

For most of the history of structured data formats, the primary consumer was a
human being. JSON, YAML, HTML, XML — these formats are legible to a person
with a text editor. That legibility is not incidental. It is a deliberate
design choice, load-bearing in the architecture of every format in widespread
use today. Human readability shaped delimiter choices, whitespace handling,
comment syntax, and the entire philosophy of what a "well-formed" document
means.

That assumption is now outdated in a growing class of applications. AI agents
are increasingly the primary consumer of structured data at scale. They do not
benefit from human-readable formatting. They pay for it.

Agent inference costs are now a known budget line item for any organization
deploying at scale. Every token an agent spends parsing structure that exists
for human convenience is a token that costs money and produces no analytical
output. At scale, this is not a rounding error. It is a compounding liability.
Current approaches to this problem are scattershot — optimizations applied
after the fact to formats that were never designed for this consumer. The
overhead is structural, not incidental, and it cannot be fully optimized away
without replacing the format itself.

The A-AST is an attempt to ask what a data structure looks like when it is
designed for the agent consumer from the beginning, with human readability as
a secondary concern rather than a primary constraint.

### Four Failure Modes of Existing Approaches

**Mutability.** Most common data formats permit modification after the fact.
A JSON document can be edited. A YAML file can be overwritten. There is no
native mechanism that guarantees the data an agent reads today is identical to
the data that was written. For applications where the integrity of historical
state matters — audit logs, verifiable state transitions, content-addressable
knowledge bases — this is a foundational problem, not an edge case.

**Post-hoc verification.** Where verification exists in current formats, it
operates after the data is already embedded in the pipeline. A checksum
applied to a file tells you whether the file has been corrupted since the
checksum was computed. It does not tell you whether the data was correct when
it was written, and it does not propagate — verifying a container tells you
nothing about the integrity of its contents without a separate mechanism for
each layer. For an agent that needs to trust its own working memory, this is
an insufficient guarantee.

**Inherited abstraction overhead.** Every format built at an abstraction layer
above the metal carries design assumptions that its consumers inherit without
recourse. A pipeline built on JSON inherits JSON's type system, its whitespace
handling, its unicode escaping rules, its parser ambiguities. These are not
neutral choices. They are a tax paid continuously at runtime, and the consumer
has no mechanism to reduce it without abandoning the format entirely. The
overhead is not a bug in the implementation. It is a property of the design.

**O(N) complexity at the format floor.** Human-readable formats require
parsing to be useful. Parsing is O(N) in the size of the document, at minimum,
before any application logic runs. This is the floor of the format itself —
not the floor of the machinery reading it. An agent that must parse a document
before it can verify or query it is paying a tax that begins before it has
done any useful work. The analogy is apt: complex carbohydrates must be fully
broken down before the body can use them. The digestion cost is real and it is
paid every time.

### The Token Cost Framing

These failure modes compound in an agentic deployment. An agent that cannot
verify its own memory without re-parsing it, that operates on mutable state it
cannot audit, and that pays O(N) parsing overhead on every read, is an agent
whose operational costs are structurally higher than they need to be. The
existing tooling that addresses this problem — and some of it is efficient and
well-designed — was built for different consumers and different problem spaces.
It was not designed for this workflow, and while adapting it is possible, it is
not native to the system in question.

---

## 2. Design Philosophy

### Immutability by Construction

Nodes in the A-AST cannot be altered once created. This is not a convention or
a guideline. It is enforced by the architecture. Modifying the state of the
tree requires accreting a new state — constructing new nodes along the modified
path while reusing unmodified branches via structural sharing. The previous
state is preserved by construction. There is no mechanism for in-place mutation 
within this architecture, as maintaining mutable nodes would invalidate the 
node's hash, break the Merkle chain, and compromise the determinism of the 
entire system. Consequently, immutability is treated as a foundational 
constraint from which the rest of the framework is derived.

This is the foundational constraint from which everything else follows. A
mutable node would invalidate its hash, break the Merkle chain, and destroy
the determinism of the system. Immutability is a binding contract, in the
greater context of the project.

### Cryptographic Provenance at Write Time

Each node's identity is a SHA-256 hash of its content and the hashes of its
children, computed at the moment of creation. Verification is not a separate
step applied after the fact — it is intrinsic to the structure. A parent
node's hash encodes the hashes of all its children. Any modification to any
node at any depth in the tree cascades upward, changing every ancestor hash
up to the root. Verifying the root hash verifies the entire tree. This is the
Merkle property, and it is the reason O(1) integrity verification is possible.

The implication is that the canonical buffer from which the hash is computed
must be strictly deterministic. Identical tree structures must produce identical
hashes regardless of construction order, ingestion sequence, or system
environment. This determinism is enforced by lexicographic sorting of children
at node creation time, and it is not surfaced as a configurable parameter by
design.

### Agent-First, Human-Readable Second

The A-AST is being optimized for machine ingestion. Human readability is a 
secondary concern, accommodated through a conditionally compiled debug 
utility that is off by default and adds zero overhead to a production build. 
This is not an aesthetic choice, but rather a direct expression of the 
problem statement. Every byte of formatting included for human convenience 
introduces structural overhead that does not serve the agent consumer, 
running counter to the project's optimization goals. To preserve a minimal 
core footprint, the library focuses strictly on data structure integrity, 
leaving the implementation of human-readable presentation layers to 
downstream application developers.

Organizations deploying the A-AST are responsible for their own human-readable
output layer. The library provides the structure, and the design ditates that
we push these downstream tasks to them. 

### Minimal Overhead as a First-Class Constraint

Reducing overhead is not an optimization target to be addressed after the
system is working, it is a design constraint applied from the beginning. The
transition from O(N) child arrays to O(1) hash table lookups in Phase 8.5 was
not just a performance improvement, it was a requirement for a library whose 
purpose includes efficient querying at scale.

This constraint has implications for contributors. The baseline overhead 
contract is zero-tolerance. Any proposed convenience features must be 
strictly opt-in, conditionally compiled, and demonstrate zero impact on 
the baseline execution path. The library’s contract remains intentionally 
minimal and focused. To protect the system against feature and scope creep, 
proposals that expand the API boundary are evaluated based on their alignment 
with the core design philosophy rather than immediate, isolated use cases. 

### Determinism as a Promise

Given the same input, the A-AST will always produce the same output. This is
not a property that can be partially relaxed for specific use cases. It is the
foundation of the library's utility as a verifiable state layer. A design
decision that introduces non-determinism — in hash computation, in
serialization order, in traversal behavior — is incompatible with the project
regardless of its other merits.

---

## 3. The Contract Boundary

The A-AST has a narrow contract. The decisions below describe where that
boundary sits and the reasoning behind each position.

### What the Library Does

- Constructs immutable, cryptographically anchored nodes from well-formed input
- Enforces deterministic hash computation across all operations
- Provides O(1) child lookup via the hash table architecture
- Serializes and deserializes the tree with zero data loss
- Verifies tree integrity via the Merkle property
- Enforces the UTF-8 NFC encoding contract at the ingestion boundary
- Manages memory safely via reference counting, with zero leaks under all
  tested conditions

### The Contract Boundary in Detail

**Encoding normalization.** The library's ingestion boundary validates input
against the UTF-8 NFC encoding contract and rejects non-conforming data. This
boundary is where the library's encoding responsibility ends. A separate,
optional re-encoding utility is planned for pipelines that need to normalize
input before ingestion — callable, off by default, and independent of the core
library. Pipelines that have already handled encoding upstream have no need for
it.

**Human-readable output.** The design philosophy established in section 2
places human readability as a secondary concern, expressed through a
conditionally compiled debug utility that is off by default. This reflects the
agent-first architecture, not an omission. Organizations that need
human-readable representations of A-AST data have the traversal primitives
available to build that layer for their specific context.

**Data hygiene.** The A-AST validates structure and encoding at the ingestion
boundary. It does not validate the semantic correctness of payload data. Data
hygiene at ingestion is vital to the core promise of the A-AST — without
deliberate decision making at this stage, the output will reflect the input.
The hash certifies the state of the data at write time, whatever that state
is. That is the Merkle property working as designed.

**Tokenization.** Path traversal functions accept pre-tokenized arrays of key
strings. The boundary between parsing and traversal is intentional — encoding
any tokenization convention into the core traversal functions would impose a
silent contract on every downstream consumer of the library. 

This boundary is intentional. Building a tokenization convention into the core 
traversal functions would impose a silent, inescapable contract on every 
downstream consumer.

A standalone tokenization utility for C consumers who need it is a candidate 
for future work, explicitly named and optional. For Python consumers, 
tokenization belongs in the Cython bridge, where string handling is natural 
and changes to the parsing strategy require no modification to the C ABI.

---

## 4. Known Constraints and Open Design Problems

The following sections outline known architectural constraints and open 
design problems. These challenges require structural, mathematically sound 
resolutions backed by empirical measurement. Documenting these open frontiers 
clearly ensures that future development addresses root causes rather than 
superficial symptoms.

### The Code Payload Problem

The hash is computed over bytes, not semantics. A payload containing source
code is, from the library's perspective, an opaque byte sequence. The hash
does not care what the payload means.

The problem is the ingestion pipeline. The parser that constructs nodes from
input must correctly identify where a payload begins and ends. A payload
containing source code may contain characters — braces, brackets, newlines,
key-value separators — that the parser would otherwise treat as structural
delimiters. A misread boundary risks producing a malformed node whose 
cryptographic hash is internally consistent but structurally unfaithful to 
the source text. In this scenario, the Merkle integrity check would pass 
despite the underlying data corruption. Current analysis suggests this is 
an ingestion boundary detection challenge rather than a vulnerability in the 
hashing context itself. The leading candidate solution is a wrapping mechanism 
that signals to the parser that a payload should be treated as opaque. This
introduces known tradeoffs: a potential memory leak if a node is emptied by a
misread wrap boundary, and a layer of complexity at the root that propagates
through every subsequent operation.

Whether this is a payload-level concern or an ingestion-level concern is not
yet determined. The hash promise — that the canonical buffer faithfully
represents the node — cannot be weakened under any resolution of this problem 
without eroding the core premise and design philosophy.

### The Encoding Boundary

UTF-8 NFC is intended as the chosen encoding. Future work involvesd explicit
functionality to build out the validate -> [reject, accept] logic to ensure
this. It has been chosen for its adoption within the greater ML ecosystem, 
to reduce friction points. There is not yet a design or plan for other encoding
schemas, though it is open for discussion in terms of externalized added 
functionality.

### The Doc-Break Problem

When a tree approaches its depth or complexity ceiling, it must be split into
a parent tree and one or more child trees. The split must occur at a clean
structural boundary — a point in the input artifact that does not bisect a
node mid-construction. Identifying that boundary requires understanding the
valid structural boundaries of the artifact being ingested.

For simple structured text, this is tractable. For artifacts containing code
payloads, deeply nested structures, or non-uniform node fill, it is the same
problem as the code payload problem viewed from a different angle. Both are
asking: where are the valid structural boundaries in this artifact? Neither can
be fully resolved until the other is.

The doc-break problem cannot be designed before the constraint envelope mapping
experiment is complete. The split threshold depends on knowing where the
ceiling is.

---

## 5. Planned Experiments

Designing branching logic before the operational ceiling is measured is 
designing against an assumption. The following experiments exist to 
establish the physical limits of the structure before that logic is written. 
The experiments in this section exist to replace assumptions with measurements.

### Phase A: Maximum Node Fill

Determine the absolute maximum size of a single node by filling all internal
parameters to their practical limits simultaneously:

- Maximum payload length
- Maximum key length
- Maximum type string length
- Maximum child count per node

This establishes the worst-case per-node memory footprint and stack frame cost,
which is the unit of measurement for Phase B. No combinatorial analysis is
meaningful without this baseline.

### Phase B: Combinatorial Ceiling Mapping

Construct trees populated entirely with maximum-fill nodes and sweep across
depth and width combinations. Record the point at which each combination
approaches structural failure. The output is not a single number. It is a
curve — a constraint envelope across the depth and width space that defines
where the library actually breaks under worst-case conditions.

This curve replaces the current AAST_MAX_DEPTH constant, which was established
empirically as a safe guardrail but is acknowledged as arbitrary relative to
the true structural limits. The 20% operational buffer below the curve at any
given operating point will define the split threshold for the partitioning
logic. The buffer is defined relative to the curve, not as a fixed constant,
so it scales correctly as tree shape varies.

These experiments must complete before the partitioning design is finalized.
The branching logic will be derived from the measurements, not the other way
around.

---

## 6. Roadmap and Downstream Vision

### Phase 9: Query API

The immediate next build phase. Provides read-only traversal and discovery
functions that allow an agent to explore the tree without prior knowledge of
its structure. Key functions:

- `aast_find_child_by_key` — O(1) single-level lookup, non-owning pointer contract
- `aast_query_path` — pre-tokenized array traversal, pure iteration over
  `aast_find_child_by_key`, no parsing logic
- `aast_query_by_type` — full tree traversal collecting nodes by semantic type

The non-owning pointer contract established here governs all query functions.
Callers who need to preserve a queried node beyond the lifecycle of its parent
tree must explicitly call `aast_retain`. This is a memory safety requirement,
not a convenience.

### Phase 10: Projections API

A renderer pattern for exporting A-AST data to external formats. The export
logic lives outside the core library. The core library provides the traversal
primitives. This is the layer where human-readable output can be constructed
by consumers who need it, without that capability becoming a core library
concern.

### Phase 11: Python Bridge

A Cython extension module wrapping the C library for Python consumers. The
Pointer Factory Paradigm governs ownership reconciliation at the language
boundary — every non-owning C pointer that crosses into Python space is
promoted to an owning pointer via an explicit `aast_retain` call before
encapsulation. The inherently acyclic structure of the A-AST makes Python's
cyclic garbage collector a non-issue. Dot-notation path parsing lives in the
Cython layer, not in the C library.

### Lookahead and Partitioning

When a tree approaches its operational threshold — defined as 20% below the
constraint envelope curve — the library will look ahead in the input artifact
for the next clean structural break, flag the partition point, insert the child
tree root hash as a value in the parent tree, and initialize a new tree from
the point immediately following. The join is encoded in the structure. No
external registry. No caller-managed ordering. The parent tree's hash proves
which child tree belongs at which position.

This cannot be designed before the constraint envelope experiments complete and
the code payload and doc-break problems are resolved. The dependency is
explicit and intentional.

### Re-encoding Utility

A separate, callable, off-by-default tool for converting non-UTF-8-NFC input
to the library's declared encoding standard before ingestion. Not part of the
core library. Not inline. Pipelines that have already handled encoding upstream
ignore it entirely.

### The Broader Vision

The A-AST is a prototype for a class of data structure that does not yet exist
in production tooling — a verifiable, minimal-overhead state layer designed
from the beginning for agentic consumers. An agent operating on an A-AST can
verify the integrity of its entire working memory in O(1) time. It can audit
its own state transitions cryptographically. It can operate within a
token-budget-conscious envelope by design rather than by post-hoc optimization.

Managing the compounding token costs of agentic deployment at scale requires a 
fundamental re-evaluation of data format design. Rather than applying post-hoc 
optimizations to formats originally built for human legibility, the A-AST 
demonstrates the viability of data architectures engineered exclusively for 
machine execution—developed deliberately, one verifiable phase at a time.
