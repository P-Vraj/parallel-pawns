import { useEffect, useMemo, useRef, useState } from "react";
import { Chess } from "chess.js";
import ChessBoard from "./components/ChessBoard";
import AnalysisPanel from "./components/AnalysisPanel";

const STARTING_FEN = new Chess().fen();
const PLAYER_COLOR = "w";
const ENGINE_COLOR = "b";

function createSocketUrl() {
  if (import.meta.env.VITE_WS_URL) {
    return import.meta.env.VITE_WS_URL;
  }

  const protocol = window.location.protocol === "https:" ? "wss:" : "ws:";
  return `${protocol}//${window.location.hostname}:3001`;
}

function isPlayerPiece(piece) {
  return piece?.pieceType?.[0] === PLAYER_COLOR;
}

function getLastMoveStyles(lastMove) {
  if (!lastMove?.from || !lastMove?.to) {
    return {};
  }

  return {
    [lastMove.from]: {
      background: "rgba(102, 178, 255, 0.34)",
    },
    [lastMove.to]: {
      background: "rgba(102, 178, 255, 0.5)",
    },
  };
}

export default function App() {
  const [game, setGame] = useState(() => new Chess());
  const [fenInput, setFenInput] = useState(STARTING_FEN);
  const [analysis, setAnalysis] = useState({
    evaluation: "+0.00",
    depth: 0,
    nodes: 0,
    nps: 0,
    pv: [],
    bestMove: "--",
    rawScore: null,
  });
  const [engineStatus, setEngineStatus] = useState("connecting");
  const [connectionState, setConnectionState] = useState("Disconnected");
  const [boardOrientation, setBoardOrientation] = useState("white");
  const [fenError, setFenError] = useState("");
  const [moveFrom, setMoveFrom] = useState("");
  const [optionSquares, setOptionSquares] = useState({});
  const [lastMove, setLastMove] = useState(null);
  const socketRef = useRef(null);
  const pendingFenRef = useRef(STARTING_FEN);
  const awaitingEngineMoveRef = useRef(false);

  const currentFen = game.fen();
  const playerTurn = game.turn() === PLAYER_COLOR;
  const sideToMove = useMemo(
    () => (game.turn() === "w" ? "White to move" : "Black to move"),
    [game]
  );
  const boardSquareStyles = useMemo(
    () => ({
      ...getLastMoveStyles(lastMove),
      ...optionSquares,
    }),
    [lastMove, optionSquares]
  );

  useEffect(() => {
    pendingFenRef.current = currentFen;
    setFenInput(currentFen);
  }, [currentFen]);

  useEffect(() => {
    const socket = new WebSocket(createSocketUrl());
    socketRef.current = socket;

    socket.addEventListener("open", () => {
      console.log("[frontend] websocket connected");
      setConnectionState("Connected");
      setEngineStatus("ready");

      if (pendingFenRef.current.split(" ")[1] === ENGINE_COLOR) {
        awaitingEngineMoveRef.current = true;
        console.log("[frontend] requesting opening engine move", pendingFenRef.current);
        socket.send(
          JSON.stringify({
            type: "analyze",
            fen: pendingFenRef.current,
          })
        );
      }
    });

    socket.addEventListener("close", () => {
      console.log("[frontend] websocket disconnected");
      setConnectionState("Disconnected");
      setEngineStatus("offline");
    });

    socket.addEventListener("error", () => {
      console.error("[frontend] websocket error");
      setConnectionState("Connection error");
      setEngineStatus("error");
    });

    socket.addEventListener("message", (event) => {
      let payload;

      try {
        payload = JSON.parse(event.data);
      } catch {
        return;
      }

      if (payload.type === "engine-status") {
        setEngineStatus(payload.status ?? "unknown");
      }

      if (payload.type === "analysis") {
        setAnalysis((previous) => ({
          ...previous,
          ...payload.data,
        }));
      }

      if (payload.type === "bestmove") {
        console.log("[frontend] bestmove received", payload.bestMove);
        setAnalysis((previous) => ({
          ...previous,
          bestMove: payload.bestMove ?? "--",
        }));

        if (!awaitingEngineMoveRef.current || !payload.bestMove || payload.bestMove === "--") {
          return;
        }

        const nextGame = new Chess(pendingFenRef.current);
        const move = nextGame.move({
          from: payload.bestMove.slice(0, 2),
          to: payload.bestMove.slice(2, 4),
          promotion: payload.bestMove.slice(4, 5) || "q",
        });

        if (!move) {
          console.warn("[frontend] failed to apply engine move", payload.bestMove);
          awaitingEngineMoveRef.current = false;
          return;
        }

        console.log("[frontend] engine move applied", move.san, nextGame.fen());
        awaitingEngineMoveRef.current = false;
        clearMoveSelection();
        setLastMove({ from: move.from, to: move.to });
        setGame(nextGame);
        pendingFenRef.current = nextGame.fen();
      }

      if (payload.type === "error") {
        console.error("[frontend] backend error", payload.message ?? "Engine error");
        setEngineStatus("error");
        setConnectionState(payload.message ?? "Engine error");
      }
    });

    return () => {
      socket.close();
    };
  }, []);

  function clearMoveSelection() {
    setMoveFrom("");
    setOptionSquares({});
  }

  function requestEngineMove(fen) {
    pendingFenRef.current = fen;
    awaitingEngineMoveRef.current = true;
    console.log("[frontend] requesting engine move", fen);

    if (socketRef.current?.readyState === WebSocket.OPEN) {
      socketRef.current.send(
        JSON.stringify({
          type: "analyze",
          fen,
        })
      );
    }
  }

  function applyGameFromFen(nextFen) {
    const nextGame = new Chess(nextFen);
    console.log("[frontend] FEN applied", nextFen);
    clearMoveSelection();
    setLastMove(null);
    setGame(nextGame);
    setFenError("");

    if (nextGame.turn() === ENGINE_COLOR) {
      requestEngineMove(nextFen);
    } else {
      awaitingEngineMoveRef.current = false;
      pendingFenRef.current = nextFen;
    }
  }

  function getMoveOptions(square) {
    const previewGame = new Chess(currentFen);
    const pieceOnSquare = previewGame.get(square);
    const moves = previewGame.moves({
      square,
      verbose: true,
    });

    if (!pieceOnSquare || pieceOnSquare.color !== PLAYER_COLOR || !playerTurn || moves.length === 0) {
      setOptionSquares({});
      return false;
    }

    const nextSquares = {};

    for (const move of moves) {
      const targetPiece = previewGame.get(move.to);
      nextSquares[move.to] = {
        background: targetPiece && targetPiece.color !== pieceOnSquare.color
          ? "radial-gradient(circle, rgba(0, 0, 0, 0.18) 78%, transparent 80%)"
          : "radial-gradient(circle, rgba(0, 0, 0, 0.18) 23%, transparent 24%)",
        borderRadius: "50%",
      };
    }

    nextSquares[square] = {
      background: "rgba(255, 215, 0, 0.36)",
    };

    setOptionSquares(nextSquares);
    return true;
  }

  function applyPlayerMove(fromSquare, toSquare) {
    const nextGame = new Chess(currentFen);
    let move;

    try {
      move = nextGame.move({
        from: fromSquare,
        to: toSquare,
        promotion: "q",
      });
    } catch {
      return false;
    }

    if (!move) {
      return false;
    }

    console.log("[frontend] player move accepted", move.san, nextGame.fen());
    clearMoveSelection();
    setLastMove({ from: move.from, to: move.to });
    setGame(nextGame);
    requestEngineMove(nextGame.fen());
    return true;
  }

  function handlePieceDrop({ piece, sourceSquare, targetSquare }) {
    if (!sourceSquare || !targetSquare || !playerTurn || !isPlayerPiece(piece)) {
      console.warn("[frontend] rejected drop", {
        sourceSquare,
        targetSquare,
        playerTurn,
        piece,
      });
      return false;
    }

    const didMove = applyPlayerMove(sourceSquare, targetSquare);

    if (!didMove) {
      console.warn("[frontend] illegal move", { sourceSquare, targetSquare });
    }

    return didMove;
  }

  function handleSquareClick({ square }) {
    if (!playerTurn) {
      return;
    }

    const clickedPiece = game.get(square);
    const clickedPlayerPiece = clickedPiece?.color === PLAYER_COLOR;

    if (!moveFrom) {
      if (!clickedPlayerPiece) {
        clearMoveSelection();
        return;
      }

      const hasMoveOptions = getMoveOptions(square);
      if (hasMoveOptions) {
        setMoveFrom(square);
      }
      return;
    }

    const didMove = applyPlayerMove(moveFrom, square);
    if (didMove) {
      return;
    }

    if (clickedPlayerPiece) {
      const hasMoveOptions = getMoveOptions(square);
      setMoveFrom(hasMoveOptions ? square : "");
      return;
    }

    clearMoveSelection();
  }

  function handleFenSubmit() {
    try {
      applyGameFromFen(fenInput.trim());
    } catch {
      console.warn("[frontend] invalid FEN submitted", fenInput);
      setFenError("Invalid FEN string.");
    }
  }

  function handleReset() {
    const nextGame = new Chess();
    console.log("[frontend] board reset", nextGame.fen());
    clearMoveSelection();
    setLastMove(null);
    setGame(nextGame);
    setFenError("");
    awaitingEngineMoveRef.current = false;
    pendingFenRef.current = nextGame.fen();
    setAnalysis({
      evaluation: "+0.00",
      depth: 0,
      nodes: 0,
      nps: 0,
      pv: [],
      bestMove: "--",
      rawScore: null,
    });
  }

  function canDragPiece({ piece }) {
    return playerTurn && isPlayerPiece(piece);
  }

  return (
    <main className="app-shell">
      <header className="app-header">
        <div>
          <p className="eyebrow">Engine</p>
          <h1>Parallel Pawns Analysis Board</h1>
        </div>
        <div className="status-pill">
          <span>{connectionState}</span>
          <span className={`status-dot status-${engineStatus}`} />
        </div>
      </header>

      <section className="app-grid">
        <div className="board-column">
          <div className="player-label top-label">
            {boardOrientation === "white" ? "Engine / Black" : "Engine / White"}
          </div>

          <ChessBoard
            fen={currentFen}
            onPieceDrop={handlePieceDrop}
            onSquareClick={handleSquareClick}
            orientation={boardOrientation}
            canDragPiece={canDragPiece}
            squareStyles={boardSquareStyles}
          />

          <div className="player-label bottom-label">
            {boardOrientation === "white" ? "Player / White" : "Player / Black"}
          </div>
        </div>

        <AnalysisPanel
          analysis={analysis}
          engineStatus={engineStatus}
          sideToMove={sideToMove}
        />
      </section>

      <section className="fen-toolbar">
        <label className="fen-field" htmlFor="fen-input">
          <span>FEN String</span>
          <input
            id="fen-input"
            type="text"
            value={fenInput}
            onChange={(event) => setFenInput(event.target.value)}
            spellCheck="false"
          />
        </label>

        <div className="button-row">
          <button type="button" onClick={handleFenSubmit}>
            Set FEN
          </button>
          <button type="button" onClick={handleReset}>
            Reset
          </button>
          <button
            type="button"
            onClick={() =>
              setBoardOrientation((current) =>
                current === "white" ? "black" : "white"
              )
            }
          >
            Flip Board
          </button>
        </div>
      </section>

      {fenError ? <p className="error-text">{fenError}</p> : null}
    </main>
  );
}
