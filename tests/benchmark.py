import csv
import os
import random
import subprocess
import time
from pathlib import Path

rng = random.Random(2026)
cases = [
    ("square_64", 64, 64),
    ("square_256", 256, 256),
    ("square_512", 512, 512),
    ("square_1024", 1024, 1024),
    ("wide_128x1024", 128, 1024),
    ("tall_1024x128", 1024, 128),
]

backends = [
    "scalar",
    "openmp",
    "openmp-tiled",
    "cuda",
    "cuda-tiled",
    "checkpoint",
]

columns = [
    "case", "length_a", "length_b", "backend",
    "tile_size", "checkpoint_rows", "repetitions",
    "score", "median_ms", "min_ms", "max_ms",
    "million_cells_per_second", "wall_total_ms",
]

output_path = Path("results/benchmark.csv")
output_path.parent.mkdir(parents=True, exist_ok=True)
environment = os.environ.copy()
environment["OMP_NUM_THREADS"] = "4"


def make_sequences(length_a, length_b):
    a = "".join(rng.choice("ACGT") for _ in range(length_a))
    b = "".join(rng.choice("ACGT") for _ in range(length_b))
    return a, b


def run_case(a, b, backend, repetitions):
    command = [
        "./build/pcap", a, b,
        "--backend", backend,
        "--match", "2",
        "--mismatch", "-1",
        "--gap-open", "3",
        "--gap-extend", "1",
        "--tile-size", "32",
        "--checkpoint-rows", "64",
        "--repetitions", str(repetitions),
    ]
    started = time.perf_counter()
    completed = subprocess.run(
        command, text=True, capture_output=True,
        check=True, env=environment
    )
    wall_ms = (time.perf_counter() - started) * 1000
    fields = dict(
        line.split(": ", 1)
        for line in completed.stdout.splitlines()
    )
    return fields, wall_ms

if "--smoke" in os.sys.argv:
    cases = cases[:1]

with output_path.open("w", newline="") as file:
    writer = csv.DictWriter(file, fieldnames=columns)
    writer.writeheader()

    for case_name, length_a, length_b in cases:
        a, b = make_sequences(length_a, length_b)
        repetitions = 3 if max(length_a, length_b) >= 512 else 5
        reference = None

        for backend in backends:
            if backend == "cuda" and max(length_a, length_b) > 256:
                continue

            try:
                fields, wall_ms = run_case(
                    a, b, backend, repetitions
                )
            except subprocess.CalledProcessError as error:
                print(error.stderr, flush=True)
                raise

            alignment_fields = [
                "Score", "Sequence 1", "Sequence 2",
                "CIGAR", "Range 1", "Range 2",
            ]
            alignment = tuple(
                fields[key] for key in alignment_fields
            )

            if reference is None:
                reference = alignment
            else:
                assert alignment == reference, (
                    case_name, backend, alignment, reference
                )

            median_ms = float(fields["Median_ms"])
            writer.writerow({
                "case": case_name,
                "length_a": length_a,
                "length_b": length_b,
                "backend": backend,
                "tile_size": 32,
                "checkpoint_rows": 64,
                "repetitions": repetitions,
                "score": fields["Score"],
                "median_ms": median_ms,
                "min_ms": fields["Min_ms"],
                "max_ms": fields["Max_ms"],
                "million_cells_per_second":
                    length_a * length_b / (median_ms * 1000),
                "wall_total_ms": wall_ms,
            })
            file.flush()
            print(
                f"{case_name}: {backend} "
                f"{median_ms:.3f} ms, score {fields['Score']}",
                flush=True
            )

print(f"Results saved to {output_path}")
