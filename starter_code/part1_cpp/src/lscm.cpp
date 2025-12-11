/**
 * @file lscm.cpp
 * @brief LSCM (Least Squares Conformal Maps) parameterization
 *
 * SKELETON - YOU IMPLEMENT THIS
 *
 * This is the MOST COMPLEX part of the assignment.
 *
 * IMPORTANT:
 * - See reference/lscm_matrix_example.cpp for matrix assembly example
 * - See reference/algorithms.md for mathematical background
 * - Use Eigen library for sparse linear algebra
 *
 * Algorithm:
 * 1. Build local vertex mapping (global -> local indices)
 * 2. Assemble LSCM sparse matrix
 * 3. Set boundary conditions (pin 2 vertices)
 * 4. Solve sparse linear system
 * 5. Normalize UVs to [0,1]^2
 */

#include "lscm.h"
#include "math_utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <map>
#include <vector>
#include <set>
#include <algorithm>
#include <complex>

// Eigen library for sparse matrices
#include <Eigen/Sparse>
#include <Eigen/SparseLU>

// Helper: get vertex position (reads from Mesh::vertices)
static inline Eigen::Vector3d get_vertex_pos(const Mesh* mesh, int vid) {
    Eigen::Vector3d p(0,0,0);
    if (!mesh) return p;
    if (vid < 0 || vid >= mesh->num_vertices) return p;
    p.x() = mesh->vertices[3 * vid + 0];
    p.y() = mesh->vertices[3 * vid + 1];
    p.z() = mesh->vertices[3 * vid + 2];
    return p;
}

int find_boundary_vertices(const Mesh* mesh,
                          const int* face_indices,
                          int num_faces,
                          int** boundary_out) {
    // Implement
    std::map<std::pair<int,int>, int> edge_count;
    for (int fi = 0; fi < num_faces; ++fi) {
        int a = face_indices[3*fi + 0];
        int b = face_indices[3*fi + 1];
        int c = face_indices[3*fi + 2];
        std::pair<int,int> e1 = std::minmax(a,b);
        std::pair<int,int> e2 = std::minmax(b,c);
        std::pair<int,int> e3 = std::minmax(c,a);
        edge_count[e1]++;
        edge_count[e2]++;
        edge_count[e3]++;
    }

    std::set<int> boundary_verts;
    for (auto &kv : edge_count) {
        if (kv.second == 1) {
            // boundary edge
            boundary_verts.insert(kv.first.first);
            boundary_verts.insert(kv.first.second);
        }
    }

    int num_boundary = (int)boundary_verts.size();
    *boundary_out = (int*)malloc(num_boundary * sizeof(int));
    int idx = 0;
    for (int v : boundary_verts) {
        (*boundary_out)[idx++] = v;
    }
    return num_boundary;
}

void normalize_uvs_to_unit_square(float* uvs, int num_verts) {
    if (!uvs || num_verts == 0) return;

    // Find bounding box
    float min_u = FLT_MAX, max_u = -FLT_MAX;
    float min_v = FLT_MAX, max_v = -FLT_MAX;

    for (int i = 0; i < num_verts; i++) {
        float u = uvs[i * 2];
        float v = uvs[i * 2 + 1];

        min_u = min_float(min_u, u);
        max_u = max_float(max_u, u);
        min_v = min_float(min_v, v);
        max_v = max_float(max_v, v);
    }

    float u_range = max_u - min_u;
    float v_range = max_v - min_v;

    if (u_range < 1e-6f) u_range = 1.0f;
    if (v_range < 1e-6f) v_range = 1.0f;

    // Normalize to [0, 1]
    for (int i = 0; i < num_verts; i++) {
        uvs[i * 2] = (uvs[i * 2] - min_u) / u_range;
        uvs[i * 2 + 1] = (uvs[i * 2 + 1] - min_v) / v_range;
    }
}

float* lscm_parameterize(const Mesh* mesh,
                         const int* face_indices,
                         int num_faces) {
    if (!mesh || !face_indices || num_faces == 0) return NULL;

    printf("LSCM parameterizing %d faces...\n", num_faces);

    // STEP 1: Local vertex mapping
    std::map<int, int> global_to_local;
    std::vector<int> local_to_global;
    local_to_global.reserve(num_faces * 3);

    for (int fi = 0; fi < num_faces; ++fi) {
        int a = face_indices[3*fi + 0];
        int b = face_indices[3*fi + 1];
        int c = face_indices[3*fi + 2];
        int arr[3] = {a,b,c};
        for (int k = 0; k < 3; ++k) {
            int g = arr[k];
            if (global_to_local.find(g) == global_to_local.end()) {
                int local_idx = (int)local_to_global.size();
                global_to_local[g] = local_idx;
                local_to_global.push_back(g);
            }
        }
    }

    int n = (int)local_to_global.size();
    printf("  Island has %d vertices\n", n);

    if (n < 3) {
        fprintf(stderr, "LSCM: Island too small (%d vertices)\n", n);
        return NULL;
    }

    // STEP 2: Build sparse matrix
    typedef Eigen::Triplet<double> T;
    std::vector<T> triplets;
    triplets.reserve(num_faces * 36); 

    // Helper to add 2x2 block for local vertices ii and jj
    auto add_block = [&](int vi_local, std::complex<double> a, int vj_local, std::complex<double> b) {
        // indices in system: u: 0..n-1, v: n..2n-1
        int ui = vi_local;
        int vi = n + vi_local;
        int uj = vj_local;
        int vj = n + vj_local;
        std::complex<double> prod = a * std::conj(b);
        double Auu = prod.real();
        double Avv = prod.real();
        double Auv = -prod.imag();
        double Avu = prod.imag();
        triplets.emplace_back(ui, uj, Auu);
        triplets.emplace_back(ui, vj, Auv);
        triplets.emplace_back(vi, uj, Avu);
        triplets.emplace_back(vi, vj, Avv);
    };

    // Loop triangles
    for (int fi = 0; fi < num_faces; ++fi) {
        int ga = face_indices[3*fi + 0];
        int gb = face_indices[3*fi + 1];
        int gc = face_indices[3*fi + 2];
        int la = global_to_local[ga];
        int lb = global_to_local[gb];
        int lc = global_to_local[gc];

        Eigen::Vector3d pa = get_vertex_pos(mesh, ga);
        Eigen::Vector3d pb = get_vertex_pos(mesh, gb);
        Eigen::Vector3d pc = get_vertex_pos(mesh, gc);

        Eigen::Vector3d e0 = pb - pa;
        Eigen::Vector3d e1 = pc - pa;
        Eigen::Vector3d nrm = e0.cross(e1);
        double area2 = nrm.norm();
        if (area2 < 1e-12) continue; // degenerate

        Eigen::Vector3d ex = e0.normalized();
        Eigen::Vector3d ey = nrm.cross(ex).normalized();

        Eigen::Vector2d p2a(0.0, 0.0);
        Eigen::Vector2d p2b((pb - pa).dot(ex), (pb - pa).dot(ey));
        Eigen::Vector2d p2c((pc - pa).dot(ex), (pc - pa).dot(ey));

        std::complex<double> cij(p2b.x() - p2a.x(), p2b.y() - p2a.y());
        std::complex<double> cik(p2c.x() - p2a.x(), p2c.y() - p2a.y());

        std::complex<double> coeffs[3];
        coeffs[0] = -cij - cik; // vertex a
        coeffs[1] = cij;        // vertex b
        coeffs[2] = cik;        // vertex c

        double w = 1.0 / (0.5 * area2);

        // add contributions for pairs (a,b,c)
        int verts_local[3] = { la, lb, lc };
        for (int ai = 0; ai < 3; ++ai) {
            for (int bj = 0; bj < 3; ++bj) {
                // scaled coefficients
                add_block(verts_local[ai], coeffs[ai] * w, verts_local[bj], coeffs[bj]);
            }
        }
    }

    // STEP 3: Boundary conditions - find boundary vertices
    int* boundary = NULL;
    int num_boundary = find_boundary_vertices(mesh, face_indices, num_faces, &boundary);

    int pin0_local = 0;
    int pin1_local = (n > 1) ? (n/2) : 0;

    if (num_boundary >= 2) {
        // pick two boundary vertices far apart (in 3D)
        // map boundary global indices -> local
        std::vector<int> b_local;
        b_local.reserve(num_boundary);
        for (int i = 0; i < num_boundary; ++i) {
            int g = boundary[i];
            auto it = global_to_local.find(g);
            if (it != global_to_local.end()) b_local.push_back(it->second);
        }
        if (b_local.size() >= 2) {
            // choose first and farthest from it
            pin0_local = b_local[0];
            double maxd = -1.0;
            int best = pin0_local;
            Eigen::Vector3d p0 = get_vertex_pos(mesh, local_to_global[pin0_local]);
            for (int x : b_local) {
                Eigen::Vector3d px = get_vertex_pos(mesh, local_to_global[x]);
                double d = (px - p0).squaredNorm();
                if (d > maxd) { maxd = d; best = x; }
            }
            pin1_local = best;
        }
    }

    if (boundary) free(boundary);

    // target pinned uv values
    double u0 = 0.0, v0 = 0.0;
    double u1 = 1.0, v1 = 0.0;

    // Build sparse matrix
    Eigen::SparseMatrix<double> A(2*n, 2*n);
    A.setFromTriplets(triplets.begin(), triplets.end());

    Eigen::VectorXd rhs = Eigen::VectorXd::Zero(2*n);

    // Apply Dirichlet BCs: indices to fix
    std::vector<int> fixed_indices = { pin0_local, n + pin0_local, pin1_local, n + pin1_local };
    std::vector<double> fixed_values = { u0, v0, u1, v1 };

    // For each fixed row, zero it and set diagonal to 1 and rhs to value, also zero column
    for (size_t k = 0; k < fixed_indices.size(); ++k) {
        int row = fixed_indices[k];
        // zero row
        for (Eigen::SparseMatrix<double>::InnerIterator it(A, row); it; ++it) {
            it.valueRef() = 0.0;
        }
        // set diagonal
        A.coeffRef(row, row) = 1.0;
        // zero column entries
        for (int r = 0; r < A.rows(); ++r) {
            if (r == row) continue;
            if (A.coeff(r, row) != 0.0) A.coeffRef(r, row) = 0.0;
        }
        rhs[row] = fixed_values[k];
    }

    // STEP 4: Solve
    Eigen::VectorXd x;
    {
        Eigen::SparseLU<Eigen::SparseMatrix<double>> solver;
        solver.analyzePattern(A);
        solver.factorize(A);
        if (solver.info() != Eigen::Success) {
            fprintf(stderr, "LSCM: SparseLU factorization failed\n");
            return NULL;
        }
        x = solver.solve(rhs);
        if (solver.info() != Eigen::Success) {
            fprintf(stderr, "LSCM: SparseLU solve failed\n");
            return NULL;
        }
    }

    // STEP 5: Extract UVs
    float* uvs = (float*)malloc(n * 2 * sizeof(float));
    for (int i = 0; i < n; ++i) {
        double uu = x(i);
        double vv = x(n + i);
        uvs[i*2 + 0] = (float)uu;
        uvs[i*2 + 1] = (float)vv;
    }

    normalize_uvs_to_unit_square(uvs, n);

    printf("  LSCM completed\n");
    return uvs;
}
