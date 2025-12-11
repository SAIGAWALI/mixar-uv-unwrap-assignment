"""
Quality metrics for UV mappings

TEMPLATE - YOU IMPLEMENT

Computes:
- Stretch: UV→3D Jacobian singular value ratio
- Coverage: Percentage of [0,1]² used
- Angle distortion: Max angle difference

See reference/metrics_spec.md for exact formulas
"""

import numpy as np


def _angle(a, b):
    na = np.linalg.norm(a)
    nb = np.linalg.norm(b)
    if na < 1e-12 or nb < 1e-12:
        return 0.0
    c = np.dot(a, b) / (na * nb)
    c = np.clip(c, -1.0, 1.0)
    return np.arccos(c)


def compute_stretch(mesh, uvs):
    verts = mesh.vertices
    tris = mesh.triangles
    vals = []

    for t in tris:
        i, j, k = t
        p0, p1, p2 = verts[i], verts[j], verts[k]
        uv0, uv1, uv2 = uvs[i], uvs[j], uvs[k]

        M = np.array(
            [
                [uv1[0] - uv0[0], uv2[0] - uv0[0]],
                [uv1[1] - uv0[1], uv2[1] - uv0[1]],
            ],
            dtype=float,
        )
        det = np.linalg.det(M)
        if abs(det) < 1e-12:
            vals.append(1.0)
            continue

        J = np.column_stack((p1 - p0, p2 - p0)).dot(np.linalg.inv(M))
        JTJ = J.T.dot(J)
        e = np.linalg.eigvals(JTJ)
        e = np.real(np.maximum(e, 0.0))
        s = np.sqrt(np.sort(e))[::-1]
        if len(s) >= 2 and s[1] > 1e-12:
            vals.append(float(s[0] / s[1]))
        else:
            vals.append(1.0)

    if not vals:
        return 1.0, 1.0

    vals = np.array(vals)
    return float(vals.mean()), float(vals.max())


def compute_coverage(uvs, triangles, resolution=1024):
    grid = np.zeros((resolution, resolution), dtype=np.uint8)

    for t in triangles:
        tri_uv = uvs[list(t)]
        min_u, max_u = tri_uv[:, 0].min(), tri_uv[:, 0].max()
        min_v, max_v = tri_uv[:, 1].min(), tri_uv[:, 1].max()

        min_u = np.clip(min_u, 0, 1)
        max_u = np.clip(max_u, 0, 1)
        min_v = np.clip(min_v, 0, 1)
        max_v = np.clip(max_v, 0, 1)

        x0 = int(min_u * (resolution - 1))
        x1 = int(max_u * (resolution - 1))
        y0 = int(min_v * (resolution - 1))
        y1 = int(max_v * (resolution - 1))

        uv0, uv1, uv2 = tri_uv
        denom = (uv1[1] - uv2[1]) * (uv0[0] - uv2[0]) + (uv2[0] - uv1[0]) * (uv0[1] - uv2[1])
        if abs(denom) < 1e-12:
            continue

        for y in range(y0, y1 + 1):
            v = y / (resolution - 1)
            for x in range(x0, x1 + 1):
                u = x / (resolution - 1)
                p = np.array([u, v])
                a = ((uv1[1] - uv2[1]) * (p[0] - uv2[0]) + (uv2[0] - uv1[0]) * (p[1] - uv2[1])) / denom
                b = ((uv2[1] - uv0[1]) * (p[0] - uv2[0]) + (uv0[0] - uv2[0]) * (p[1] - uv2[1])) / denom
                c = 1 - a - b
                if a >= 0 and b >= 0 and c >= 0:
                    grid[y, x] = 1

    return float(grid.mean())


def compute_angle_distortion(mesh, uvs):
    verts = mesh.vertices
    tris = mesh.triangles
    max_err = 0.0

    for t in tris:
        i, j, k = t
        p0, p1, p2 = verts[i], verts[j], verts[k]
        u0, u1, u2 = uvs[i], uvs[j], uvs[k]

        a0 = _angle(p1 - p0, p2 - p0)
        a1 = _angle(p0 - p1, p2 - p1)
        a2 = _angle(p0 - p2, p1 - p2)

        b0 = _angle(u1 - u0, u2 - u0)
        b1 = _angle(u0 - u1, u2 - u1)
        b2 = _angle(u0 - u2, u1 - u2)

        err = max(abs(a0 - b0), abs(a1 - b1), abs(a2 - b2))
        if err > max_err:
            max_err = err

    return float(max_err)
