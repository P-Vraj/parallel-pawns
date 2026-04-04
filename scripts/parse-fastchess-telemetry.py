#!/usr/bin/env python3

from __future__ import annotations

import argparse
import csv
import json
import re
import sys
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path


STARTED_GAME_RE = re.compile(r"Started game (?P<game>\d+) of \d+ \((?P<white>.+?) vs (?P<black>.+?)\)")
FINISHED_GAME_RE = re.compile(r"Finished game (?P<game>\d+) \((?P<white>.+?) vs (?P<black>.+?)\): (?P<result>\S+)")
TELEMETRY_RE = re.compile(r"info string telemetry (?P<payload>.*)$")

EXPECTED_FIELDS = (
    "nodes",
    "qnodes",
    "elapsed_ms",
    "tt_hits",
    "tt_misses",
    "tt_writes",
    "tt_rewrites",
    "completed_depth",
)

GAME_ID_PATTERNS = (
    re.compile(r"\bgame\s+(?P<game>\d+)\b", re.IGNORECASE),
    re.compile(r"\bg\s*(?P<game>\d+)\b", re.IGNORECASE),
    re.compile(r"\[#(?P<game>\d+)\]"),
)


@dataclass
class GameInfo:
    white: str
    black: str
    result: str | None = None


@dataclass
class Aggregate:
    searches: int = 0
    nodes: int = 0
    qnodes: int = 0
    elapsed_ms: int = 0
    tt_hits: int = 0
    tt_misses: int = 0
    tt_writes: int = 0
    tt_rewrites: int = 0
    completed_depth_sum: int = 0

    def add(self, fields: dict[str, int]) -> None:
        self.searches += 1
        self.nodes += fields["nodes"]
        self.qnodes += fields["qnodes"]
        self.elapsed_ms += fields["elapsed_ms"]
        self.tt_hits += fields["tt_hits"]
        self.tt_misses += fields["tt_misses"]
        self.tt_writes += fields["tt_writes"]
        self.tt_rewrites += fields["tt_rewrites"]
        self.completed_depth_sum += fields["completed_depth"]

    def merge(self, other: "Aggregate") -> None:
        self.searches += other.searches
        self.nodes += other.nodes
        self.qnodes += other.qnodes
        self.elapsed_ms += other.elapsed_ms
        self.tt_hits += other.tt_hits
        self.tt_misses += other.tt_misses
        self.tt_writes += other.tt_writes
        self.tt_rewrites += other.tt_rewrites
        self.completed_depth_sum += other.completed_depth_sum

    def as_row(self) -> dict[str, str]:
        total_nodes = self.nodes + self.qnodes
        tt_accesses = self.tt_hits + self.tt_misses
        tt_writes_total = self.tt_writes + self.tt_rewrites
        avg_depth = self.completed_depth_sum / self.searches if self.searches else 0.0
        nps = (1000.0 * total_nodes / self.elapsed_ms) if self.elapsed_ms else 0.0
        hit_rate = (100.0 * self.tt_hits / tt_accesses) if tt_accesses else 0.0
        rewrite_rate = (100.0 * self.tt_rewrites / tt_writes_total) if tt_writes_total else 0.0

        return {
            "searches": str(self.searches),
            "nodes": str(self.nodes),
            "qnodes": str(self.qnodes),
            "elapsed_ms": str(self.elapsed_ms),
            "nps": f"{nps:.2f}",
            "tt_hits": str(self.tt_hits),
            "tt_misses": str(self.tt_misses),
            "tt_hit_rate_pct": f"{hit_rate:.2f}",
            "tt_writes": str(self.tt_writes),
            "tt_rewrites": str(self.tt_rewrites),
            "tt_rewrite_rate_pct": f"{rewrite_rate:.2f}",
            "avg_completed_depth": f"{avg_depth:.2f}",
        }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Parse fastchess trace logs that contain engine telemetry lines emitted as `info string telemetry ...`."
    )
    parser.add_argument(
        "input",
        type=Path,
        help="Path to a fastchess games.log file or to a run directory containing games.log",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        help="Directory for generated summaries. Defaults to the log directory.",
    )
    return parser.parse_args()


def resolve_log_path(input_path: Path) -> Path:
    if input_path.is_dir():
        return input_path / "games.log"
    return input_path


def parse_payload(payload: str) -> dict[str, int]:
    fields: dict[str, int] = {}
    for token in payload.split():
        if "=" not in token:
            continue
        key, value = token.split("=", 1)
        if key not in EXPECTED_FIELDS:
            continue
        fields[key] = int(value)

    missing = [field for field in EXPECTED_FIELDS if field not in fields]
    if missing:
        raise ValueError(f"missing telemetry fields: {', '.join(missing)}")
    return fields


def detect_game_id(line: str) -> int | None:
    for pattern in GAME_ID_PATTERNS:
        match = pattern.search(line)
        if match:
            return int(match.group("game"))
    return None


def detect_engine_name(prefix: str, engine_names: list[str]) -> str | None:
    for name in engine_names:
        if name and name in prefix:
            return name
    return None


def write_csv(path: Path, rows: list[dict[str, str]], fieldnames: list[str]) -> None:
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def main() -> int:
    args = parse_args()
    log_path = resolve_log_path(args.input)
    if not log_path.is_file():
        print(f"missing log file: {log_path}", file=sys.stderr)
        return 1

    output_dir = args.output_dir or log_path.parent
    output_dir.mkdir(parents=True, exist_ok=True)

    lines = log_path.read_text(encoding="utf-8", errors="replace").splitlines()

    games: dict[int, GameInfo] = {}
    for line in lines:
        if match := STARTED_GAME_RE.search(line):
            game_id = int(match.group("game"))
            games[game_id] = GameInfo(white=match.group("white"), black=match.group("black"))
        elif match := FINISHED_GAME_RE.search(line):
            game_id = int(match.group("game"))
            existing = games.get(game_id, GameInfo(white=match.group("white"), black=match.group("black")))
            existing.result = match.group("result")
            games[game_id] = existing

    engine_names = sorted({info.white for info in games.values()} | {info.black for info in games.values()}, key=len, reverse=True)

    game_engine_aggregates: dict[tuple[int, str], Aggregate] = defaultdict(Aggregate)
    skipped_lines: list[str] = []
    unmatched_context = 0

    for line in lines:
        match = TELEMETRY_RE.search(line)
        if not match:
            continue

        payload = match.group("payload")
        prefix = line[: match.start()]
        try:
            fields = parse_payload(payload)
        except ValueError:
            skipped_lines.append(line)
            continue

        game_id = detect_game_id(prefix)
        engine_name = detect_engine_name(prefix, engine_names)

        if game_id is None or engine_name is None:
            unmatched_context += 1
            skipped_lines.append(line)
            continue

        game_engine_aggregates[(game_id, engine_name)].add(fields)

    if not game_engine_aggregates:
        print(
            "no telemetry lines with detectable game and engine context were found; make sure fastchess trace logging is enabled",
            file=sys.stderr,
        )
        return 2

    per_game_rows: list[dict[str, str]] = []
    per_run_aggregates: dict[str, Aggregate] = defaultdict(Aggregate)

    for (game_id, engine_name), aggregate in sorted(game_engine_aggregates.items()):
        info = games.get(game_id)
        row = {
            "game": str(game_id),
            "engine": engine_name,
            "color": "white" if info and info.white == engine_name else "black",
            "opponent": (info.black if info and info.white == engine_name else info.white) if info else "",
            "result": info.result if info and info.result else "",
        }
        row.update(aggregate.as_row())
        per_game_rows.append(row)
        per_run_aggregates[engine_name].merge(aggregate)

    per_run_rows: list[dict[str, str]] = []
    for engine_name, aggregate in sorted(per_run_aggregates.items()):
        row = {"engine": engine_name}
        row.update(aggregate.as_row())
        per_run_rows.append(row)

    per_game_path = output_dir / "telemetry_per_game.csv"
    per_run_path = output_dir / "telemetry_per_run.csv"
    summary_path = output_dir / "telemetry_summary.json"

    write_csv(
        per_game_path,
        per_game_rows,
        [
            "game",
            "engine",
            "color",
            "opponent",
            "result",
            "searches",
            "nodes",
            "qnodes",
            "elapsed_ms",
            "nps",
            "tt_hits",
            "tt_misses",
            "tt_hit_rate_pct",
            "tt_writes",
            "tt_rewrites",
            "tt_rewrite_rate_pct",
            "avg_completed_depth",
        ],
    )
    write_csv(
        per_run_path,
        per_run_rows,
        [
            "engine",
            "searches",
            "nodes",
            "qnodes",
            "elapsed_ms",
            "nps",
            "tt_hits",
            "tt_misses",
            "tt_hit_rate_pct",
            "tt_writes",
            "tt_rewrites",
            "tt_rewrite_rate_pct",
            "avg_completed_depth",
        ],
    )

    summary = {
        "log_path": str(log_path),
        "games_detected": len(games),
        "telemetry_records": sum(aggregate.searches for aggregate in game_engine_aggregates.values()),
        "unmatched_telemetry_lines": unmatched_context,
        "skipped_telemetry_lines": len(skipped_lines),
        "outputs": {
            "per_game_csv": str(per_game_path),
            "per_run_csv": str(per_run_path),
        },
    }
    summary_path.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")

    print(f"Wrote per-game summary to {per_game_path}")
    print(f"Wrote per-run summary to {per_run_path}")
    print(f"Wrote parser metadata to {summary_path}")
    if skipped_lines:
        print(f"Skipped {len(skipped_lines)} telemetry lines that could not be parsed cleanly.", file=sys.stderr)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
