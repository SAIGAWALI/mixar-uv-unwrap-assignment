# UV Unwrapping – Approach and Algorithms

This document explains the approach used to implement UV unwrapping in Part 1 of the assignment.

The solution follows the required pipeline:
1. Build mesh topology
2. Detect seams
3. Extract UV islands
4. Parameterize islands using LSCM
5. Pack islands into $[0,1]^2$
6. Compute basic quality metrics

Each stage is summarized below.

---

## 1. Topology Construction
The input mesh contains only vertex and face data. The first step is to construct full connectivity information so later stages can reason about adjacency.

### Approach
* For every triangle, enumerate its three edges.
* Canonicalize each edge as `(min(v0,v1), max(v0,v1))`.
* Insert edges into a hash map so duplicates are merged.
* For each edge, store the indices of adjacent faces.
* For vertices, build adjacency lists of edges and triangles.

**This gives:**
* A unique edge list
* Per-edge face connectivity
* Per-vertex edge connectivity
* A reliable Euler characteristic check

This structure is reused by seam detection and island extraction.

## 2. Seam Detection
Seam detection determines which mesh edges must be cut to convert a manifold surface into multiple topological disks suitable for flattening.

Two principles drive the approach:
1. Ensure each island is simply connected.
2. Place seams in high-curvature areas to reduce distortion.

### Dual Graph + Spanning Tree
* Build a dual graph where nodes are faces and edges represent shared mesh edges.
* Perform a BFS spanning tree starting from face 0.
* Any interior mesh edge **not** used in the spanning tree is marked as a seam candidate.

**Reasoning:** Removing all non-tree dual edges guarantees the mesh becomes cut into a disk-like structure.

### Angular Defect Refinement
For each vertex:
1. Sum all angles around that vertex.
2. Compute defect:
   $$\text{defect} = 2\pi - \text{sum}$$
3. If the defect exceeds a threshold, mark all edges incident to that vertex as additional seams.

This places seams along areas of high curvature (e.g., cube corners), improving flattening quality.

### Result
The final seam set is the union of:
* Non-tree interior edges
* Edges incident to high-defect vertices

## 3. UV Island Extraction
Once seams are identified, the mesh is cut along these edges, resulting in multiple connected components.

### Approach
1. Rebuild dual adjacency, but exclude edges that correspond to seams.
2. Use BFS/DFS to find connected face groups.
3. Assign an island ID to every face.

**This yields:**
* `num_islands`
* `face_island_ids[i]` for every triangle

Each island is then parameterized independently.

## 4. LSCM Parameterization
Each island is flattened into 2D using **Least-Squares Conformal Maps (LSCM)**. The goal is to preserve angles as much as possible.

### Approach
1. For every triangle, compute its 3D edges and assemble LSCM linear energy terms.
2. Build a sparse linear system $Ax=b$.
3. Fix two vertices (one at $(0,0)$ and another at $(1,0)$) to remove rigid-motion ambiguity.
4. Solve using Eigen’s sparse solver.
5. Write the resulting $(u,v)$ back into the mesh’s UV array.

Each island ends up with UVs in its own local coordinate space.

## 5. Island Packing
The goal is to arrange all islands inside the $[0,1]^2$ square without overlaps.

### Approach
* **Bounding Boxes:** For each island, compute its min/max U and V. Store width and height.
* **Sorting:** Sort islands by height (descending). Larger islands are placed first to improve packing efficiency.
* **Shelf Packing:**
    * Maintain a running shelf height.
    * Place islands left-to-right until no space remains.
    * Start a new shelf below.
    * For each island, compute its target translation.
    * Apply offsets to all UVs of that island.
* **Scaling:** Compute the bounding rectangle of all packed UVs. Uniformly scale all UVs so they fit inside $[0,1]^2$ with the requested margin.

This basic method satisfies the requirement for >60% coverage.

## 6. Full Unwrapping Pipeline
The function `unwrap_mesh` orchestrates the entire workflow:

1. Construct topology
2. Detect seams
3. Extract islands
4. Compute LSCM for each island
5. Pack islands
6. Calculate metrics
7. Return a new mesh with UVs attached

This completes the required functionality for Part 1.