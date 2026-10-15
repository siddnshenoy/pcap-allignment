import random
import sys
import re
import subprocess


BACKEND = sys.argv[1] if len(sys.argv) > 1 else "scalar"
TILE_SIZE = sys.argv[2] if len(sys.argv) > 2 else "32"


def reference_score(a, b, match, mismatch, opening, extension):
    h = [[0] * (len(b) + 1) for _ in range(len(a) + 1)]

    for i in range(1, len(a) + 1):
        for j in range(1, len(b) + 1):
            diagonal = h[i - 1][j - 1] + (
                match if a[i - 1] == b[j - 1] else mismatch
            )
            vertical = max(
                h[i - length][j] - opening - (length - 1) * extension
                for length in range(1, i + 1)
            )
            horizontal = max(
                h[i][j - length] - opening - (length - 1) * extension
                for length in range(1, j + 1)
            )
            h[i][j] = max(0, diagonal, vertical, horizontal)

    return max(max(row) for row in h)


def check_alignment(a, b, scoring):
    match, mismatch, opening, extension = scoring
    command = [
        "./pcap", a, b, "--backend", BACKEND,
        "--tile-size", TILE_SIZE,
        "--checkpoint-rows", TILE_SIZE,
        "--match", str(match),
        "--mismatch", str(mismatch),
        "--gap-open", str(opening),
        "--gap-extend", str(extension),
    ]
    output = subprocess.run(
        command, capture_output=True, text=True, check=True
    ).stdout

    if BACKEND == "checkpoint":
        scalar_command = command.copy()
        scalar_command[4] = "scalar"
        scalar_output = subprocess.run(
            scalar_command, capture_output=True, text=True, check=True
        ).stdout
        assert output == scalar_output, (a, b, scoring, output, scalar_output)

    fields = dict(line.split(": ", 1) for line in output.splitlines())
    score = int(fields["Score"])
    expected = reference_score(a, b, *scoring)
    assert score == expected, (a, b, scoring, score, expected)

    aligned_a = fields["Sequence 1"]
    aligned_b = fields["Sequence 2"]
    assert len(aligned_a) == len(aligned_b)

    begin_a, end_a = map(int, re.findall(r"\d+", fields["Range 1"]))
    begin_b, end_b = map(int, re.findall(r"\d+", fields["Range 2"]))
    assert 0 <= begin_a <= end_a <= len(a)
    assert 0 <= begin_b <= end_b <= len(b)
    assert aligned_a.replace("-", "") == a[begin_a:end_a]
    assert aligned_b.replace("-", "") == b[begin_b:end_b]

    operations = []
    for x, y in zip(aligned_a, aligned_b):
        assert not (x == "-" and y == "-")
        operations.append(
            "D" if x == "-" else
            "I" if y == "-" else
            "=" if x == y else "X"
        )

    cigar = fields["CIGAR"]
    tokens = re.findall(r"([1-9]\d*)([=XID])", cigar)
    assert "".join(n + op for n, op in tokens) == cigar
    assert "".join(op * int(n) for n, op in tokens) == "".join(operations)

    rescored = 0
    previous = None
    for operation in operations:
        if operation == "=":
            rescored += match
        elif operation == "X":
            rescored += mismatch
        else:
            rescored -= extension if operation == previous else opening
        previous = operation

    assert rescored == score, (a, b, scoring, cigar, rescored, score)


rng = random.Random(42)
cases = [
    ("A", "A"),
    ("A", "T"),
    ("ACGT", "ACGT"),
    ("AAAACCCCGGGG", "AAAAGGGG"),
    ("AAAAGGGG", "AAAACCCCGGGG"),
    ("TTACGTAA", "GGACGTCC"),
]

for _ in range(80):
    a = "".join(rng.choice("ACGT") for _ in range(rng.randint(1, 12)))
    b = "".join(rng.choice("ACGT") for _ in range(rng.randint(1, 12)))
    cases.append((a, b))

for length_a, length_b in [(31, 33), (33, 65), (65, 17), (1, 65), (65, 1)]:
    a = "".join(rng.choice("ACGT") for _ in range(length_a))
    b = "".join(rng.choice("ACGT") for _ in range(length_b))
    cases.append((a, b))

settings = [
    (2, -1, 2, 2),
    (2, -1, 3, 1),
    (3, -2, 5, 2),
]

count = 0
for scoring in settings:
    for a, b in cases:
        check_alignment(a, b, scoring)
        count += 1
        if count == 1 or count % 10 == 0:
            print(
                f"{BACKEND}: {count}/{len(cases) * len(settings)} checks passed",
                flush=True
            )

invalid_options = [
    ["--gap-open", "0"],
    ["--gap-extend", "3"],
    ["--match", "0"],
    ["--mismatch", "1"],
    ["--match", "2junk"],
    ["--unknown", "2"],
    ["--gap-open"],
]

for options in invalid_options:
    result = subprocess.run(
        ["./pcap", "ACGT", "ACGT", "--backend", BACKEND, *options],
        capture_output=True, text=True
    )
    assert result.returncode != 0, f"Invalid options accepted: {options}"

print(f"{BACKEND}: All {count} independent score and CIGAR checks passed.")
print(f"All {len(invalid_options)} invalid-option checks passed.")
