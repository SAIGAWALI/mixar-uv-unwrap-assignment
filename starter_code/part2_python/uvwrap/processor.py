"""
Multi-threaded batch processor

TEMPLATE - YOU IMPLEMENT

Processes multiple meshes in parallel using ThreadPoolExecutor.
"""

from concurrent.futures import ThreadPoolExecutor, as_completed
import threading
import os
import time
from . import bindings, metrics


class UnwrapProcessor:
    def __init__(self, num_threads=None):
        self.num_threads = num_threads or os.cpu_count()
        self.completed = 0

    def process_batch(self, input_files, output_dir, params, on_progress=None):
        os.makedirs(output_dir, exist_ok=True)
        results = []
        total = len(input_files)
        self.completed = 0
        start = time.time()

        with ThreadPoolExecutor(max_workers=self.num_threads) as ex:
            futs = {ex.submit(self._run_single, path, output_dir, params): path for path in input_files}
            for fut in as_completed(futs):
                inp = futs[fut]
                try:
                    r = fut.result()
                    results.append(r)
                except Exception as e:
                    results.append({"file": os.path.basename(inp), "error": str(e)})

                self.completed += 1
                if on_progress:
                    on_progress(self.completed, total, os.path.basename(inp))

        return {
            "summary": self._summary(results, time.time() - start),
            "files": results,
        }

    def _run_single(self, path, out_dir, params):
        t0 = time.time()
        mesh = bindings.load_mesh(path)
        out, _ = bindings.unwrap(mesh, params)
        out_path = os.path.join(out_dir, os.path.basename(path))
        bindings.save_mesh(out, out_path)

        uvs = out.uvs
        tris = out.triangles

        avg_s, max_s = metrics.compute_stretch(out, uvs)
        cov = metrics.compute_coverage(uvs, tris, resolution=512)
        ang = metrics.compute_angle_distortion(out, uvs)

        return {
            "file": os.path.basename(path),
            "vertices": out.num_vertices,
            "triangles": out.num_triangles,
            "time": time.time() - t0,
            "metrics": {
                "avg_stretch": avg_s,
                "max_stretch": max_s,
                "coverage": cov,
                "angle_distortion": ang,
            },
        }

    def _summary(self, results, total_time):
        success = [r for r in results if "error" not in r]
        failure = len(results) - len(success)

        avg_time = (
            sum(r["time"] for r in success) / len(success)
            if success else 0.0
        )

        if success:
            avg_stretch = sum(r["metrics"]["avg_stretch"] for r in success) / len(success)
            avg_cov = sum(r["metrics"]["coverage"] for r in success) / len(success)
        else:
            avg_stretch = avg_cov = 0.0

        return {
            "total": len(results),
            "success": len(success),
            "failed": failure,
            "total_time": total_time,
            "avg_time": avg_time,
            "avg_stretch": avg_stretch,
            "avg_coverage": avg_cov,
        }
