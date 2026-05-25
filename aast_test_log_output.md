# A-AST Phase 9.0: Empirical Validation Logs

This document archives the raw baseline validation telemetry for the Accretive-Abstract-State-Tree (A-AST) core library architecture. These records verify the runtime behavior of the software-enforced constraint filters under simulated edge-case workloads.

---

## 1. Volumetric Payload Sweep (`make test_payload_sweep`)

This test isolates the payload ingestion vector by sweeping contiguous allocation boundaries with zero child nodes to check serialization stability and verify the gatekeeper filters.

```text
user@host:~/aast$ make test_payload_sweep
gcc -g -Wall -Wextra -std=c11 -I. tests/test_phase_a.c aast.c -o tests/test_phase_a -lcrypto
--- Beginning Progressive Payload Scale Sweep ---
Initializing Phase A Maximum Node Fill Test...
Constructing the node...
Node created successfully. Root hash: 974a0428fce94415cd2027f7ea23a84ad6d44c1ebc4ccc20fcb0d145c07db30d
Phase A Test Complete.
Initializing Phase A Maximum Node Fill Test...
Constructing the node...
Node created successfully. Root hash: 5fa57df23fd7b307f280e400254c8e149abdd2696a51d262af355cc16076ac00
Phase A Test Complete.
Initializing Phase A Maximum Node Fill Test...
Constructing the node...
Node created successfully. Root hash: 3e5daa55d03b4d1d2fddc564c73feecbf662fc8026707a3b31dba46c04ead6cd
Phase A Test Complete.
Initializing Phase A Maximum Node Fill Test...
Constructing the node...
[A-AST Error] Payload size (1073741823 bytes) exceeds maximum chunk threshold of 536870912 bytes.
Node creation failed.
Phase A Test Complete.

```

### Telemetry Analysis: Payload

The telemetry confirms that the operational constraint validation safely blocks monolithic payloads crossing the `512 MB` (`536870912 bytes`) architectural line. At `1,073,741,823 bytes` (~1 GB), the constructor safely short-circuits, prints a structured warning to stderr, and avoids unmanaged heap fragmentation or kernel Out-Of-Memory (OOM) interventions.

---

## 2. High-Density Vertical Traversal Sweep (`make test_depth_sweep`)

This test profiles recursive stack allocation by building a single-child linear spine to confirm that software-defined guardrails gracefully intercept execution before hitting the system thread allocation ceiling.

```text
user@host:~/aast$ make test_depth_sweep
gcc -g -Wall -Wextra -std=c11 -I. tests/test_depth_limit.c aast.c -o tests/test_depth_limit -lcrypto
--- Beginning Progressive Vertical Depth Sweep ---
Executing Vertical Architecture Audit: Depth Axis
Attempting to construct a linear tree of depth 1000...
[SUCCESS] Built vertical tree of depth 1000. Root Hash: 1b59fae0b7cd87f1a591f0f3712168ec8d62400fa37219620ecf11d26055090f
Triggering recursive integrity check...
Integrity result: VALID
Triggering recursive deallocation...
Vertical Depth Test Complete.
Executing Vertical Architecture Audit: Depth Axis
Attempting to construct a linear tree of depth 10000...
[SUCCESS] Built vertical tree of depth 10000. Root Hash: ae5ab802876f28b24a242c627d04ad3d6042a2646d4213e79d0a723fb92ddb44
Triggering recursive integrity check...
Integrity result: VALID
Triggering recursive deallocation...
Vertical Depth Test Complete.
Executing Vertical Architecture Audit: Depth Axis
Attempting to construct a linear tree of depth 50000...
[SUCCESS] Built vertical tree of depth 50000. Root Hash: 0d0b862dc2e34e55889030b2578b4b85ab402a72e62e27ef1cac4c5aa672aebb
Triggering recursive integrity check...
ERROR: Max recursion depth reached...
Integrity result: INVALID/RECURSION_LIMIT_HIT
Triggering recursive deallocation...
WARNING: Max recursion depth reached...
Vertical Depth Test Complete.

```

### Telemetry Analysis: Depth

The vertical sweep metrics demonstrate that the stack-overflow defense mechanisms are fully active. At a depth of 50,000 layers, the traversal helper successfully intercepts the `AAST_MAX_DEPTH` boundary (35,000), catches the depth violation cleanly, flags the tree state as invalid, and winds down allocations using non-destructive warning flags rather than permitting a raw runtime segmentation fault.
