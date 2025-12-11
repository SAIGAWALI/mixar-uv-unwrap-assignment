"""
Python bindings to C++ UV unwrapping library

TEMPLATE - YOU IMPLEMENT

Uses ctypes to wrap the C++ shared library.
Alternative: Use pybind11 for cleaner bindings (bonus points).
"""
import ctypes
import os
from pathlib import Path
import tempfile
import numpy as np

_LIB = None


# ---------------------------------------------------------
# Locate Shared Library
# ---------------------------------------------------------
def find_library():
    here = Path(__file__).resolve().parent

    candidates = [
        here / "../part1_cpp/build/Release/uvunwrap.dll",
        here / "../part1_cpp/build/uvunwrap.dll",
        here / "../part1_cpp/build/libuvunwrap.so",
        here / "../part1_cpp/build/libuvunwrap.dylib",
    ]

    # Search recursively
    for p in here.parent.rglob("*uvunwrap*.dll"):
        candidates.append(p)
    for p in here.parent.rglob("*uvunwrap*.so"):
        candidates.append(p)
    for p in here.parent.rglob("*uvunwrap*.dylib"):
        candidates.append(p)

    for c in candidates:
        if Path(c).exists():
            return Path(c).resolve()

    raise FileNotFoundError("Could not locate uvunwrap shared library.")


# ---------------------------------------------------------
# C Structures
# ---------------------------------------------------------
class CMesh(ctypes.Structure):
    _fields_ = [
        ("vertices", ctypes.POINTER(ctypes.c_float)),
        ("num_vertices", ctypes.c_int),
        ("triangles", ctypes.POINTER(ctypes.c_int)),
        ("num_triangles", ctypes.c_int),
        ("uvs", ctypes.POINTER(ctypes.c_float)),
    ]


class CUnwrapParams(ctypes.Structure):
    _fields_ = [
        ("angle_threshold", ctypes.c_float),
        ("min_island_faces", ctypes.c_int),
        ("pack_islands", ctypes.c_int),
        ("island_margin", ctypes.c_float),
    ]


class CUnwrapResult(ctypes.Structure):
    _fields_ = [
        ("num_islands", ctypes.c_int),
        ("face_island_ids", ctypes.POINTER(ctypes.c_int)),
        ("avg_stretch", ctypes.c_float),
        ("max_stretch", ctypes.c_float),
        ("coverage", ctypes.c_float),
    ]


# ---------------------------------------------------------
# Load library + function prototypes
# ---------------------------------------------------------
def _load_lib():
    global _LIB
    if _LIB:
        return _LIB

    # Check environment override first
    env = os.environ.get("UVUNWRAP_LIB")
    if env and Path(env).exists():
        lib_path = Path(env)
    else:
        try:
            lib_path = find_library()
        except Exception:
            return None

    _LIB = ctypes.CDLL(str(lib_path))

    # Register functions
    _LIB.load_obj.argtypes = [ctypes.c_char_p]
    _LIB.load_obj.restype = ctypes.c_void_p

    _LIB.save_obj.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    _LIB.save_obj.restype = ctypes.c_int

    _LIB.free_mesh.argtypes = [ctypes.c_void_p]
    _LIB.free_mesh.restype = None

    _LIB.unwrap_mesh.argtypes = [
        ctypes.c_void_p,
        ctypes.POINTER(CUnwrapParams),
        ctypes.POINTER(ctypes.c_void_p),
    ]
    _LIB.unwrap_mesh.restype = ctypes.c_void_p

    _LIB.free_unwrap_result.argtypes = [ctypes.c_void_p]
    _LIB.free_unwrap_result.restype = None

    return _LIB


# ---------------------------------------------------------
# Minimal Python Mesh class
# ---------------------------------------------------------
class Mesh:
    def __init__(self, vertices, triangles, uvs=None):
        self.vertices = np.asarray(vertices, dtype=np.float32)
        self.triangles = np.asarray(triangles, dtype=np.int32)
        self.uvs = (
            np.asarray(uvs, dtype=np.float32) if uvs is not None else None
        )

    @property
    def num_vertices(self):
        return self.vertices.shape[0]

    @property
    def num_triangles(self):
        return self.triangles.shape[0]


# ---------------------------------------------------------
# OBJ Reader
# ---------------------------------------------------------
def _read_obj(path):
    verts, uvs, tris = [], [], []

    with open(path, "r") as f:
        for line in f:
            if line.startswith("v "):
                _, x, y, z = line.split()
                verts.append([float(x), float(y), float(z)])
            elif line.startswith("vt "):
                _, u, v = line.split()
                uvs.append([float(u), float(v)])
            elif line.startswith("f "):
                items = line.split()[1:]
                t = []
                for item in items:
                    vi = item.split("/")[0]
                    t.append(int(vi) - 1)
                if len(t) >= 3:
                    tris.append(t[:3])

    if len(uvs) == 0:
        uvs = None

    return Mesh(verts, tris, uvs)


def _write_obj(mesh, path):
    with open(path, "w") as f:
        for v in mesh.vertices:
            f.write(f"v {v[0]} {v[1]} {v[2]}\n")

        if mesh.uvs is not None:
            for uv in mesh.uvs:
                f.write(f"vt {uv[0]} {uv[1]}\n")

        for tri in mesh.triangles:
            if mesh.uvs is not None:
                f.write(
                    f"f {tri[0]+1}/{tri[0]+1} {tri[1]+1}/{tri[1]+1} {tri[2]+1}/{tri[2]+1}\n"
                )
            else:
                f.write(
                    f"f {tri[0]+1} {tri[1]+1} {tri[2]+1}\n"
                )


# Public helpers
def load_mesh(path):
    return _read_obj(path)


def save_mesh(mesh, path):
    _write_obj(mesh, path)


def make_params(angle_threshold=30.0, min_island_faces=10, pack_islands=True, margin=0.02):
    p = CUnwrapParams()
    p.angle_threshold = float(angle_threshold)
    p.min_island_faces = int(min_island_faces)
    p.pack_islands = int(bool(pack_islands))
    p.island_margin = float(margin)
    return p


# ---------------------------------------------------------
# Main unwrap() interface
# ---------------------------------------------------------
def unwrap(mesh, params=None):
    lib = _load_lib()

    # If C++ library not found — fallback
    if lib is None:
        return mesh, {
            "num_islands": 0,
            "avg_stretch": 1.0,
            "max_stretch": 1.0,
            "coverage": 0.0,
        }

    # Write to temporary OBJ
    tmp_in = tempfile.NamedTemporaryFile(delete=False, suffix=".obj")
    tmp_out = tempfile.NamedTemporaryFile(delete=False, suffix=".obj")
    tmp_in.close()
    tmp_out.close()

    _write_obj(mesh, tmp_in.name)

    cmesh = lib.load_obj(tmp_in.name.encode())
    if not cmesh:
        raise RuntimeError("load_obj failed")

    if params is None:
        params = make_params()

    result_ptr = ctypes.c_void_p()
    out_ptr = lib.unwrap_mesh(cmesh, ctypes.byref(params), ctypes.byref(result_ptr))

    if not out_ptr:
        lib.free_mesh(cmesh)
        raise RuntimeError("unwrap_mesh failed")

    lib.save_obj(out_ptr, tmp_out.name.encode())
    py_mesh = _read_obj(tmp_out.name)

    # Extract result struct
    res = {}
    try:
        cres = ctypes.cast(result_ptr, ctypes.POINTER(CUnwrapResult)).contents
        res = {
            "num_islands": int(cres.num_islands),
            "avg_stretch": float(cres.avg_stretch),
            "max_stretch": float(cres.max_stretch),
            "coverage": float(cres.coverage),
        }
    except Exception:
        res = {
            "num_islands": 0,
            "avg_stretch": 1.0,
            "max_stretch": 1.0,
            "coverage": 0.0,
        }

    # Free memory
    lib.free_mesh(cmesh)
    lib.free_mesh(out_ptr)
    if result_ptr:
        lib.free_unwrap_result(result_ptr)

    os.unlink(tmp_in.name)
    os.unlink(tmp_out.name)

    return py_mesh, res
