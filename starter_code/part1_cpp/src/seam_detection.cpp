/**
 * @file seam_detection.cpp
 * @brief Seam detection using spanning tree + angular defect
 *
 * SKELETON - YOU IMPLEMENT THIS
 *
 * Algorithm:
 * 1. Build dual graph (faces as nodes, shared edges as edges)
 * 2. Compute spanning tree via BFS
 * 3. Mark non-tree edges as seam candidates
 * 4. Refine using angular defect
 *
 * See reference/algorithms.md for detailed description
 */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <cmath>

#include "unwrap.h"
#include "math_utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <vector>
#include <set>
#include <queue>
#include <utility>
#define _USE_MATH_DEFINES

using namespace std;
/**
 * @brief Compute unit normal of a triangle face
 */
static void compute_face_normal(const Mesh* mesh, int face_idx, float out_n[3])
{
    const int* tris = mesh->triangles;
    const float* V = mesh->vertices;

    int v0 = tris[3*face_idx + 0];
    int v1 = tris[3*face_idx + 1];
    int v2 = tris[3*face_idx + 2];

    float x0 = V[3*v0 + 0], y0 = V[3*v0 + 1], z0 = V[3*v0 + 2];
    float x1 = V[3*v1 + 0], y1 = V[3*v1 + 1], z1 = V[3*v1 + 2];
    float x2 = V[3*v2 + 0], y2 = V[3*v2 + 1], z2 = V[3*v2 + 2];

    // edge vectors
    float e1x = x1 - x0, e1y = y1 - y0, e1z = z1 - z0;
    float e2x = x2 - x0, e2y = y2 - y0, e2z = z2 - z0;

    // cross product e1 × e2
    float nx = e1y * e2z - e1z * e2y;
    float ny = e1z * e2x - e1x * e2z;
    float nz = e1x * e2y - e1y * e2x;

    float len = sqrtf(nx*nx + ny*ny + nz*nz);
    if (len > 0.0f) {
        out_n[0] = nx / len;
        out_n[1] = ny / len;
        out_n[2] = nz / len;
    } else {
        out_n[0] = out_n[1] = out_n[2] = 0.0f;
    }
}

/**
 * @brief Compute angular defect at a vertex
 */
static float compute_angular_defect(const Mesh* mesh, int vertex_idx) {
    if (!mesh) return 0.0f;

    int F = mesh->num_triangles;
    const int* tris = mesh->triangles;
    float angle_sum = 0.0f;

    for (int t = 0; t < F; ++t) {
        int v0 = tris[3*t + 0];
        int v1 = tris[3*t + 1];
        int v2 = tris[3*t + 2];

        if (v0 == vertex_idx || v1 == vertex_idx || v2 == vertex_idx) {
            float ang = compute_vertex_angle_in_triangle(mesh, t, vertex_idx);
            angle_sum += ang;
        }
    }

    return 2.0f * (float)M_PI - angle_sum;
}

/**
 * @brief Get edges incident to vertex
 */
static std::vector<int> get_vertex_edges(const TopologyInfo* topo, int vertex_idx) {
    std::vector<int> edges;
    if (!topo) return edges;

    int E = topo->num_edges;
    const int* arr = topo->edges;

    for (int ei = 0; ei < E; ++ei) {
        int v0 = arr[2*ei + 0];
        int v1 = arr[2*ei + 1];
        if (v0 == vertex_idx || v1 == vertex_idx)
            edges.push_back(ei);
    }
    return edges;
}

int* detect_seams(const Mesh* mesh,
                  const TopologyInfo* topo,
                  float angle_threshold,
                  int* num_seams_out)
{
    if (!mesh || !topo || !num_seams_out) return NULL;

    int F = mesh->num_triangles;
    int E = topo->num_edges;

    vector<vector<pair<int,int>>> face_adj;
    face_adj.resize(F);
    const int* edge_faces = topo->edge_faces;

    // Build dual graph
    for (int ei = 0; ei < E; ++ei) {
        int f0 = edge_faces[2*ei + 0];
        int f1 = edge_faces[2*ei + 1];

        if (f0 >= 0 && f1 >= 0) {
            face_adj[f0].push_back({f1, ei});
            face_adj[f1].push_back({f0, ei});
        }
    }

    // BFS spanning tree
    set<int> tree_edges;
    vector<char> visited(F, 0);

    if (F > 0) {
        queue<int> q;
        q.push(0);
        visited[0] = 1;

        while (!q.empty()) {
            int f = q.front(); q.pop();
            for (auto &nbr : face_adj[f]) {
                int nf = nbr.first;
                int eidx = nbr.second;
                if (!visited[nf]) {
                    tree_edges.insert(eidx);
                    visited[nf] = 1;
                    q.push(nf);
                }
            }
        }
    }
    
    // =====================================================
    // STEP 3: DIHEDRAL-ANGLE-BASED INITIAL SEAMS
    // =====================================================
    set<int> seam_candidates;

    for (int ei = 0; ei < E; ++ei) {

        int f0 = edge_faces[2*ei];
        int f1 = edge_faces[2*ei + 1];

        if (f0 < 0 || f1 < 0) continue;  // skip boundary edges

        float n0[3], n1[3];
        compute_face_normal(mesh, f0, n0);
        compute_face_normal(mesh, f1, n1);

        float dot = n0[0]*n1[0] + n0[1]*n1[1] + n0[2]*n1[2];
        dot = fmaxf(-1.0f, fminf(1.0f, dot));
        float ang = acosf(dot) * 180.0f / (float)M_PI;

        if (ang < 5.0f) continue;        // ignore flat edges
        if (dot < -0.99f) continue;      // ignore flipped

        if (ang > angle_threshold)
            seam_candidates.insert(ei);
    }

    // STEP 4: Angular defect refinement
    float thresh_rad = angle_threshold * (float)M_PI / 180.0f;
    int V = mesh->num_vertices;

    for (int v = 0; v < V; ++v) {
        float defect = compute_angular_defect(mesh, v);
        if (defect > thresh_rad) {
            auto edges = get_vertex_edges(topo, v);
            for (int ei : edges) seam_candidates.insert(ei);
        }
    }

    // STEP 5: Output
    *num_seams_out = seam_candidates.size();
    if (*num_seams_out == 0) return NULL;

    int* seams = (int*)malloc(*num_seams_out * sizeof(int));
    int idx = 0;

    for (int e : seam_candidates)
        seams[idx++] = e;

    printf("Detected %d seams\n", *num_seams_out);
    return seams;
}
