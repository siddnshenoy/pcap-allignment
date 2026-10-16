import csv
from collections import defaultdict
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

summary = list(csv.DictReader(
    Path("results/benchmark_summary.csv").open(newline="")
))
tile_rows = list(csv.DictReader(
    Path("results/tile_tuning.csv").open(newline="")
))
figures = Path("figures")
figures.mkdir(exist_ok=True)

colors = {
    "scalar": "#334155",
    "openmp": "#a855f7",
    "openmp-tiled": "#0891b2",
    "cuda": "#f97316",
    "cuda-tiled": "#dc2626",
}

square = sorted(
    (row for row in summary if row["case"].startswith("square_")),
    key=lambda row: int(row["length_a"]),
)

fig, ax = plt.subplots(figsize=(8, 5))
for backend in (
    "scalar", "openmp", "openmp-tiled", "cuda", "cuda-tiled"
):
    selected = [
        row for row in square if row["backend"] == backend
    ]
    if not selected:
        continue

    x = [int(row["length_a"]) for row in selected]
    y = [float(row["median_ms"]) for row in selected]
    low = [float(row["min_trial_ms"]) for row in selected]
    high = [float(row["max_trial_ms"]) for row in selected]

    ax.plot(
        x, y, marker="o", linewidth=2,
        color=colors[backend], label=backend,
    )
    ax.fill_between(
        x, low, high, color=colors[backend], alpha=0.12
    )

ax.set_xscale("log", base=2)
ax.set_yscale("log")
ax.set_xticks([64, 256, 512, 1024])
ax.set_xticklabels(["64", "256", "512", "1024"])
ax.set_xlabel("Sequence length (square matrices)")
ax.set_ylabel("Median end-to-end backend time (ms)")
ax.set_title("Smith–Waterman runtime scaling")
ax.grid(True, which="both", alpha=0.25)
ax.legend(frameon=False, ncol=2)
fig.tight_layout()
fig.savefig(figures / "runtime_scaling.png", dpi=200)
plt.close(fig)

by_case = defaultdict(dict)
for row in summary:
    by_case[row["case"]][row["backend"]] = row

case_order = [
    "square_64", "square_256", "square_512",
    "square_1024", "wide_128x1024", "tall_1024x128",
]
labels = ["64²", "256²", "512²", "1024²", "128×1024", "1024×128"]
backends = ["openmp", "openmp-tiled", "cuda-tiled"]
positions = list(range(len(case_order)))
width = 0.25

fig, ax = plt.subplots(figsize=(10, 5))
for offset, backend in enumerate(backends):
    speedups = [
        float(by_case[case]["scalar"]["median_ms"])
        / float(by_case[case][backend]["median_ms"])
        for case in case_order
    ]
    x = [position + (offset - 1) * width for position in positions]
    ax.bar(
        x, speedups, width=width,
        color=colors[backend], label=backend,
    )

ax.axhline(1, color="#475569", linestyle="--", linewidth=1)
ax.set_xticks(positions)
ax.set_xticklabels(labels)
ax.set_xlabel("Matrix dimensions")
ax.set_ylabel("Speedup over scalar (×)")
ax.set_title("End-to-end speedup by backend and matrix shape")
ax.grid(axis="y", alpha=0.2)
ax.legend(frameon=False, ncol=3)
fig.tight_layout()
fig.savefig(figures / "speedup.png", dpi=200)
plt.close(fig)

cuda_tiles = defaultdict(list)
for row in tile_rows:
    if row["backend"] == "cuda-tiled":
        cuda_tiles[row["case"]].append(row)

fig, ax = plt.subplots(figsize=(8, 5))
for case, entries in cuda_tiles.items():
    entries.sort(key=lambda row: int(row["tile_size"]))
    ax.plot(
        [int(row["tile_size"]) for row in entries],
        [float(row["median_ms"]) for row in entries],
        marker="o", linewidth=2, label=case.replace("_", " "),
    )

ax.set_xticks([8, 16, 32])
ax.set_xlabel("CUDA tile width (threads per block)")
ax.set_ylabel("Median end-to-end time (ms)")
ax.set_title("CUDA tile size comparison")
ax.grid(alpha=0.25)
ax.legend(frameon=False)
fig.tight_layout()
fig.savefig(figures / "cuda_tile_sizes.png", dpi=200)
plt.close(fig)

print("Saved runtime_scaling.png, speedup.png, cuda_tile_sizes.png")
