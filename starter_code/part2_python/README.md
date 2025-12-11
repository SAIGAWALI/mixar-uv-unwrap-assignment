# Part 2 – Python Batch Processor

This document explains the approach used to implement the Python-based UV unwrapping batch processor. Part 2 builds on the C++ unwrapping core from Part 1 and adds high-level automation, evaluation, and command-line tools.

## Overview

The Part 2 implementation consists of four major components:

* Python bindings to the C++ unwrapping library
* Quality metrics (stretch, coverage, angle distortion)
* A multi-threaded batch processing engine
* A command-line interface providing unwrap, batch, optimize, and analyze operations

The goal is to provide a complete pipeline for evaluating and optimizing UV unwrapping results across many meshes.

### 1. Python Bindings

The C++ library is accessed through a thin `ctypes` wrapper. The bindings load the compiled shared library, define the mesh and result structures in Python, and expose the unwrap function to Python callers.

The binding layer handles:

* Converting Python data into the C-compatible mesh format
* Passing pointers to the C++ unwrap function
* Receiving the generated UVs and result metadata
* Ensuring proper memory management by calling the corresponding free functions

This design keeps the C++ computation intact while making it accessible from Python workflows.

### 2. Quality Metrics

Three metrics are implemented based on the provided specification.

* **Stretch:** Computed using the Jacobian of the UV-to-3D mapping for each triangle. The singular values represent local scale change, and their ratio gives the stretch. Both average and maximum stretch values are reported.
* **Coverage:** UV space is rasterized into a fixed-resolution occupancy grid. The proportion of pixels covered by UV triangles represents how efficiently the layout uses the 0–1 texture domain.
* **Angle Distortion:** Angles of each 3D triangle are compared with angles of the corresponding UV triangle. The maximum angular deviation across all faces quantifies distortion introduced during parameterization.

These metrics collectively measure conformity, packing efficiency, and angular fidelity.

### 3. Multi-Threaded Batch Processor

The batch processor executes UV unwrapping on multiple meshes in parallel. It uses Python’s `ThreadPoolExecutor` to distribute work across available CPU cores.

The processing pipeline for each mesh consists of:

* Loading the mesh file
* Calling the C++ unwrap function via bindings
* Computing detailed UV quality metrics
* Saving the resulting UV-mapped mesh
* Saving per-mesh metric reports

The processor maintains thread-safe counters to track progress and optionally aggregates results into a single JSON report.

### 4. Command-Line Interface (CLI)

The CLI provides an easy way to run individual unwrapping tasks or entire processing pipelines. Commands include:

* **unwrap:** Runs the unwrapping algorithm on a single mesh and writes an output file.
* **batch:** Processes all meshes in a directory using multiple threads and produces a metrics report.
* **optimize:** Performs parameter search (such as angle thresholds) to minimize a chosen metric.
* **analyze:** Computes and displays quality metrics for an existing UV-mapped mesh.

The interface is designed to be simple, scriptable, and suitable for automated workflows.

## Approach Summary

Part 2 extends the C++ unwrapping engine into a full toolchain. The bindings allow Python to call the core algorithm. The metrics follow the exact mathematical definitions provided in the specification. The batch processor streamlines evaluation at scale through multi-threading. The CLI wraps everything in a clean command-based interface.

This modular approach keeps computationally heavy tasks in optimized C++ while using Python for flexibility, evaluation, and automation.