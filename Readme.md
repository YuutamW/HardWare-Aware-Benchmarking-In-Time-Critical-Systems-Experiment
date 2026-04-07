# Data-Oriented Design vs. The Hardware: A Microbenchmarking Experiment

> *A practical experiment exploring why code that looks perfectly fast on a whiteboard can be incredibly slow in reality due to physical computer hardware limits.*

## The Premise: The Illusion of O(1)
In standard data structure curricula, **O(1)** complexity is often treated as the ultimate gold standard.However, in reality, not all O(1) algorithms are created equal.

In time-critical and real-time systems, proving an algorithm is mathematically deterministic is only half the battle. An algorithm can be theoretically perfect on a whiteboard, but if a system requires a strict execution window (e.g., under 300 milliseconds), that same algorithm can easily fail due to physical hardware bottlenecks, cache fragmentation, and pipeline stalls.

## The Inspiration
The premise for this experiment was sparked by looking out at a parking lot from my balcony one morning. Observing the license plates on the cars posed a highly practical systems design question: How would a time-critical system efficiently store, route, and access a massive database of vehicles using a 9-digit identifier? 
## The Objective
The goal of this experiment is straightforward: Design the fastest possible O(1) deterministic access architecture for a large dataset, bound strictly by the physical limitations of the hardware rather than just algorithmic theory.

## Methodology
Throughout this repository, I document a series of architectural approaches to this problem. Using Google Benchmark for software-level timing and Intel VTune Profiler for bare-metal hardware telemetry (running natively on an Intel CPU), I analyze exactly what the processor is doing under the hood.
For each approach, I will:
    1. Introduce the routing concept and its intended benefits.

    2. Break down the bare-metal execution and hardware penalties (the  "bumps" in the pipeline).

    3. Improve upon the architecture in the next iteration based purely on the physical profiling data.
