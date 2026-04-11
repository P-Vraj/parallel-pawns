function normalizeScore(score, sideToMove) {
  if (!score) {
    return null;
  }

  const multiplier = sideToMove === "b" ? -1 : 1;
  return {
    unit: score.unit,
    value: score.value * multiplier,
  };
}

function formatEvaluation(score) {
  if (!score) {
    return "--";
  }

  if (score.unit === "mate") {
    const prefix = score.value >= 0 ? "+M" : "-M";
    return `${prefix}${Math.abs(score.value)}`;
  }

  const pawns = score.value / 100;
  const sign = pawns >= 0 ? "+" : "";
  return `${sign}${pawns.toFixed(2)}`;
}

export function parseInfoLine(line, sideToMove = "w") {
  const tokens = line.trim().split(/\s+/);
  if (tokens[0] !== "info") {
    return null;
  }

  const info = {
    depth: null,
    nodes: null,
    nps: null,
    pv: null,
    evaluation: null,
    rawScore: null,
  };

  for (let index = 1; index < tokens.length; index += 1) {
    const token = tokens[index];

    if (token === "depth" && tokens[index + 1]) {
      info.depth = Number(tokens[index + 1]);
      index += 1;
      continue;
    }

    if (token === "nodes" && tokens[index + 1]) {
      info.nodes = Number(tokens[index + 1]);
      index += 1;
      continue;
    }

    if (token === "nps" && tokens[index + 1]) {
      info.nps = Number(tokens[index + 1]);
      index += 1;
      continue;
    }

    if (token === "score" && tokens[index + 2]) {
      const unit = tokens[index + 1];
      const value = Number(tokens[index + 2]);
      info.rawScore = normalizeScore({ unit, value }, sideToMove);
      info.evaluation = formatEvaluation(info.rawScore);
      index += 2;
      continue;
    }

    if (token === "pv") {
      info.pv = tokens.slice(index + 1);
      break;
    }
  }

  return info;
}

export function parseBestMoveLine(line) {
  const tokens = line.trim().split(/\s+/);
  if (tokens[0] !== "bestmove") {
    return null;
  }

  return {
    bestMove: tokens[1] ?? "--",
    ponder: tokens[3] ?? null,
  };
}
