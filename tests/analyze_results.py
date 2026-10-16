import csv
from collections import defaultdict
from pathlib import Path

source = Path("results/benchmark_summary.csv")
rows = list(csv.DictReader(source.open(newline="")))
by_case = defaultdict(dict)

for row in rows:
    by_case[row["case"]][row["backend"]] = row

output = Path("results/speedup.csv")
with output.open("w", newline="") as file:
    writer = csv.writer(file)
    writer.writerow([
        "case", "length_a", "length_b", "backend",
        "median_ms", "scalar_speedup",
        "openmp_efficiency_4_threads",
        "million_cells_per_second",
    ])

    for case, backends in by_case.items():
        scalar_ms = float(backends["scalar"]["median_ms"])
        length_a = int(backends["scalar"]["length_a"])
        length_b = int(backends["scalar"]["length_b"])

        for backend, row in backends.items():
            elapsed_ms = float(row["median_ms"])
            speedup = scalar_ms / elapsed_ms
            efficiency = (
                speedup / 4
                if backend in ("openmp", "openmp-tiled")
                else ""
            )
            writer.writerow([
                case, length_a, length_b, backend,
                elapsed_ms, speedup, efficiency,
                length_a * length_b / (elapsed_ms * 1000),
            ])

        fastest = min(
            backends, key=lambda name: float(backends[name]["median_ms"])
        )
        cuda_row = backends.get("cuda-tiled")
        cuda_speedup = (
            scalar_ms / float(cuda_row["median_ms"])
            if cuda_row else float("nan")
        )
        print(
            f"{case}: fastest={fastest}; "
            f"CUDA-tiled speedup={cuda_speedup:.2f}x"
        )

print(f"Analysis saved to {output}")
