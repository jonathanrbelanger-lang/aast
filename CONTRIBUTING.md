# Contributing to the A-AST

Thank you for your interest in the Accretive-Abstract-State-Tree (A-AST). 

This project is an experimental data structure built to solve a highly specific problem: eliminating the O(N) parsing tax and mutability risks for agentic LLM workflows. Because the A-AST operates as a "ledger of intent," it relies on strict cryptographic and memory-safety guarantees.

To protect the integrity of the architecture and respect everyone's time, please review these guidelines before opening an issue or submitting a pull request.

---

## 1. Project nature and expectations

A-AST is developed and maintained as an independent research initiative under Exorobourii LLC, focusing on architecture efficiency and language models. 
* **This is a pure research project.** There are currently no bounties, consulting contracts, or paid positions available. 
* Contributions are accepted strictly on an open-source, academic, and collaborative basis. 

## 2. The engineering standard

We are building for a strict constraint envelope. Convenience features that violate the design contract will not be merged. Before proposing a solution, ensure it aligns with the [A-AST Design Philosophy](DESIGN.md).

* **Zero-Allocation Focus:** For critical path operations (like ingestion validation), logic must operate entirely in-place or on the stack. 
* **No External Bloat:** Do not submit PRs that link against large external libraries (e.g., `libicu` for string handling) to solve complex problems. We are prioritizing O(1) operational focus and minimal inherited abstraction. 
* **Empirical Over Theoretical:** If you are proposing a fix for structural limitations or memory envelopes, your proposal must be backed by empirical measurement. 

## 3. Code style and security

Given the cryptographic and memory-managed nature of the A-AST, security and predictability are our highest priorities. We operate with a zero-trust mindset regarding memory manipulation.

* **"Dumb" C is better than "Clever" C:** Code must be highly readable. Obfuscated pointer arithmetic, complex macro magic, and overly dense one-liners will be rejected. If it takes more than two minutes to decipher a memory operation, it needs to be rewritten.
* **Valgrind requirement:** No code will be merged unless it passes strict memory profiling. You are expected to provide the output of `valgrind --leak-check=full` demonstrating zero leaks and zero out-of-bounds reads under maximum-fill node conditions.
* **Core operations:** Pull Requests that touch the core `Node` struct, the OpenSSL EVP hash computations, or the reference counting engine (`aast_retain` / `aast_release`) are subject to extreme scrutiny. In many cases, accepted architectural solutions in these areas will be treated as pseudocode and manually integrated by the maintainers to ensure cryptographic chain integrity.

## 4. How to contribute

To prevent wasted effort, please follow this workflow:

1. **Check the Issue Tracker:** Look for open Request for Comments (RFCs). These are the active, unsolved architectural problems we are currently focused on.
2. **Open a Discussion first:** If you have a feature idea or an optimization, open an issue detailing the physics of the problem and your proposed constraints *before* writing code.
3. **Draft the PR:** Keep pull requests narrowly scoped. A PR should do exactly one thing.
4. **Provide proof:** Include your benchmarking results and Valgrind outputs directly in the PR description.

We appreciate the time and rigor of developers willing to engage with these constraints.
