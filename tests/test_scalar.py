import subprocess

cases = [
    ("ACGT", "ACGT", 8),
    ("TTACGTAA", "GGACGTCC", 8),
    ("AAAA", "CCCC", 0),
    ("ACGTACGT", "ACGACGT", 12),
    ("ACGACGT", "ACGTACGT", 12),
    ("ACGTACGT", "ACGTTCGT", 13),
    ("A", "A", 2),
    ("A", "T", 0),
]

for a, b, expected in cases:
    result = subprocess.run(
        ["./pcap", a, b], capture_output=True, text=True, check=True
    )
    lines = result.stdout.splitlines()
    score = int(lines[0].split(":", 1)[1])
    aligned_a = lines[1].split(":", 1)[1].strip()
    aligned_b = lines[2].split(":", 1)[1].strip()

    assert score == expected, (a, b, score, expected)
    assert len(aligned_a) == len(aligned_b)
    assert aligned_a.replace("-", "") in a
    assert aligned_b.replace("-", "") in b

    rescored = 0
    for x, y in zip(aligned_a, aligned_b):
        assert not (x == "-" and y == "-")
        if x == "-" or y == "-":
            rescored -= 2
        else:
            rescored += 2 if x == y else -1

    assert rescored == score, (a, b, rescored, score)
    print(f"PASS: {a} vs {b}, score {score}")

for args in [[], ["ACGT"], ["", "ACGT"], ["ACNX", "ACGT"]]:
    result = subprocess.run(
        ["./pcap", *args], capture_output=True, text=True
    )
    assert result.returncode != 0, f"Invalid input accepted: {args}"

print("All 8 alignment checks and 4 invalid-input checks passed.")
