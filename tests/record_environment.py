import json
import os
import platform
import subprocess
from datetime import datetime, timezone
from pathlib import Path


def output(command):
    try:
        return subprocess.run(
            command, capture_output=True, text=True, check=True
        ).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return "unavailable"


details = {
    "recorded_utc": datetime.now(timezone.utc).isoformat(),
    "platform": platform.platform(),
    "python": platform.python_version(),
    "cpu": output(["lscpu", "--parse=MODELNAME"]).splitlines()[-1],
    "gpu": output([
        "nvidia-smi",
        "--query-gpu=name,driver_version,memory.total",
        "--format=csv,noheader",
    ]),
    "g++": output(["g++", "--version"]).splitlines()[0],
    "nvcc": output(["nvcc", "--version"]).splitlines()[-1],
    "cmake": output(["cmake", "--version"]).splitlines()[0],
    "git_commit": output(["git", "rev-parse", "HEAD"]),
    "omp_threads": 4,
    "benchmark_seed": 2026,
    "scoring": {
        "match": 2,
        "mismatch": -1,
        "gap_open": 3,
        "gap_extend": 1,
    },
    "timing": "One warmup; median/min/max of backend calls in one process.",
    "benchmark_file": "results/benchmark.csv",
}

path = Path("results/environment.json")
path.parent.mkdir(parents=True, exist_ok=True)
path.write_text(json.dumps(details, indent=2) + "\n")
print(f"Environment saved to {path}")
