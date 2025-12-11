/**
 * @file topology.cpp
 * @brief Topology builder implementation
 *
 * SKELETON - YOU IMPLEMENT THIS
 *
 * Algorithm:
 * 1. Extract all edges from triangles
 * 2. Ensure uniqueness (always store as v0 < v1)
 * 3. For each edge, find adjacent faces
 * 4. Validate using Euler characteristic
 */

#include "topology.h"
#include <stdlib.h>
#include <stdio.h>
#include <map>
#include <vector>
using namespace std;

/**
 * @brief Edge structure for uniqueness
 */
struct Edge {
    int v0, v1;

    Edge(int a, int b) {
        // Always store smaller vertex first
        if (a < b) {
            v0 = a;
            v1 = b;
        } else {
            v0 = b;
            v1 = a;
        }
    }

    bool operator<(const Edge& other) const {
        if (v0 != other.v0) return v0 < other.v0;
        return v1 < other.v1;
    }
};

/**
 * @brief Edge information
 */
struct EdgeInfo {
    int face0;
    int face1;

    EdgeInfo() : face0(-1), face1(-1) {}
};

TopologyInfo* build_topology(const Mesh* mesh) {
    if (!mesh) return NULL;

    // TODO: Implement topology building
    //
    // Steps:
    // 1. Create std::map<Edge, EdgeInfo> to collect edges
    // 2. Iterate through all triangles
    //    For each triangle, extract 3 edges
    //    Add to map, tracking which faces use each edge
    // 3. Convert map to arrays (edges, edge_faces)
    // 4. Allocate TopologyInfo and fill arrays
    //
    // Hints:
    // - Use Edge struct for automatic ordering
    // - Each edge should have 1 or 2 adjacent faces
    // - Boundary edges have only 1 face (set face1 = -1)
    //
    // See reference/topology_example.cpp for complete example

    TopologyInfo* topo = (TopologyInfo*)malloc(sizeof(TopologyInfo));

    // Initialize to safe defaults (prevents crashes before implementation)
    topo->edges = NULL;
    topo->num_edges = 0;
    topo->edge_faces = NULL;

    // TODO: Your implementation here
    map<Edge,EdgeInfo> edge_map;
    int num_trangles=mesh->num_triangles;
    int* trangles=mesh->triangles;
    for(int trangle_ind=0;trangle_ind<num_trangles;trangle_ind++){
        int a=trangles[trangle_ind*3];
        int b=trangles[trangle_ind*3+1];
        int c=trangles[trangle_ind*3+2];
        Edge E1(a,b);
        Edge E2(a,c);
        Edge E3(b,c);
        if(!edge_map.count(E1)){
            edge_map[E1].face0=trangle_ind;
        }
        else{
            if(edge_map[E1].face1!=-1){
                printf("Warning: non-manifold edge (%d,%d)\n", E1.v0, E1.v1);
            }
            else{
            edge_map[E1].face1=trangle_ind;
            }
        }
        if(!edge_map.count(E2)){
            edge_map[E2].face0=trangle_ind;
        }
        else{
            if(edge_map[E2].face1!=-1){
                printf("Warning: non-manifold edge (%d,%d)\n", E2.v0, E2.v1);
            }
            else{
            edge_map[E2].face1=trangle_ind;
            }
        }
        if(!edge_map.count(E3)){
            edge_map[E3].face0=trangle_ind;
        }
        else{
            if(edge_map[E3].face1!=-1){
                printf("Warning: non-manifold edge (%d,%d)\n", E3.v0, E3.v1);
            }
            else{
            edge_map[E3].face1=trangle_ind;
            }
        }
    }
    int num_edges=edge_map.size();
    topo->edges = (int*)malloc(sizeof(int) * 2 * num_edges);
    topo->edge_faces = (int*)malloc(sizeof(int) * 2 * num_edges);
    int count=0;
    for(auto i:edge_map){
        topo->edges[count]=i.first.v0;
        topo->edges[count+1]=i.first.v1;
        topo->edge_faces[count]=i.second.face0;
        topo->edge_faces[count+1]=i.second.face1;
        count+=2;
    }
    
    topo->num_edges=num_edges;

    return topo;
}

void free_topology(TopologyInfo* topo) {
    if (!topo) return;

    if (topo->edges) free(topo->edges);
    if (topo->edge_faces) free(topo->edge_faces);
    free(topo);
}

int validate_topology(const Mesh* mesh, const TopologyInfo* topo) {
    if (!mesh || !topo) return 0;

    int V = mesh->num_vertices;
    int E = topo->num_edges;
    int F = mesh->num_triangles;

    int euler = V - E + F;

    printf("Topology validation:\n");
    printf("  V=%d, E=%d, F=%d\n", V, E, F);
    printf("  Euler characteristic: %d (expected 2 for closed mesh)\n", euler);

    // Closed meshes should have Euler = 2
    // Open meshes or meshes with holes may differ
    if (euler != 2) {
        printf("  Warning: Non-standard Euler characteristic\n");
        printf("  (This may be OK for open meshes or meshes with boundaries)\n");
    }

    return 1;
}
