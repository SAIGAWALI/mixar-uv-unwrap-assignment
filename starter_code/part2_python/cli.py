"""
Command-line interface for UV unwrapping

TEMPLATE - YOU IMPLEMENT

Commands:
- unwrap: Unwrap single mesh
- batch: Batch process directory
- optimize: Find optimal parameters
- analyze: Analyze mesh quality
"""

import argparse
import sys
import os
import json
from pathlib import Path

from uvwrap.bindings import unwrap_mesh_file, load_mesh, save_mesh
from uvwrap.metrics import compute_metrics
from uvwrap.processor import UnwrapProcessor
from uvwrap.optimizer import ParameterOptimizer


def cmd_unwrap(args):
    inp = Path(args.input)
    out = Path(args.output)

    if not inp.exists():
        print(f"Error: {inp} not found.")
        return 1

    print(f"Unwrapping: {inp.name}")

    params = {
        "angle_threshold": args.angle_threshold,
        "min_island_faces": args.min_island,
        "pack_islands": args.pack,
        "island_margin": args.margin,
    }

    mesh, result = unwrap_mesh_file(str(inp), params)
    save_mesh(mesh, str(out))

    m = compute_metrics(mesh, result)

    print("✓ Completed")
    print(f"  Stretch: {m['stretch']:.3f}")
    print(f"  Coverage: {m['coverage'] * 100:.1f}%")
    print(f"  Islands: {result.num_islands}")

    return 0


def cmd_batch(args):
    indir = Path(args.input_dir)
    outdir = Path(args.output_dir)

    if not indir.exists():
        print(f"Error: input directory '{indir}' not found.")
        return 1

    outdir.mkdir(parents=True, exist_ok=True)

    files = sorted([p for p in indir.glob("*.obj")])
    if not files:
        print("No OBJ files found.")
        return 0

    print("UV Unwrapping Batch Processor")
    print("=" * 40)

    processor = UnwrapProcessor(
        angle_threshold=args.angle_threshold,
        threads=args.threads,
    )

    results = processor.process(files, outdir)

    if args.report:
        with open(args.report, "w") as f:
            json.dump(results, f, indent=2)
        print(f"\nReport saved to {args.report}")

    return 0


def cmd_optimize(args):
    inp = Path(args.input)
    if not inp.exists():
        print(f"Error: {inp} not found.")
        return 1

    print(f"Optimizing parameters for: {inp.name}")

    opt = ParameterOptimizer(metric=args.metric)
    best_params, best_score = opt.run(str(inp))

    print("Best Parameters:")
    for k, v in best_params.items():
        print(f"  {k}: {v}")

    print(f"Best Score ({args.metric}): {best_score:.4f}")

    if args.save_params:
        with open(args.save_params, "w") as f:
            json.dump(best_params, f, indent=2)
        print(f"Saved params to {args.save_params}")

    if args.output:
        mesh, _ = unwrap_mesh_file(str(inp), best_params)
        save_mesh(mesh, args.output)
        print(f"Saved optimized output to {args.output}")

    return 0


def cmd_analyze(args):
    inp = Path(args.input)

    if not inp.exists():
        print(f"Error: {inp} not found.")
        return 1

    mesh = load_mesh(str(inp))
    if mesh.uvs is None:
        print("Mesh has no UVs.")
        return 1

    m = compute_metrics(mesh)

    print(f"Analyzing: {inp.name}")
    print(f"  Stretch: {m['stretch']:.3f}")
    print(f"  Coverage: {m['coverage'] * 100:.1f}%")
    print(f"  Angle distortion: {m['angle_distortion']:.3f}")

    return 0


def main():
    parser = argparse.ArgumentParser(
        description="UV Unwrapping Tool",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )

    sub = parser.add_subparsers(dest="cmd")

    p = sub.add_parser("unwrap")
    p.add_argument("input")
    p.add_argument("output")
    p.add_argument("--angle-threshold", type=float, default=30.0)
    p.add_argument("--min-island", type=int, default=10)
    p.add_argument("--no-pack", dest="pack", action="store_false")
    p.add_argument("--margin", type=float, default=0.02)
    p.set_defaults(func=cmd_unwrap)

    p = sub.add_parser("batch")
    p.add_argument("input_dir")
    p.add_argument("output_dir")
    p.add_argument("--threads", type=int, default=None)
    p.add_argument("--angle-threshold", type=float, default=30.0)
    p.add_argument("--report")
    p.set_defaults(func=cmd_batch)

    p = sub.add_parser("optimize")
    p.add_argument("input")
    p.add_argument("--metric", choices=["stretch", "coverage", "angle_distortion"],
                  default="stretch")
    p.add_argument("--save-params")
    p.add_argument("--output")
    p.set_defaults(func=cmd_optimize)

    p = sub.add_parser("analyze")
    p.add_argument("input")
    p.set_defaults(func=cmd_analyze)

    args = parser.parse_args()

    if not args.cmd:
        parser.print_help()
        return 1

    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
