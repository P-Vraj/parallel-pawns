#include "search.h"

#include "evaluation.h"

namespace {

constexpr Eval kQSearchDeltaMargin = 200;

Eval qsearch_gain(const Position& pos, Move move) noexcept {
    Eval gain = 0;

    if (move.isCapture()) {
        PieceType victim = PieceType::Pawn;
        if (move.moveType() != MoveType::EnPassant)
            victim = piece_type(pos.pieceOn(move.to()));

        gain = static_cast<Eval>(gain + kPieceValues[to_underlying(victim)]);
    }

    if (move.isPromotion()) {
        gain = static_cast<Eval>(
            gain + kPieceValues[to_underlying(move.promotionType())] - kPieceValues[to_underlying(PieceType::Pawn)]
        );
    }

    return gain;
}

}  // namespace

namespace engine {

SearchResult Search::search(Position& pos, const SearchLimits& limits) {
    nodes_ = 0;
    qNodes_ = 0;
    resetHeuristics_();

    if (tt_ != nullptr)
        tt_->newSearch();

    SearchResult result{};

    MoveList moves(pos);

    if (moves.empty()) {
        Eval terminalScore = 0;
        if (isTerminal_(pos, moves, 0, terminalScore)) {
            result.score = terminalScore;
            result.bestMove = Move{};
            result.nodes = nodes_;
            result.qNodes = qNodes_;
            return result;
        }
    }

    Move bestMove{};
    Eval bestScore = -kEvalInf;

    for (int currentDepth = 1; currentDepth <= limits.depth; ++currentDepth) {
        Move ttMove = bestMove;
        if (tt_ != nullptr) {
            if (const TTEntry* entry = tt_->probe(pos.hash())) {
                if (!entry->bestMove.isNone())
                    ttMove = entry->bestMove;
            }
        }

        orderMoves_(pos, moves, ttMove, 0);

        Eval alpha = -kEvalInf;
        const Eval beta = kEvalInf;

        Eval iterationBestScore = -kEvalInf;
        Move iterationBestMove{};

        for (const Move m : moves) {
            UndoInfo u{};
            pos.makeMove(m, u);

            const Eval score = -alphaBeta_(pos, currentDepth - 1, -beta, -alpha, 1);

            pos.undoMove(m, u);

            if (score > iterationBestScore) {
                iterationBestScore = score;
                iterationBestMove = m;
            }

            if (score > alpha)
                alpha = score;
        }

        bestMove = iterationBestMove;
        bestScore = iterationBestScore;
    }

    result.bestMove = bestMove;
    result.score = bestScore;
    result.nodes = nodes_;
    result.qNodes = qNodes_;
    return result;
}

Eval Search::alphaBeta_(Position& pos, int depth, Eval alpha, Eval beta, int ply) {
    ++nodes_;

    if (ply >= kMaxPly)
        return evaluate_(pos);

    MoveList moves(pos);

    Eval terminalScore = 0;
    if (isTerminal_(pos, moves, ply, terminalScore))
        return terminalScore;

    if (depth <= 0)
        return quiescence_(pos, alpha, beta, ply);

    const Eval originalAlpha = alpha;
    const Key key = pos.hash();

    Move ttMove{};

    if (tt_ != nullptr) {
        if (const TTEntry* entry = tt_->probe(key)) {
            ttMove = entry->bestMove;
            const Eval ttScore = decodeMateScore(entry->score, ply);

            if (static_cast<int>(entry->depth) >= depth) {
                switch (entry->bound) {
                    case Bound::Exact:
                        return ttScore;
                    case Bound::Lower:
                        if (ttScore >= beta)
                            return ttScore;
                        break;
                    case Bound::Upper:
                        if (ttScore <= alpha)
                            return ttScore;
                        break;
                    case Bound::None:
                    default:
                        break;
                }
            }
        }
    }

    orderMoves_(pos, moves, ttMove, ply);

    Eval bestScore = -kEvalInf;
    Move bestMove{};

    for (const Move m : moves) {
        UndoInfo u{};
        pos.makeMove(m, u);

        const Eval score = -alphaBeta_(pos, depth - 1, -beta, -alpha, ply + 1);

        pos.undoMove(m, u);

        if (score > bestScore) {
            bestScore = score;
            bestMove = m;
        }

        if (score > alpha)
            alpha = score;

        if (alpha >= beta) {
            if (!m.isCapture() && !m.isPromotion())
                updateQuietHeuristics_(pos, m, ply, depth);
            break;
        }
    }

    if (tt_ != nullptr) {
        Bound bound = Bound::Exact;
        if (bestScore <= originalAlpha)
            bound = Bound::Upper;
        else if (bestScore >= beta)
            bound = Bound::Lower;

        tt_->store(key, bestMove, bestScore, depth, bound, ply);
    }

    return bestScore;
}

Eval Search::quiescence_(Position& pos, Eval alpha, Eval beta, int ply) {
    ++qNodes_;

    if (ply >= kMaxPly)
        return evaluate_(pos);

    const bool inCheck = pos.inCheck();
    MoveList moves{};
    generate_moves(pos, moves, inCheck ? GenMode::Evasions : GenMode::Tactical);

    Eval terminalScore = 0;
    if (isTerminal_(pos, moves, ply, terminalScore))
        return terminalScore;

    Eval standPat = kEvalNegInf;
    if (!inCheck) {
        standPat = evaluate_(pos);
        if (standPat >= beta)
            return standPat;

        if (standPat > alpha)
            alpha = standPat;
    }

    orderQMoves_(pos, moves, inCheck);

    for (const Move m : moves) {
        if (!inCheck) {
            const Eval optimisticScore = static_cast<Eval>(standPat + qsearch_gain(pos, m) + kQSearchDeltaMargin);
            if (optimisticScore < alpha)
                continue;

            if (m.isCapture() && !m.isPromotion() && isBadQCapture_(pos, m))
                continue;
        }

        UndoInfo u{};
        pos.makeMove(m, u);

        const Eval score = -quiescence_(pos, -beta, -alpha, ply + 1);

        pos.undoMove(m, u);

        if (score >= beta)
            return score;

        if (score > alpha)
            alpha = score;
    }

    return alpha;
}

Eval Search::evaluate_(const Position& pos) const noexcept {
    Eval score = evaluation(pos);
    return (pos.sideToMove() == Color::White) ? score : static_cast<Eval>(-score);
}

bool Search::isTerminal_(const Position& pos, const MoveList& moves, int ply, Eval& terminalScore) const noexcept {
    if (!moves.empty())
        return false;

    if (pos.inCheck())
        terminalScore = matedScore(ply);
    else
        terminalScore = kDrawScore;

    return true;
}

void Search::resetHeuristics_() noexcept {
    for (auto& plyKillers : killers_) plyKillers = { Move::none(), Move::none() };

    for (auto& colorHistory : history_) {
        for (auto& fromHistory : colorHistory) fromHistory.fill(0);
    }
}

void Search::orderMoves_(const Position& pos, MoveList& moves, Move ttMove, int ply) const noexcept {
    std::sort(moves.begin(), moves.end(), [&](Move lhs, Move rhs) {
        return scoreMove_(pos, lhs, ttMove, ply) > scoreMove_(pos, rhs, ttMove, ply);
    });
}

int Search::scoreMove_(const Position& pos, Move move, Move ttMove, int ply) const noexcept {
    if (move == ttMove)
        return 2'000'000;

    if (move.isCapture() || move.isPromotion())
        return 1'000'000 + scoreQMove_(pos, move);

    if (ply < kMaxPly) {
        if (move == killers_[ply][0])
            return 900'000;
        if (move == killers_[ply][1])
            return 800'000;
    }

    const auto colorIdx = to_underlying(pos.sideToMove());
    return history_[colorIdx][to_underlying(move.from())][to_underlying(move.to())];
}

void Search::updateQuietHeuristics_(const Position& pos, Move move, int ply, int depth) noexcept {
    if (ply < kMaxPly && move != killers_[ply][0]) {
        killers_[ply][1] = killers_[ply][0];
        killers_[ply][0] = move;
    }

    const auto colorIdx = to_underlying(pos.sideToMove());
    int& historyScore = history_[colorIdx][to_underlying(move.from())][to_underlying(move.to())];
    historyScore += depth * depth;
}

int Search::scoreQMove_(const Position& pos, Move move) const noexcept {
    if (!move.isCapture() && !move.isPromotion())
        return -kMateScore;

    const PieceType attacker = piece_type(pos.pieceOn(move.from()));
    const int attackerValue = kPieceValues[to_underlying(attacker)];

    int score = 0;

    if (move.isCapture()) {
        PieceType victim = PieceType::Pawn;
        if (move.moveType() != MoveType::EnPassant)
            victim = piece_type(pos.pieceOn(move.to()));

        score += 16 * kPieceValues[to_underlying(victim)] - attackerValue;
    }

    if (move.isPromotion())
        score += 8 * kPieceValues[to_underlying(move.promotionType())];

    return score;
}

bool Search::isBadQCapture_(const Position& pos, Move move) const noexcept {
    if (!move.isCapture() || move.moveType() == MoveType::EnPassant)
        return false;

    const PieceType attacker = piece_type(pos.pieceOn(move.from()));
    const PieceType victim = piece_type(pos.pieceOn(move.to()));
    if (is_empty(attacker) || is_empty(victim))
        return false;

    if (kPieceValues[to_underlying(attacker)] <= kPieceValues[to_underlying(victim)])
        return false;

    return is_square_attacked(pos, move.to(), ~pos.sideToMove());
}

int Search::scoreQEvasion_(const Position& pos, Move move) const noexcept {
    if (move.isCapture() || move.isPromotion())
        return 100000 + scoreQMove_(pos, move);

    const PieceType mover = piece_type(pos.pieceOn(move.from()));
    if (mover == PieceType::King)
        return 1000;

    return 10000 - kPieceValues[to_underlying(mover)];
}

void Search::orderQMoves_(const Position& pos, MoveList& moves, bool inCheck) const noexcept {
    std::sort(moves.begin(), moves.end(), [&](Move lhs, Move rhs) {
        const int lhsScore = inCheck ? scoreQEvasion_(pos, lhs) : scoreQMove_(pos, lhs);
        const int rhsScore = inCheck ? scoreQEvasion_(pos, rhs) : scoreQMove_(pos, rhs);
        return lhsScore > rhsScore;
    });
}

}  // namespace engine
