<div align="center">

# ⚡ Nakamoto Core

### A Low-Latency Crypto Perpetual Futures Execution Engine built in Modern C++20

![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)
![Ubuntu](https://img.shields.io/badge/Ubuntu-24.04-E95420?logo=ubuntu)
![Linux](https://img.shields.io/badge/Linux-WSL2-yellow?logo=linux)
![Build](https://img.shields.io/badge/CMake-3.20+-064F8C?logo=cmake)
![Compiler](https://img.shields.io/badge/GCC-13-blue?logo=gnu)
![Status](https://img.shields.io/badge/Status-In%20Development-orange)
![License](https://img.shields.io/badge/License-AGPL--3.0-green)

---

**COEP Quant Finance Club • Low-Level Programming & HFT Initiative**

Building the fastest retail-accessible crypto execution engine without exchange colocation.

</div>

---

# Vision

Nakamoto Core is a research-oriented engineering project focused on designing and implementing a modern low-latency execution engine for cryptocurrency perpetual futures.

Unlike most retail trading bots, the project focuses primarily on **software engineering**, **systems programming**, and **performance optimization**.

The long-term objective is to understand and optimize every stage between receiving market data and sending an order.

---

# Project Goals

- Build a production-quality C++20 execution engine
- Minimize end-to-end software latency
- Benchmark every subsystem
- Understand Linux systems programming
- Learn modern low-latency software architecture
- Support paper trading before real capital deployment
- Eventually deploy on a Linux VPS

---

# Project Philosophy

Measure first.

Optimize second.

Never optimize blindly.

Every optimization must be backed by benchmarks.

---

# Long-Term Roadmap

## Phase 1 — Infrastructure

- [x] Linux Development Environment
- [x] Modern CMake
- [x] High Resolution Timer
- [x] Benchmark Framework v1
- [ ] Logging System
- [ ] Configuration System

---

## Phase 2 — Networking

- [x] TCP Socket
- [x] TLS
- [x] WebSocket Client
- [ ] Exchange Connectivity

---

## Phase 3 — Market Data

- [x] Live Market Data
- [ ] Binary / JSON Parser
- [ ] Order Book
- [ ] Local Market State

---

## Phase 4 — Execution

- [ ] Execution Engine
- [ ] Risk Checks
- [ ] Paper Trading
- [ ] Live Trading

---

## Phase 5 — Optimization

- [ ] CPU Affinity
- [ ] Lock-Free Queues
- [ ] Memory Pool
- [ ] Cache Optimization
- [ ] Latency Profiling
- [ ] End-to-End Benchmarking

---

# Current Architecture

```text
Trader

│

├── Latency
│   ├── HighResolutionTimer
│   └── Benchmark

├── Networking
│
├── Market Data
│
├── Order Book
│
├── Execution
│
└── Risk
```

---

# Development Stack

| Component | Technology |
|-----------|------------|
| Language | C++20 |
| Build System | CMake |
| Compiler | GCC 13 |
| Platform | Ubuntu 24.04 |
| Environment | WSL2 |
| IDE | VS Code Remote |
| Version Control | Git + GitHub |

---

# Current Status

🚧 Early Development

The project is currently focused on building the software infrastructure required for a modern low-latency execution engine.

No live trading functionality has been implemented yet.

---

# License

AGPL-3.0
