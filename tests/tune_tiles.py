import csv
import os
import random
import subprocess
from pathlib import Path

rng = random.Random(2027)
cases = [
    ("square_256", 256, 256),
    ("square_512", 512, 512),
    ("wide_128x1024", 128, 1024),
    ("tall_1024x128", 1024, 128),
]

configurations = [
    ("openmp-tiled", size) for size in (8, 16, 32, 64, 128)
] + [
    ("cuda-tiled", size) for size in (8, 16, 32)
]

environment = os.environ.copy()
environment["OMP_NUM_THREADS"] = "4"

path = Path("results/tile_tuning.csv")
path.parent.mkdir(parents=True, exist_ok=True)


def measure(a, b, backend, tile_size):
    command = [
        "./build/pcap", a, b,
        "--backend", backend,
        "--tile-size", str(tile_size),
        "--gap-open", "3",
        "--gap-extend", "1",
        "--repetitions", "5",
    ]
    completed = subprocess.run(
        command, capture_output=True, text=True,
        check=True, env=environment
    )
    return dict(
        line.split(": ", 1)
        for line in completed.stdout.splitlines()
    )

with path.open("w", newline="") as file:
    writer = csv.DictWriter(
        file,
        fieldnames=[
            "case", "length_a", "length_b",
            "backend", "tile_size",
            "score", "median_ms", "min_ms", "max_ms",
        ],
    )
    writer.writeheader()

    for name, length_a, length_b in cases:
        a = "".join(rng.choice("ACGT") for _ in range(length_a))
        b = "".join(rng.choice("ACGT") for _ in range(length_b))

        reference = measure(a, b, "scalar", 32)
        alignment_fields = (
            "Score", "Sequence 1", "Sequence 2",
            "CIGAR", "Range 1", "Range 2",
        )
        expected = tuple(reference[key] for key in alignment_fields)

        for backend, tile_size in configurations:
            fields = measure(a, b, backend, tile_size)
            actual = tuple(fields[key] for key in alignment_fields)
            assert actual == expected, (name, backend, tile_size)

            writer.writerow({
                "case": name,
                "length_a": length_a,
                "length_b": length_b,
                "backend": backend,
                "tile_size": tile_size,
                "score": fields["Score"],
                "median_ms": fields["Median_ms"],
                "min_ms": fields["Min_ms"],
                "max_ms": fields["Max_ms"],
            })
            file.flush()
            print(
                f"{name}: {backend} tile {tile_size}: "
                f"{fields['Median_ms']} ms",
                flush=True,
            )

print(f"Tile measurements saved to {path}")
