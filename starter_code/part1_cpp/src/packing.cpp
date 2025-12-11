/**
 * @file packing.cpp
 * @brief UV island packing into [0,1]² texture space
 *
 * SKELETON - YOU IMPLEMENT THIS
 *
 * Algorithm: Shelf packing
 * 1. Compute bounding box for each island
 * 2. Sort islands by height (descending)
 * 3. Pack using shelf algorithm
 * 4. Scale to fit [0,1]²
 */

#include "unwrap.h"
#include "math_utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <vector>
#include <algorithm>
#include <set>

/**
 * @brief Island bounding box info
 */
struct Island {
    int id;
    float min_u, max_u, min_v, max_v;
    float width, height;
    float target_x, target_y;  // Packed position (before final global scale)
    std::vector<int> vertex_indices;
};

void pack_uv_islands(Mesh* mesh,
                     const UnwrapResult* result,
                     float margin) {
    if (!mesh || !result || !mesh->uvs) return;

    if (result->num_islands <= 1) {
        // Single island, already normalized to [0,1]
        return;
    }

    printf("Packing %d islands...\n", result->num_islands);

    int F = mesh->num_triangles;
    const int* tris = mesh->triangles;
    const int* face_island = result->face_island_ids;
    int num_islands = result->num_islands;
    int V = mesh->num_vertices;

    // Create containers per island: we'll collect unique vertex indices per island
    std::vector<std::set<int>> island_vertex_sets(num_islands);

    // Build island vertex sets by iterating faces
    for (int f = 0; f < F; ++f) {
        int iid = 0;
        if (face_island) iid = face_island[f];
        if (iid < 0 || iid >= num_islands) continue;
        int a = tris[3*f + 0];
        int b = tris[3*f + 1];
        int c = tris[3*f + 2];
        island_vertex_sets[iid].insert(a);
        island_vertex_sets[iid].insert(b);
        island_vertex_sets[iid].insert(c);
    }

    // Build Island structs
    std::vector<Island> islands;
    islands.reserve(num_islands);

    for (int iid = 0; iid < num_islands; ++iid) {
        Island I;
        I.id = iid;
        I.min_u = FLT_MAX;
        I.max_u = -FLT_MAX;
        I.min_v = FLT_MAX;
        I.max_v = -FLT_MAX;
        I.target_x = 0.0f;
        I.target_y = 0.0f;
        I.vertex_indices.reserve(island_vertex_sets[iid].size());

        // convert set -> vector of indices and compute bbox
        for (int vid : island_vertex_sets[iid]) {
            if (vid < 0 || vid >= V) continue;
            I.vertex_indices.push_back(vid);
            float u = mesh->uvs[2*vid + 0];
            float v = mesh->uvs[2*vid + 1];
            if (u < I.min_u) I.min_u = u;
            if (u > I.max_u) I.max_u = u;
            if (v < I.min_v) I.min_v = v;
            if (v > I.max_v) I.max_v = v;
        }

        // Handle empty / degenerate island
        if (I.vertex_indices.empty()) {
            I.min_u = I.min_v = 0.0f;
            I.max_u = I.max_v = 0.0f;
        }

        I.width = I.max_u - I.min_u;
        I.height = I.max_v - I.min_v;

        // Avoid zero-sized dims
        if (I.width < 1e-6f) I.width = 1e-6f;
        if (I.height < 1e-6f) I.height = 1e-6f;

        islands.push_back(std::move(I));
    }

    // STEP 2: Sort by height (descending), break ties by width
    std::sort(islands.begin(), islands.end(), [](const Island& A, const Island& B){
        if (A.height == B.height) return A.width > B.width;
        return A.height > B.height;
    });

    // STEP 3: Shelf packing
    // We'll pack in the unit square but allow final scaling step.
    // margin is interpreted in UV units (0..1).
    float usable_width = 1.0f - 2.0f * margin;
    if (usable_width <= 0.0f) usable_width = 1.0f;

    float cur_x = margin;
    float cur_y = margin;
    float shelf_h = 0.0f;

    float packed_max_x = 0.0f;
    float packed_max_y = 0.0f;

    for (auto &I : islands) {
        // island footprint including internal margin between islands
        float footprint_w = I.width + margin;
        float footprint_h = I.height + margin;

        // If it doesn't fit in current shelf (and it's not the first item), start new shelf
        if (cur_x + footprint_w > 1.0f - margin && cur_x > margin) {
            // finalize shelf
            if (cur_x > packed_max_x) packed_max_x = cur_x;
            cur_x = margin;
            cur_y += shelf_h + margin;
            shelf_h = 0.0f;
        }

        // place island at current cursor (top-left style)
        I.target_x = cur_x;
        I.target_y = cur_y;

        // advance cursor
        cur_x += footprint_w;
        if (footprint_h > shelf_h) shelf_h = footprint_h;
    }

    // finalize extents
    if (cur_x > packed_max_x) packed_max_x = cur_x;
    packed_max_y = cur_y + shelf_h;

    // ensure positive
    if (packed_max_x < 1e-6f) packed_max_x = 1.0f;
    if (packed_max_y < 1e-6f) packed_max_y = 1.0f;

    // STEP 4: Move islands to packed positions
    for (const Island &I : islands) {
        for (int vid : I.vertex_indices) {
            // compute offset from island min to vertex
            float u = mesh->uvs[2*vid + 0];
            float v = mesh->uvs[2*vid + 1];

            float du = u - I.min_u;
            float dv = v - I.min_v;

            // new (unscaled) position
            float new_u = I.target_x + du;
            float new_v = I.target_y + dv;

            mesh->uvs[2*vid + 0] = new_u;
            mesh->uvs[2*vid + 1] = new_v;
        }
    }

    // STEP 5: Scale everything to fit [0,1]^2
    float pack_min_u = FLT_MAX, pack_max_u = -FLT_MAX;
    float pack_min_v = FLT_MAX, pack_max_v = -FLT_MAX;

    for (int vi = 0; vi < V; ++vi) {
        float u = mesh->uvs[2*vi + 0];
        float v = mesh->uvs[2*vi + 1];
        if (u < pack_min_u) pack_min_u = u;
        if (u > pack_max_u) pack_max_u = u;
        if (v < pack_min_v) pack_min_v = v;
        if (v > pack_max_v) pack_max_v = v;
    }

    if (pack_min_u == FLT_MAX) {
        // nothing was packed; bail
        printf("  Packing: nothing packed (no UVs?)\n");
        return;
    }

    float packed_w = pack_max_u - pack_min_u;
    float packed_h = pack_max_v - pack_min_v;

    if (packed_w < 1e-6f) packed_w = 1.0f;
    if (packed_h < 1e-6f) packed_h = 1.0f;

    float scale = 1.0f / std::max(packed_w, packed_h);

    // After scaling, translate so min maps to 0
    for (int vi = 0; vi < V; ++vi) {
        float u = mesh->uvs[2*vi + 0];
        float v = mesh->uvs[2*vi + 1];

        float su = (u - pack_min_u) * scale;
        float sv = (v - pack_min_v) * scale;

        mesh->uvs[2*vi + 0] = su;
        mesh->uvs[2*vi + 1] = sv;
    }

    printf("  Packing completed (packed_w=%.4f packed_h=%.4f scale=%.4f)\n",
           packed_w, packed_h, scale);
}

void compute_quality_metrics(const Mesh* mesh, UnwrapResult* result) {
    if (!mesh || !result || !mesh->uvs) return;

    // Default values (replace with your implementation)
    result->avg_stretch = 1.0f;
    result->max_stretch = 1.0f;
    result->coverage = 0.7f;

    printf("Quality metrics: (using defaults - implement for accurate values)\n");
    printf("  Avg stretch: %.2f\n", result->avg_stretch);
    printf("  Max stretch: %.2f\n", result->max_stretch);
    printf("  Coverage: %.1f%%\n", result->coverage * 100);
}
