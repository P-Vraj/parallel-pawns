import { Chessboard } from "react-chessboard";

export default function ChessBoard({
  fen,
  onPieceDrop,
  onSquareClick,
  orientation,
  canDragPiece,
  squareStyles,
}) {
  const options = {
    id: "parallel-pawns-board",
    position: fen,
    onPieceDrop,
    onSquareClick,
    canDragPiece,
    squareStyles,
    boardOrientation: orientation,
    allowDragging: true,
    darkSquareStyle: { backgroundColor: "#55515b" },
    lightSquareStyle: { backgroundColor: "#f3f0ec" },
    boardStyle: {
      borderRadius: "12px",
      boxShadow: "0 16px 40px rgba(19, 15, 20, 0.16)",
    },
  };

  return (
    <div className="board-card">
      <Chessboard options={options} />
    </div>
  );
}
