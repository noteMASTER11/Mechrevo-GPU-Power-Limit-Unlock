#!/usr/bin/env python3
"""Run a bounded FurMark load and collect read-only NVIDIA rail status."""
import argparse
import csv
import json
import os
from pathlib import Path
import struct
import subprocess
import time

STATUS_SIZE = 0x60EF8
RAILS = {
    "policy_13": {"limit": 210000},
    "policy_14": {"limit": 60000},
}


def find_rail_sample(data: bytes, limit: int) -> dict:
    pattern = struct.pack("<I", limit)
    candidates = []
    start = 0
    while True:
        offset = data.find(pattern, start)
        if offset < 0:
            break
        if offset % 4 == 0 and offset + 8 <= len(data):
            current = struct.unpack_from("<I", data, offset + 4)[0]
            if current <= limit * 2:
                candidates.append((offset, current))
        start = offset + 1
    # The live status member is the candidate whose following dword changes
    # between samples. Selection is completed after all captures are available.
    return {"candidates": candidates}


def summarize_candidates(rows: list[dict]) -> list[dict]:
    """Keep offsets present in every capture and summarize changing values."""
    by_offset: dict[int, list[int]] = {}
    for row in rows:
        for offset, value in row["values"]:
            by_offset.setdefault(offset, []).append(value)
    summary = []
    for offset, values in sorted(by_offset.items()):
        if len(values) != len(rows) or len(set(values)) < 2:
            continue
        summary.append({
            "offset": offset,
            "offset_hex": hex(offset),
            "samples": len(values),
            "minimum": min(values),
            "maximum": max(values),
            "distinct": len(set(values)),
        })
    return summary


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--preload", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--seconds", type=int, default=25)
    args = parser.parse_args()
    if not 5 <= args.seconds <= 60:
        parser.error("--seconds must be between 5 and 60")
    args.output.mkdir(parents=True, exist_ok=False)

    command = [
        "/usr/bin/furmark", "--demo", "furmark-gl",
        "--width", "3840", "--height", "2160", "--msaa", "2",
        "--vsync", "0", "--max-time", str(args.seconds),
        "--no-score-box",
    ]
    telemetry = []
    captures = []
    with (args.output / "furmark.log").open("w") as log, \
         (args.output / "probe.log").open("w") as probe_log:
        process = subprocess.Popen(command, cwd="/opt/furmark", stdout=log,
                                   stderr=subprocess.STDOUT)
        deadline = time.monotonic() + args.seconds + 20
        sample = 0
        try:
            while process.poll() is None and time.monotonic() < deadline:
                smi = subprocess.run([
                    "nvidia-smi",
                    "--query-gpu=timestamp,power.draw,power.draw.instant,"
                    "enforced.power.limit,utilization.gpu,temperature.gpu,"
                    "clocks_event_reasons.active",
                    "--format=csv,noheader,nounits",
                ], check=True, capture_output=True, text=True, timeout=5)
                row = next(csv.reader([smi.stdout.strip()]))
                telemetry.append(row)
                if float(row[5]) >= 80:
                    raise RuntimeError("temperature reached 80 C stop threshold")

                path = args.output / f"status-{sample:03d}.bin"
                env = dict(os.environ, LD_PRELOAD=str(args.preload.resolve()),
                           PROBE_STATUS_PATH=str(path.resolve()))
                subprocess.run(["nvidia-smi", "-q"], env=env,
                               stdout=subprocess.DEVNULL, stderr=probe_log,
                               check=True, timeout=10)
                if path.exists() and path.stat().st_size == STATUS_SIZE:
                    captures.append(path)
                sample += 1
                time.sleep(1)
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()

    rail_candidates = {name: [] for name in RAILS}
    for path in captures:
        data = path.read_bytes()
        for name, rail in RAILS.items():
            found = find_rail_sample(data, rail["limit"])["candidates"]
            rail_candidates[name].append({"file": path.name, "values": found})

    rail_summary = {
        name: summarize_candidates(rows)
        for name, rows in rail_candidates.items()
    }
    result = {
        "command": command,
        "exit_code": process.returncode,
        "telemetry": telemetry,
        "captures": len(captures),
        "rail_summary": rail_summary,
    }
    (args.output / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
