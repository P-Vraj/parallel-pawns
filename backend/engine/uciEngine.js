import { EventEmitter } from "node:events";
import { spawn } from "node:child_process";
import path from "node:path";
import { parseBestMoveLine, parseInfoLine } from "./uciParser.js";

function getSideToMoveFromFen(fen) {
  return fen?.trim().split(/\s+/)[1] === "b" ? "b" : "w";
}

export default class UciEngine extends EventEmitter {
  constructor({ enginePath, depth = 2, cwd = process.cwd() }) {
    super();
    this.enginePath = enginePath ? path.resolve(cwd, enginePath) : "";
    this.depth = depth;
    this.cwd = cwd;
    this.process = null;
    this.buffer = "";
    this.status = "idle";
    this.currentAnalysisSide = "w";
    this.lastAnalysis = {
      evaluation: "+0.00",
      depth: 0,
      nodes: 0,
      nps: 0,
      pv: [],
      bestMove: "--",
      rawScore: null,
    };
  }

  start() {
    if (!this.enginePath) {
      this.emit("error", new Error("ENGINE_PATH is not configured."));
      return;
    }

    if (this.process) {
      return;
    }

    this.process = spawn(this.enginePath, [], {
      cwd: path.dirname(this.enginePath),
      stdio: ["pipe", "pipe", "pipe"],
    });

    this.status = "starting";
    this.emit("status", this.status);

    this.process.on("error", (error) => {
      this.emit("error", error);
      this.status = "error";
      this.emit("status", this.status);
      this.process = null;
    });

    this.process.stdout.on("data", (chunk) => {
      this.buffer += chunk.toString();

      const lines = this.buffer.split(/\r?\n/);
      this.buffer = lines.pop() ?? "";

      for (const line of lines) {
        this.handleLine(line.trim());
      }
    });

    this.process.stderr.on("data", (chunk) => {
      this.emit("error", new Error(chunk.toString().trim()));
    });

    this.process.on("close", () => {
      this.process = null;
      this.status = "offline";
      this.emit("status", this.status);
    });

    this.sendCommand("uci");
    this.sendCommand("isready");
  }

  stop() {
    if (!this.process) {
      return;
    }

    this.sendCommand("stop");
    this.status = "ready";
    this.emit("status", this.status);
  }

  newGame() {
    if (!this.process) {
      this.start();
    }

    this.sendCommand("ucinewgame");
    this.sendCommand("isready");
  }

  analyzePosition(fen, depth = this.depth) {
    if (!this.process) {
      this.start();
    }

    this.currentAnalysisSide = getSideToMoveFromFen(fen);
    this.stop();
    this.sendCommand(`position fen ${fen}`);
    this.sendCommand(`go depth ${depth}`);
    this.status = "searching";
    this.emit("status", this.status);
  }

  sendCommand(command) {
    if (!this.process?.stdin.writable) {
      return;
    }

    this.process.stdin.write(`${command}\n`);
  }

  handleLine(line) {
    if (!line) {
      return;
    }

    if (line === "uciok") {
      this.status = "ready";
      this.emit("status", this.status);
      return;
    }

    if (line === "readyok") {
      if (this.status !== "searching") {
        this.status = "ready";
        this.emit("status", this.status);
      }
      return;
    }

    const info = parseInfoLine(line, this.currentAnalysisSide);
    if (info) {
      this.lastAnalysis = {
        ...this.lastAnalysis,
        ...Object.fromEntries(
          Object.entries(info).filter(([, value]) => value !== null)
        ),
      };
      this.emit("analysis", this.lastAnalysis);
      return;
    }

    const bestMove = parseBestMoveLine(line);
    if (bestMove) {
      this.lastAnalysis = {
        ...this.lastAnalysis,
        bestMove: bestMove.bestMove,
      };
      this.status = "ready";
      this.emit("analysis", this.lastAnalysis);
      this.emit("bestmove", bestMove);
      this.emit("status", this.status);
    }
  }
}
