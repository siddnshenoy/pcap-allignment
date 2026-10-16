import csv
import shutil
import statistics
import subprocess
from collections import defaultdict
from pathlib import Path

results_dir = Path("results")
results_dir.mkdir(exist_ok=True)
measurements = defaultdict(list)
metadata = {}

for trial in range(1, 4):
    log_path = results_dir / f"benchmark_trial_{trial}.log"
    with log_path.open("w") as log:
        subprocess.run(
            ["python3", "tests/benchmark.py"],
            stdout=log, stderr=subprocess.STDOUT, check=True
        )

    source = results_dir / "benchmark.csv"
    destination = results_dir / f"benchmark_trial_{trial}.csv"
    shutil.copyfile(source, destination)

    with destination.open(newline="") as file:
        rows = list(csv.DictReader(file))

    for row in rows:
        key = (row["case"], row["backend"])
        measurements[key].append(float(row["median_ms"]))
        if key in metadata:
            assert row["score"] == metadata[key]["score"], key
        else:
            metadata[key] = row

    print(f"Trial {trial}/3 complete: {len(rows)} cases saved.", flush=True)

summary_path = results_dir / "benchmark_summary.csv"
with summary_path.open("w", newline="") as file:
    writer = csv.writer(file)
    writer.writerow([
        "case", "length_a", "length_b", "backend", "score",
        "trials", "median_ms", "min_trial_ms", "max_trial_ms",
    ])

    for key, values in measurements.items():
        assert len(values) == 3, (key, values)
        row = metadata[key]
        writer.writerow([
            row["case"], row["length_a"], row["length_b"],
            row["backend"], row["score"], 3,
            statistics.median(values), min(values), max(values),
        ])

print(f"Three-trial summary saved to {summary_path}")
