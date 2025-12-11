"""
Parameter optimization using grid search

TEMPLATE - YOU IMPLEMENT

Finds best parameters for a given mesh by testing multiple combinations.
"""

import itertools

import json
import numpy as np
from . import bindings, metrics


def optimize_parameters(mesh_path, target_metric="avg_stretch"):
    angles = [20, 25, 30, 35, 40]
    mins = [5, 10, 20]

    best = None
    if target_metric == "coverage":
        best_score = -1
    else:
        best_score = float("inf")

    for a in angles:
        for m in mins:
            params = bindings.make_params(angle_threshold=a, min_island_faces=m)
            mesh = bindings.load_mesh(mesh_path)

            try:
                out, _ = bindings.unwrap(mesh, params)
            except Exception:
                continue

            uvs = out.uvs
            tris = out.triangles

            avg_s, max_s = metrics.compute_stretch(out, uvs)
            cov = metrics.compute_coverage(uvs, tris, resolution=512)

            score = (
                avg_s if target_metric == "avg_stretch"
                else max_s if target_metric == "max_stretch"
                else cov
            )

            better = score < best_score if target_metric != "coverage" else score > best_score
            if best is None or better:
                best = {"angle_threshold": a, "min_island_faces": m}
                best_score = score

    return best, best_score
