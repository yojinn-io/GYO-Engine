"""v8-product-anim-bottlenecks B2 memory: the peak resident set a full player load adds.

Runs the measurement program three times: --baseline (catalog only), --load-only (one full load), then the full
report, whose output is forwarded. The peak of waited children only grows, so after each run it is the maximum so
far; the load's increase is the second reading minus the first. The C++ side reads no OS API (os_boundary_guard R3);
where the resource module is missing (Windows) the delta is reported as n/a, as declared.

Usage: python3 anim_bottlenecks.py PROGRAM [--out report.json]
"""
import subprocess
import sys

try:
    import resource
except ImportError:  # Windows
    resource = None


def peak_children_bytes():
    if resource is None:
        return None
    peak = resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss
    # Linux reports KiB, macOS bytes.
    return peak if sys.platform == "darwin" else peak * 1024


def run(arguments):
    completed = subprocess.run(arguments)
    if completed.returncode != 0:
        sys.exit(completed.returncode)


def main():
    if len(sys.argv) < 2:
        print(__doc__, file=sys.stderr)
        return 2
    program, rest = sys.argv[1], sys.argv[2:]
    run([program, "--baseline"])
    baseline = peak_children_bytes()
    run([program, "--load-only"])
    loaded = peak_children_bytes()
    delta = "n/a" if baseline is None else str((loaded - baseline) // (1024 * 1024))
    print(f"anim bottlenecks memory: b2_rss_delta_mb={delta} baseline_mb="
          f"{'n/a' if baseline is None else baseline // (1024 * 1024)}", flush=True)
    run([program, *rest])
    return 0


if __name__ == "__main__":
    sys.exit(main())
