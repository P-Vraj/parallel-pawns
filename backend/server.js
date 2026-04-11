import "dotenv/config";
import express from "express";
import cors from "cors";
import http from "node:http";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { WebSocketServer } from "ws";
import UciEngine from "./engine/uciEngine.js";

const PORT = Number(process.env.PORT ?? 3001);
const ENGINE_DEPTH = Number(process.env.ENGINE_DEPTH ?? 15);
const ENGINE_PATH = process.env.ENGINE_PATH ?? "../build/engine";
const backendRoot = path.dirname(fileURLToPath(import.meta.url));

const app = express();
app.use(cors());
app.use(express.json());

const server = http.createServer(app);
const wss = new WebSocketServer({ server });

const engine = new UciEngine({
  enginePath: ENGINE_PATH,
  depth: ENGINE_DEPTH,
  cwd: backendRoot,
});

function broadcast(message) {
  const payload = JSON.stringify(message);
  for (const client of wss.clients) {
    if (client.readyState === 1) {
      client.send(payload);
    }
  }
}

engine.on("status", (status) => {
  console.log("[backend] engine status", status);
  broadcast({
    type: "engine-status",
    status,
  });
});

engine.on("analysis", (data) => {
  broadcast({
    type: "analysis",
    data,
  });
});

engine.on("bestmove", ({ bestMove, ponder }) => {
  console.log("[backend] bestmove", { bestMove, ponder });
  broadcast({
    type: "bestmove",
    bestMove,
    ponder,
  });
});

engine.on("error", (error) => {
  console.error("[backend] engine error", error.message);
  broadcast({
    type: "error",
    message: error.message,
  });
});

app.get("/", (_request, response) => {
  response.json({
    ok: true,
    message: "Parallel Pawns backend is running. Use /health for status.",
  });
});

app.get("/health", (_request, response) => {
  response.json({
    ok: true,
    status: engine.status,
    enginePath: engine.enginePath,
  });
});

wss.on("connection", (socket) => {
  console.log("[backend] websocket connected");

  socket.send(
    JSON.stringify({
      type: "engine-status",
      status: engine.status,
    })
  );

  socket.send(
    JSON.stringify({
      type: "analysis",
      data: engine.lastAnalysis,
    })
  );

  socket.on("message", (raw) => {
    let message;

    try {
      message = JSON.parse(raw.toString());
    } catch {
      console.warn("[backend] invalid websocket payload", raw.toString());
      socket.send(
        JSON.stringify({
          type: "error",
          message: "Invalid WebSocket payload.",
        })
      );
      return;
    }

    if (message.type === "analyze" && typeof message.fen === "string") {
      const depth = Number(message.depth) || ENGINE_DEPTH;
      console.log("[backend] analyze request", {
        fen: message.fen,
        depth,
      });
      engine.analyzePosition(message.fen, depth);
      return;
    }

    if (message.type === "stop") {
      console.log("[backend] stop request");
      engine.stop();
      return;
    }

    if (message.type === "new-game") {
      console.log("[backend] new-game request");
      engine.newGame();
    }
  });

  socket.on("close", () => {
    console.log("[backend] websocket disconnected");
  });
});

engine.start();

server.listen(PORT, () => {
  console.log(`Parallel Pawns backend listening on http://localhost:${PORT}`);
});
