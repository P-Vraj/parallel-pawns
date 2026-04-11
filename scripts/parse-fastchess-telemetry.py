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


LOG_CONTEXT_RE = re.compile(r"^.*?<(?P<context>\s*\d+)>\s+(?P<message>.*)$")
ENGINE_LINE_RE = re.compile(r"^\[Engine\]\s+\[[^\]]+\]\s+<(?P<context>\s*\d+)>\s+(?P<engine>.+?)\s+--->\s+(?P<body>.*)$")
STARTED_GAME_RE = re.compile(r"Game (?P<game>\d+) between (?P<white>.+?) and (?P<black>.+?) starting")
FINISHED_GAME_RE = re.compile(r"Game (?P<game>\d+) between (?P<white>.+?) and (?P<black>.+?) finished")
TELEMETRY_RE = re.compile(r"info string telemetry (?P<payload>.*)$")
DIST_WORKER_RE = re.compile(r"info string dist_worker (?P<payload>.*)$")

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

EXPECTED_DIST_WORKER_FIELDS = (
    "endpoint",
    "mode",
    "fallback_local",
    "assigned",
    "score",
    "depth",
    "nodes",
    "qnodes",
    "tt_hits",
    "tt_misses",
    "tt_writes",
    "tt_rewrites",
    "stopped",
    "bestmove",
)

OPTIONAL_DIST_WORKER_FIELDS = (
    "tt_hits",
    "tt_misses",
    "tt_writes",
    "tt_rewrites",
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


@dataclass
class DistWorkerAggregate:
    reports: int = 0
    remote_reports: int = 0
    local_reports: int = 0
    fallback_local_reports: int = 0
    assigned_root_moves: int = 0
    nodes: int = 0
    qnodes: int = 0
    tt_hits: int = 0
    tt_misses: int = 0
    tt_writes: int = 0
    tt_rewrites: int = 0
    stopped_reports: int = 0
    completed_depth_sum: int = 0

    def add(self, fields: dict[str, str]) -> None:
        self.reports += 1
        self.remote_reports += 1 if fields["mode"] == "remote" else 0
        self.local_reports += 1 if fields["mode"] == "local" else 0
        self.fallback_local_reports += int(fields["fallback_local"])
        self.assigned_root_moves += int(fields["assigned"])
        self.nodes += int(fields["nodes"])
        self.qnodes += int(fields["qnodes"])
        self.tt_hits += int(fields["tt_hits"])
        self.tt_misses += int(fields["tt_misses"])
        self.tt_writes += int(fields["tt_writes"])
        self.tt_rewrites += int(fields["tt_rewrites"])
        self.stopped_reports += int(fields["stopped"])
        self.completed_depth_sum += int(fields["depth"])

    def merge(self, other: "DistWorkerAggregate") -> None:
        self.reports += other.reports
        self.remote_reports += other.remote_reports
        self.local_reports += other.local_reports
        self.fallback_local_reports += other.fallback_local_reports
        self.assigned_root_moves += other.assigned_root_moves
        self.nodes += other.nodes
        self.qnodes += other.qnodes
        self.tt_hits += other.tt_hits
        self.tt_misses += other.tt_misses
        self.tt_writes += other.tt_writes
        self.tt_rewrites += other.tt_rewrites
        self.stopped_reports += other.stopped_reports
        self.completed_depth_sum += other.completed_depth_sum

    def as_row(self) -> dict[str, str]:
        total_nodes = self.nodes + self.qnodes
        tt_accesses = self.tt_hits + self.tt_misses
        tt_writes_total = self.tt_writes + self.tt_rewrites
        avg_depth = self.completed_depth_sum / self.reports if self.reports else 0.0
        hit_rate = (100.0 * self.tt_hits / tt_accesses) if tt_accesses else 0.0
        rewrite_rate = (100.0 * self.tt_rewrites / tt_writes_total) if tt_writes_total else 0.0

        return {
            "worker_reports": str(self.reports),
            "remote_reports": str(self.remote_reports),
            "local_reports": str(self.local_reports),
            "fallback_local_reports": str(self.fallback_local_reports),
            "assigned_root_moves": str(self.assigned_root_moves),
            "worker_nodes": str(self.nodes),
            "worker_qnodes": str(self.qnodes),
            "worker_total_nodes": str(total_nodes),
            "worker_tt_hits": str(self.tt_hits),
            "worker_tt_misses": str(self.tt_misses),
            "worker_tt_hit_rate_pct": f"{hit_rate:.2f}",
            "worker_tt_writes": str(self.tt_writes),
            "worker_tt_rewrites": str(self.tt_rewrites),
            "worker_tt_rewrite_rate_pct": f"{rewrite_rate:.2f}",
            "worker_stopped_reports": str(self.stopped_reports),
            "worker_avg_completed_depth": f"{avg_depth:.2f}",
        }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Parse fastchess trace logs that contain engine telemetry lines emitted as `info string telemetry ...`."
    )
    parser.add_argument(
        "input",
        type=Path,
        help="Path to a fastchess trace.log file or to a run directory containing trace.log",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        help="Directory for generated summaries. Defaults to the log directory.",
    )
    return parser.parse_args()


def resolve_log_path(input_path: Path) -> Path:
    if input_path.is_dir():
        return input_path / "trace.log"
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


def parse_dist_worker_payload(payload: str) -> dict[str, str]:
    fields: dict[str, str] = {}
    for token in payload.split():
        if "=" not in token:
            continue
        key, value = token.split("=", 1)
        if key not in EXPECTED_DIST_WORKER_FIELDS:
            continue
        fields[key] = value

    for field in OPTIONAL_DIST_WORKER_FIELDS:
        fields.setdefault(field, "0")

    missing = [field for field in EXPECTED_DIST_WORKER_FIELDS if field not in fields]
    if missing:
        raise ValueError(f"missing dist_worker fields: {', '.join(missing)}")
    return fields


def parse_context(line: str) -> tuple[str, str] | None:
    match = LOG_CONTEXT_RE.match(line)
    if not match:
        return None
    return match.group("context").strip(), match.group("message")


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
    active_games_by_context: dict[str, int] = {}
    game_engine_aggregates: dict[tuple[int, str], Aggregate] = defaultdict(Aggregate)
    game_engine_worker_aggregates: dict[tuple[int, str], DistWorkerAggregate] = defaultdict(DistWorkerAggregate)
    game_engine_endpoint_worker_aggregates: dict[tuple[int, str, str], DistWorkerAggregate] = defaultdict(DistWorkerAggregate)
    skipped_lines: list[str] = []
    unmatched_context = 0

    for line in lines:
        parsed_context = parse_context(line)
        if not parsed_context:
            continue
        context_id, message = parsed_context

        if match := STARTED_GAME_RE.search(message):
            game_id = int(match.group("game"))
            games[game_id] = GameInfo(white=match.group("white"), black=match.group("black"))
            active_games_by_context[context_id] = game_id
            continue

        if match := FINISHED_GAME_RE.search(message):
            game_id = int(match.group("game"))
            existing = games.get(game_id, GameInfo(white=match.group("white"), black=match.group("black")))
            games[game_id] = existing
            active_games_by_context.pop(context_id, None)
            continue

        match = ENGINE_LINE_RE.match(line)
        if not match:
            continue

        body = match.group("body")
        telemetry_match = TELEMETRY_RE.search(body)
        context_id = match.group("context").strip()
        engine_name = match.group("engine").strip()

        game_id = active_games_by_context.get(context_id)

        if game_id is None or engine_name is None:
            unmatched_context += 1
            skipped_lines.append(line)
            continue

        if telemetry_match:
            payload = telemetry_match.group("payload")
            try:
                fields = parse_payload(payload)
            except ValueError:
                skipped_lines.append(line)
                continue
            game_engine_aggregates[(game_id, engine_name)].add(fields)
            continue

        dist_worker_match = DIST_WORKER_RE.search(body)
        if dist_worker_match:
            payload = dist_worker_match.group("payload")
            try:
                fields = parse_dist_worker_payload(payload)
            except ValueError:
                skipped_lines.append(line)
                continue
            game_engine_worker_aggregates[(game_id, engine_name)].add(fields)
            game_engine_endpoint_worker_aggregates[(game_id, engine_name, fields["endpoint"])].add(fields)

    if not game_engine_aggregates:
        print(
            "no telemetry lines with detectable game and engine context were found; make sure fastchess trace logging is enabled",
            file=sys.stderr,
        )
        return 2

    per_game_rows: list[dict[str, str]] = []
    per_run_aggregates: dict[str, Aggregate] = defaultdict(Aggregate)
    per_game_worker_rows: list[dict[str, str]] = []
    per_run_worker_aggregates: dict[str, DistWorkerAggregate] = defaultdict(DistWorkerAggregate)
    per_endpoint_worker_rows: list[dict[str, str]] = []

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
        if (game_id, engine_name) in game_engine_worker_aggregates:
            row.update(game_engine_worker_aggregates[(game_id, engine_name)].as_row())
        per_game_rows.append(row)
        per_run_aggregates[engine_name].merge(aggregate)
        if (game_id, engine_name) in game_engine_worker_aggregates:
            per_run_worker_aggregates[engine_name].merge(game_engine_worker_aggregates[(game_id, engine_name)])

    for (game_id, engine_name), aggregate in sorted(game_engine_worker_aggregates.items()):
        info = games.get(game_id)
        row = {
            "game": str(game_id),
            "engine": engine_name,
            "color": "white" if info and info.white == engine_name else "black",
            "opponent": (info.black if info and info.white == engine_name else info.white) if info else "",
            "result": info.result if info and info.result else "",
        }
        row.update(aggregate.as_row())
        per_game_worker_rows.append(row)

    per_run_rows: list[dict[str, str]] = []
    for engine_name, aggregate in sorted(per_run_aggregates.items()):
        row = {"engine": engine_name}
        row.update(aggregate.as_row())
        if engine_name in per_run_worker_aggregates:
            row.update(per_run_worker_aggregates[engine_name].as_row())
        per_run_rows.append(row)

    for (game_id, engine_name, endpoint), aggregate in sorted(game_engine_endpoint_worker_aggregates.items()):
        info = games.get(game_id)
        row = {
            "game": str(game_id),
            "engine": engine_name,
            "endpoint": endpoint,
            "color": "white" if info and info.white == engine_name else "black",
            "opponent": (info.black if info and info.white == engine_name else info.white) if info else "",
            "result": info.result if info and info.result else "",
        }
        row.update(aggregate.as_row())
        per_endpoint_worker_rows.append(row)

    per_game_path = output_dir / "telemetry_per_game.csv"
    per_run_path = output_dir / "telemetry_per_run.csv"
    per_game_worker_path = output_dir / "telemetry_workers_per_game.csv"
    per_endpoint_worker_path = output_dir / "telemetry_workers_per_endpoint.csv"
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
            "worker_reports",
            "remote_reports",
            "local_reports",
            "fallback_local_reports",
            "assigned_root_moves",
            "worker_nodes",
            "worker_qnodes",
            "worker_total_nodes",
            "worker_tt_hits",
            "worker_tt_misses",
            "worker_tt_hit_rate_pct",
            "worker_tt_writes",
            "worker_tt_rewrites",
            "worker_tt_rewrite_rate_pct",
            "worker_stopped_reports",
            "worker_avg_completed_depth",
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
            "worker_reports",
            "remote_reports",
            "local_reports",
            "fallback_local_reports",
            "assigned_root_moves",
            "worker_nodes",
            "worker_qnodes",
            "worker_total_nodes",
            "worker_tt_hits",
            "worker_tt_misses",
            "worker_tt_hit_rate_pct",
            "worker_tt_writes",
            "worker_tt_rewrites",
            "worker_tt_rewrite_rate_pct",
            "worker_stopped_reports",
            "worker_avg_completed_depth",
        ],
    )
    write_csv(
        per_game_worker_path,
        per_game_worker_rows,
        [
            "game",
            "engine",
            "color",
            "opponent",
            "result",
            "worker_reports",
            "remote_reports",
            "local_reports",
            "fallback_local_reports",
            "assigned_root_moves",
            "worker_nodes",
            "worker_qnodes",
            "worker_total_nodes",
            "worker_tt_hits",
            "worker_tt_misses",
            "worker_tt_hit_rate_pct",
            "worker_tt_writes",
            "worker_tt_rewrites",
            "worker_tt_rewrite_rate_pct",
            "worker_stopped_reports",
            "worker_avg_completed_depth",
        ],
    )
    write_csv(
        per_endpoint_worker_path,
        per_endpoint_worker_rows,
        [
            "game",
            "engine",
            "endpoint",
            "color",
            "opponent",
            "result",
            "worker_reports",
            "remote_reports",
            "local_reports",
            "fallback_local_reports",
            "assigned_root_moves",
            "worker_nodes",
            "worker_qnodes",
            "worker_total_nodes",
            "worker_tt_hits",
            "worker_tt_misses",
            "worker_tt_hit_rate_pct",
            "worker_tt_writes",
            "worker_tt_rewrites",
            "worker_tt_rewrite_rate_pct",
            "worker_stopped_reports",
            "worker_avg_completed_depth",
        ],
    )

    summary = {
        "log_path": str(log_path),
        "games_detected": len(games),
        "telemetry_records": sum(aggregate.searches for aggregate in game_engine_aggregates.values()),
        "dist_worker_records": sum(aggregate.reports for aggregate in game_engine_worker_aggregates.values()),
        "unmatched_telemetry_lines": unmatched_context,
        "skipped_telemetry_lines": len(skipped_lines),
        "outputs": {
            "per_game_csv": str(per_game_path),
            "per_run_csv": str(per_run_path),
            "workers_per_game_csv": str(per_game_worker_path),
            "workers_per_endpoint_csv": str(per_endpoint_worker_path),
        },
    }
    summary_path.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")

    print(f"Wrote per-game summary to {per_game_path}")
    print(f"Wrote per-run summary to {per_run_path}")
    print(f"Wrote worker per-game summary to {per_game_worker_path}")
    print(f"Wrote worker per-endpoint summary to {per_endpoint_worker_path}")
    print(f"Wrote parser metadata to {summary_path}")
    if skipped_lines:
        print(f"Skipped {len(skipped_lines)} telemetry lines that could not be parsed cleanly.", file=sys.stderr)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
