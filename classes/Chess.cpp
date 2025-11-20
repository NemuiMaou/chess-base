#include "Chess.h"
#include <limits>
#include <cmath>

Chess::Chess()
{
    _grid = new Grid(8, 8);
}

Chess::~Chess()
{
    delete _grid;
}

char Chess::pieceNotation(int x, int y) const
{
    const char *wpieces = { "0PNBRQK" };
    const char *bpieces = { "0pnbrqk" };
    Bit *bit = _grid->getSquare(x, y)->bit();
    char notation = '0';
    if (bit) {
        notation = bit->gameTag() < 128 ? wpieces[bit->gameTag()] : bpieces[bit->gameTag()-128];
    }
    return notation;
}

Bit* Chess::PieceForPlayer(const int playerNumber, ChessPiece piece)
{
    const char* pieces[] = { "pawn.png", "knight.png", "bishop.png", "rook.png", "queen.png", "king.png" };

    Bit* bit = new Bit();
    // should possibly be cached from player class?
    const char* pieceName = pieces[piece - 1];
    std::string spritePath = std::string("") + (playerNumber == 0 ? "w_" : "b_") + pieceName;
    bit->LoadTextureFromFile(spritePath.c_str());
    bit->setOwner(getPlayerAt(playerNumber));
    bit->setSize(pieceSize, pieceSize);

    return bit;
}

void Chess::setUpBoard()
{
    setNumberOfPlayers(2);
    _gameOptions.rowX = 8;
    _gameOptions.rowY = 8;

    _grid->initializeChessSquares(pieceSize, "boardsquare.png");
    FENtoBoard("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR");

    
    generateKnightMoveBitBoard();
    generateKingMoveBitBoard();
    startGame();
}

void Chess::FENtoBoard(const std::string& fen) {
    // convert a FEN string to a board
    // FEN is a space delimited string with 6 fields
    int boardIndex = 0;
    for(int i = 0; i < fen.length(); ++i){

        if(fen[i] == '/') continue;

        else if(isdigit(fen[i])){
            boardIndex += fen[i] - '0';
            continue;
        }
        else{

            bool isWhite = fen[i] >= 'A' && fen[i] <= 'Z';
            int playerColor = isWhite ? 1 : 0;

            ChessPiece chessPiece = Pawn;
            char piece = std::tolower(fen[i]);
            switch(piece)
            {
                case 'p':
                    chessPiece = ChessPiece::Pawn;
                    break;
                case 'r':
                    chessPiece = ChessPiece::Rook;
                    break;
                case 'n':
                    chessPiece = ChessPiece::Knight;
                    break;
                case 'b':
                    chessPiece = ChessPiece::Bishop;
                    break;
                case 'q':
                    chessPiece = ChessPiece::Queen;
                    break;
                case 'k':
                    chessPiece = ChessPiece::King;
                    break;
            }
            Bit* bit = PieceForPlayer(playerColor, chessPiece);

            int colorTag = (playerColor == 1) ? 128 : 0;
            bit->setGameTag(colorTag + static_cast<int>(chessPiece));

            // int flippedIndex = (7 - (boardIndex / 8)) * 8 + (boardIndex % 8);
            ChessSquare* currentSquare = _grid->getSquareByIndex(boardIndex);
            currentSquare->setBit(bit);
            bit->setPosition(currentSquare->getPosition());
            bit->setParent(currentSquare);
        }
        boardIndex++;
    }
    // 1: piece placement (from white's perspective)
    // NOT PART OF THIS ASSIGNMENT BUT OTHER THINGS THAT CAN BE IN A FEN STRING
    // ARE BELOW
    // 2: active color (W or B)
    // 3: castling availability (KQkq or -)
    // 4: en passant target square (in algebraic notation, or -)
    // 5: halfmove clock (number of halfmoves since the last capture or pawn advance)
}

bool Chess::actionForEmptyHolder(BitHolder &holder)
{
    return false;
}

bool Chess::canBitMoveFrom(Bit &bit, BitHolder &src)
{
    // need to implement friendly/unfriendly in bit so for now this hack
    int currentPlayer = getCurrentPlayer()->playerNumber() * 128;
    int pieceColor = bit.gameTag() & 128;
    if (pieceColor == currentPlayer) return true;
    return false;
}

bool Chess::canBitMoveFromTo(Bit &bit, BitHolder &src, BitHolder &dst)
{
    ChessSquare* srcSquare = dynamic_cast<ChessSquare*>(&src);
    ChessSquare* dstSquare = dynamic_cast<ChessSquare*>(&dst);
    if (!srcSquare || !dstSquare) return false;

    const int fromX = srcSquare->getColumn();
    const int fromY = srcSquare->getRow();
    const int toX   = dstSquare->getColumn();
    const int toY   = dstSquare->getRow();
    if (fromX == toX && fromY == toY) return false;

    // to prevent self-capture
    Player* mover = bit.getOwner();
    Player* atDst = ownerAt(toX, toY);
    if (atDst && mover == atDst) return false;

    
    char p = pieceNotation(fromX, fromY);
    if (p == '0') return false;

    auto idx = [](int x, int y){ return y * 8 + x; };
    const int fromSq = idx(fromX, fromY);
    const int toSq   = idx(toX, toY);

    // knights
    if (p == 'N' || p == 'n') {
        uint64_t mask = _knightBitboards[fromSq].getData();
        return (mask & (1ULL << toSq)) != 0ULL;
    }

    // kings
    if (p == 'K' || p == 'k') {
        uint64_t mask = _kingBitboards[fromSq].getData();
        return (mask & (1ULL << toSq)) != 0ULL;
    }

    // pawns
    if (p == 'P' || p == 'p') {
        std::vector<BitMove> pmoves;
        const int playerColor = (p == 'P') ? 0 : 1;   
        auto s = stateString();                       
        generatePawnMoves(s.c_str(), pmoves, fromX, fromY, playerColor);

        for (const auto& m : pmoves) {
            if (m.to == toSq) return true;
        }
        return false;
    }

    // need to add the others (anything else can just move as of right now)
    return true;
}

void Chess::stopGame()
{
    _grid->forEachSquare([](ChessSquare* square, int x, int y) {
        square->destroyBit();
    });
}

Player* Chess::ownerAt(int x, int y) const
{
    if (x < 0 || x >= 8 || y < 0 || y >= 8) {
        return nullptr;
    }

    auto square = _grid->getSquare(x, y);
    if (!square || !square->bit()) {
        return nullptr;
    }
    return square->bit()->getOwner();
}

Player* Chess::checkForWinner()
{
    return nullptr;
}

bool Chess::checkForDraw()
{
    return false;
}

std::string Chess::initialStateString()
{
    return stateString();
}

std::string Chess::stateString()
{
    std::string s;
    s.reserve(64);
    _grid->forEachSquare([&](ChessSquare* square, int x, int y) {
            s += pieceNotation( x, y );
        }
    );
    return s;}

void Chess::setStateString(const std::string &s)
{
    _grid->forEachSquare([&](ChessSquare* square, int x, int y) {
        int index = y * 8 + x;
        char playerNumber = s[index] - '0';
        if (playerNumber) {
            square->setBit(PieceForPlayer(playerNumber - 1, Pawn));
        } else {
            square->setBit(nullptr);
        }
    });
}

void Chess::generateKnightMoveBitBoard(){

    std::pair<int,int> offsets[] = {
        {-2,-1},{-2, 1},{ 2,-1},{ 2, 1},
        {-1,-2},{-1, 2},{ 1,-2},{ 1, 2}
    };

    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            uint64_t mask = 0ULL; 
            for (auto [dx, dy] : offsets) {
                int nx = x + dx, ny = y + dy;
                // skip anything that goes out of bounds
                if (nx >= 0 && nx < 8 && ny >= 0 && ny < 8) {
                    int to = ny * 8 + nx;
                    mask |= (1ULL << to);
                }
            }
            _knightBitboards[y * 8 + x] = BitBoard(mask);
        }
    }
}

void Chess::generateKnightMoves(std::vector<BitMove>& moves, BitBoard knightBoard, uint64_t emptySquares){
    knightBoard.forEachBit([&](int fromSquare){
        BitBoard moveBitboard = BitBoard(_knightBitboards[fromSquare].getData() & emptySquares);
        moveBitboard.forEachBit([&](int toSquare) {
            moves.emplace_back(fromSquare, toSquare, Knight);
        });
    });
}

void Chess::generateKingMoveBitBoard(){
    std::pair<int,int> offsets[] = {
        {-1,1},{0,1},{1,1},
        {-1,0},{1,0},
        {-1,-1},{0,-1},{1,-1}
    };

    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            uint64_t mask = 0ULL; 
            for (auto [dx, dy] : offsets) {
                int nx = x + dx, ny = y + dy;
                // skip anything that goes out of bounds
                if (nx >= 0 && nx < 8 && ny >= 0 && ny < 8) {
                    int to = ny * 8 + nx;
                    mask |= (1ULL << to);
                }
            }
            _kingBitboards[y * 8 + x] = BitBoard(mask); 
        }
    }
}

void Chess::generateKingMoves(std::vector<BitMove>& moves, BitBoard kingBoard, uint64_t emptySquares){
    kingBoard.forEachBit([&](int fromSquare){
        BitBoard moveBitboard = BitBoard(_kingBitboards[fromSquare].getData() & emptySquares);
        moveBitboard.forEachBit([&](int toSquare) {
            moves.emplace_back(fromSquare, toSquare, King);
        });
    });
}

void Chess::generatePawnMoveBitBoardList(std::vector<BitMove>& moves, const BitBoard pawnBoard, const BitBoard emptySquares, const BitBoard enemySquares, char playerColor)
{
    // file and rankk masks for pawns
    const uint64_t NotA   = 0xfefefefefefefefeULL;
    const uint64_t NotH   = 0x7f7f7f7f7f7f7f7fULL;
    const uint64_t RANK_2 = 0x000000000000FF00ULL; // white
    const uint64_t RANK_7 = 0x00FF000000000000ULL; // black

    const uint64_t pawnSquares = pawnBoard.getData();
    const uint64_t empty = emptySquares.getData();
    const uint64_t enemy = enemySquares.getData();

    const bool white = (playerColor == 'w' || playerColor == 'W' || playerColor == 0);

    // helper to convert bit masks int BitMoves
    auto emitFromMask = [&](uint64_t mask, int deltaToMinusFrom) {
        if (!mask) return;
        BitBoard bitBoard(mask);
        bitBoard.forEachBit([&](int toSq){
            int fromSq = toSq - deltaToMinusFrom;
            moves.emplace_back(fromSq, toSq, Pawn);
        });
    };

    if (white) {
        // forwards movement
        uint64_t one = (pawnSquares << 8) & empty;
        uint64_t two = (((pawnSquares & RANK_2) << 8) & empty);
        two = (two << 8) & empty;

        // diag captures
        uint64_t capL = (pawnSquares << 7) & enemy & NotH;
        uint64_t capR = (pawnSquares << 9) & enemy & NotA;

        emitFromMask(one, 8);
        emitFromMask(two, 16);
        emitFromMask(capL, 7);
        emitFromMask(capR, 9);
    } else { // black moves (reversed)
        uint64_t one = (pawnSquares >> 8) & empty;
        uint64_t two = (((pawnSquares & RANK_7) >> 8) & empty);
        two = (two >> 8) & empty;
        uint64_t capL = (pawnSquares >> 9) & enemy & NotH;
        uint64_t capR = (pawnSquares >> 7) & enemy & NotA;

        emitFromMask(one, -8);
        emitFromMask(two, -16);
        emitFromMask(capL, -9);
        emitFromMask(capR, -7);
    }
}

void Chess::generatePawnMoves(const char *state, std::vector<BitMove>& moves, int file, int rank, int playerColor)
{
    if (!state) return;
    if (file < 0 || file >= 8 || rank < 0 || rank >= 8) return;

    auto index    = [](int x, int y){ return y * 8 + x; };
    auto inBounds = [](int x, int y){ return x >= 0 && x < 8 && y >= 0 && y < 8; };

    // helpers for us to check for squares and the player color
    auto at = [&](int x, int y)->char {
        if (!inBounds(x, y)) return '#';
        return state[index(x, y)];
    };

    auto isEmpty      = [&](int x, int y)->bool { return at(x, y) == '0'; };
    auto isWhitePiece = [&](char c)->bool { return (c >= 'A' && c <= 'Z'); };
    auto isBlackPiece = [&](char c)->bool { return (c >= 'a' && c <= 'z'); };

    const bool white = (playerColor == 0 || playerColor == 'w' || playerColor == 'W');

    auto detectWhiteStartRow = [&]() -> int { //detecting our starting row for white pawns
        for (int x = 0; x < 8; ++x) {
            if (at(x, 6) == 'P') return 6;
            if (at(x, 1) == 'P') return 1;
        }
        return 6;
    };
    auto detectBlackStartRow = [&]() -> int { //detecting our starting row for black pawns
        for (int x = 0; x < 8; ++x) {
            if (at(x, 1) == 'p') return 1;
            if (at(x, 6) == 'p') return 6;
        }
        return 1;
    };

    const int whiteStartRow = detectWhiteStartRow();
    const int blackStartRow = detectBlackStartRow();

    const int whiteDir = (whiteStartRow == 6) ? -1 : +1;
    const int blackDir = (blackStartRow == 1) ? +1 : -1;
    const int dir      = white ? whiteDir : blackDir;
    const int startRow = white ? whiteStartRow : blackStartRow;
    const int fromSq = index(file, rank);

    // pawn check (has to be right player color)
    char src = at(file, rank);
    if (white) { if (src != 'P') return; }
    else       { if (src != 'p') return; }

    // to see if the next space forward is empty
    int forward1Rank = rank + dir;
    if (inBounds(file, forward1Rank) && isEmpty(file, forward1Rank)) {
        moves.emplace_back(fromSq, index(file, forward1Rank), Pawn);

        // at the start, if moving 2 ranks, checks to see for both spaces to be empty
        if (rank == startRow) {
            int forward2Ranks = rank + 2 * dir;
            if (inBounds(file, forward2Ranks) && isEmpty(file, forward2Ranks)) {
                moves.emplace_back(fromSq, index(file, forward2Ranks), Pawn);
            }
        }
    }

    // we can only capture diagonally 
    int leftFile  = file - 1;
    int rightFile = file + 1;

    if (inBounds(leftFile, forward1Rank)) {
        char dst = at(leftFile, forward1Rank);
        if (dst != '0' && (white ? isBlackPiece(dst) : isWhitePiece(dst))) {
            moves.emplace_back(fromSq, index(leftFile, forward1Rank), Pawn);
        }
    }
    if (inBounds(rightFile, forward1Rank)) {
        char dst = at(rightFile, forward1Rank);
        if (dst != '0' && (white ? isBlackPiece(dst) : isWhitePiece(dst))) {
            moves.emplace_back(fromSq, index(rightFile, forward1Rank), Pawn);
        }
    }
}