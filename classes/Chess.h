#pragma once

#include "Game.h"
#include "Grid.h"
#include "BitBoard.h"

constexpr int pieceSize = 80;

enum AllBitBoards
{
    WHITE_PAWNS,
    WHITE_KNIGHTS,
    WHITE_BISHOPS,
    WHITE_ROOKS,
    WHITE_QUEENS,
    WHITE_KING,
    BLACK_PAWNS,
    BLACK_KNIGHTS,
    BLACK_BISHOPS,
    BLACK_ROOKS,
    BLACK_QUEENS,
    BLACK_KING,
    WHITE_ALL_PIECES,
    BLACK_ALL_PIECES,
    OCCUPANCY,
    EMPTY_SQUARES,
    e_numBitBoards
};

class Chess : public Game
{
public:
    Chess();
    ~Chess();

    void setUpBoard() override;

    bool canBitMoveFrom(Bit &bit, BitHolder &src) override;
    bool canBitMoveFromTo(Bit &bit, BitHolder &src, BitHolder &dst) override;
    bool actionForEmptyHolder(BitHolder &holder) override;

    void stopGame() override;

    Player *checkForWinner() override;
    bool checkForDraw() override;

    std::string initialStateString() override;
    std::string stateString() override;
    void setStateString(const std::string &s) override;

    Grid* getGrid() override { return _grid; }

    void generateKingMoveBitBoard();
    void generateKingMoves(std::vector<BitMove>& moves, BitBoard knightBoard, uint64_t emptySquares);

    void generateKnightMoveBitBoard();
    void generateKnightMoves(std::vector<BitMove>& moves, BitBoard knightBoard, uint64_t emptySquares);

    void generatePawnMoveBitBoardList(std::vector<BitMove>& moves, const BitBoard pawnBoard, const BitBoard emptySquares, const BitBoard enemySquares, char playerColor);
    void generatePawnMoves(const char *state, std::vector<BitMove>& moevs, int file, int rank, int playerColor);

private:
    Bit* PieceForPlayer(const int playerNumber, ChessPiece piece);
    Player* ownerAt(int x, int y) const;
    void FENtoBoard(const std::string& fen);
    char pieceNotation(int x, int y) const;

    Grid* _grid;

    BitBoard _knightBitboards[64];
    BitBoard _kingBitboards[64];
};