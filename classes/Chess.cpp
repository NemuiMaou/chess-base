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
    generateRookMoveBitBoard();
    generateBishopMoveBitBoard();
    generateQueenMoveBitBoard();

    if (gameHasAI()) {
        // If your engine uses AI_PLAYER like TicTacToe, you can use that instead of 1
        setAIPlayer(1);   // second player = AI (usually black)
    }

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

    // prevent self-capture
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

    // helper to help my sliding pieces and creates a mask for them that prevents them from moving pass any piece
    auto isObstructed = [&](const std::vector<std::pair<int,int>>& dirs) -> uint64_t {
        uint64_t mask = 0ULL;
        for (auto [dx, dy] : dirs) {
            int x = fromX + dx;
            int y = fromY + dy;
            while (x >= 0 && x < 8 && y >= 0 && y < 8) {
                int sq = idx(x, y);
                mask |= (1ULL << sq);
                if (ownerAt(x, y) != nullptr) { // if there is a piece, stop the search
                    break;
                }
                x += dx;
                y += dy;
            }
        }
        return mask;
    };

    // rooks
    if (p == 'R' || p == 'r') {
        std::vector<std::pair<int,int>> dirs = {
            { 1,  0},
            {-1,  0},
            { 0,  1},
            { 0, -1}
        };

        uint64_t mask = isObstructed(dirs);
        _rookBitboards[fromSq] = BitBoard(mask);
        return (mask & (1ULL << toSq)) != 0ULL;
    }

    // bishops
    if (p == 'B' || p == 'b') {
        std::vector<std::pair<int,int>> dirs = {
            { 1,  1},
            { 1, -1},
            {-1,  1},
            {-1, -1}
        };

        uint64_t mask = isObstructed(dirs);
        _bishopBitboards[fromSq] = BitBoard(mask);
        return (mask & (1ULL << toSq)) != 0ULL;
    }

    // queens
    if (p == 'Q' || p == 'q') {
        std::vector<std::pair<int,int>> dirs = {
            { 1,  0}, {-1,  0}, { 0,  1}, { 0, -1},
            { 1,  1}, { 1, -1}, {-1,  1}, {-1, -1}
        };

        uint64_t mask = isObstructed(dirs);
        _queenBitboards[fromSq] = BitBoard(mask);
        return (mask & (1ULL << toSq)) != 0ULL;
    }
    return false;
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

void Chess::generateRookMoveBitBoard(){
    for (int from = 0; from < 64; ++from) {
        int fromX = from % 8;
        int fromY = from / 8;
        uint64_t mask = 0ULL;

        auto direction = [&](int dx, int dy)
        {
            int x = fromX + dx;
            int y = fromY + dy;
            while (x >= 0 && x < 8 && y >= 0 && y < 8) {
                int to = y * 8 + x;
                mask |= (1ULL << to);
                x += dx;
                y += dy;
            }
        };
        direction( 1,  0);
        direction(-1,  0);
        direction( 0,  1);
        direction( 0, -1);
        _rookBitboards[from] = BitBoard(mask);
    }
}

void Chess::generateRookMoves(std::vector<BitMove>& moves, BitBoard rookBoard, uint64_t emptySquares){
    rookBoard.forEachBit([&](int fromSquare) {
        BitBoard moveBitboard(_rookBitboards[fromSquare].getData() & emptySquares);
        moveBitboard.forEachBit([&](int toSquare) {
            moves.emplace_back(fromSquare, toSquare, Rook);
        });
    });
}

void Chess::generateBishopMoveBitBoard(){
    for (int from = 0; from < 64; ++from) {
        int fromX = from % 8;
        int fromY = from / 8;
        uint64_t mask = 0ULL;

        auto direction = [&](int dx, int dy)
        {
            int x = fromX + dx;
            int y = fromY + dy;
            while (x >= 0 && x < 8 && y >= 0 && y < 8) {
                int to = y * 8 + x;
                mask |= (1ULL << to);
                x += dx;
                y += dy;
            }
        };
        direction( 1,  1);
        direction( 1, -1);
        direction(-1,  1);
        direction(-1, -1);
        _bishopBitboards[from] = BitBoard(mask);
    }
}

void Chess::generateBishopMoves(std::vector<BitMove>& moves, BitBoard bishopBoard, uint64_t emptySquares){
    bishopBoard.forEachBit([&](int fromSquare) {
        BitBoard moveBitboard(_bishopBitboards[fromSquare].getData() & emptySquares);
        moveBitboard.forEachBit([&](int toSquare) {
            moves.emplace_back(fromSquare, toSquare, Bishop);
        });
    });
}

void Chess::generateQueenMoveBitBoard(){
    for (int from = 0; from < 64; ++from) {
        int fromX = from % 8;
        int fromY = from / 8;
        uint64_t mask = 0ULL;

        auto direction = [&](int dx, int dy)
        {
            int x = fromX + dx;
            int y = fromY + dy;
            while (x >= 0 && x < 8 && y >= 0 && y < 8) {
                int to = y * 8 + x;
                mask |= (1ULL << to);
                x += dx;
                y += dy;
            }
        };
        direction( 1,  0);
        direction( 1,  1);
        direction(-1,  0);
        direction( 1, -1);
        direction( 0,  1);
        direction(-1,  1);
        direction( 0, -1);
        direction(-1, -1);
        _queenBitboards[from] = BitBoard(mask);
    }
}

void Chess::generateQueenMoves(std::vector<BitMove>& moves, BitBoard queenBoard, uint64_t emptySquares){
    queenBoard.forEachBit([&](int fromSquare) {
        BitBoard moveBitboard(_queenBitboards[fromSquare].getData() & emptySquares);
        moveBitboard.forEachBit([&](int toSquare) {
            moves.emplace_back(fromSquare, toSquare, Queen);
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

void Chess::updateAI(){
    int playerNum = getCurrentPlayer()->playerNumber();
    int colorToMove = (playerNum == 0) ? WHITE : BLACK;

    std::string state = stateString();

    auto _moves = generateAllMoves(state, colorToMove);
    // if (rootMoves.empty()) return;

    // int bestVal = negInfinite;
    // BitMove bestMove = rootMoves.front();
    // int searchDepth = 4;

    // for (const auto& move : rootMoves) {
    //     char captured    = state[move.to];
    //     char pieceMoving = state[move.from];

    //     state[move.to]   = pieceMoving; // temp move
    //     state[move.from] = '0';

    //     int val = -negamax(state, searchDepth - 1, negInfinite, posInfinite, -colorToMove);

    //     state[move.from] = pieceMoving; // the undo
    //     state[move.to]   = captured;

    //     if (val > bestVal) {
    //         bestVal = val;
    //         bestMove = move;
    //     }
    // }
    // if (bestVal == negInfinite) return; // if no move was found

    // int srcSquare = bestMove.from;
    // int dstSquare = bestMove.to;

    // BitHolder& src = getHolderAt(srcSquare & 7, srcSquare / 8);
    // BitHolder& dst = getHolderAt(dstSquare & 7, dstSquare / 8);
    // Bit* bit = src.bit();
    // if (!bit) return;

    // dst.dropBitAtPoint(bit, ImVec2(0, 0));
    // src.setBit(nullptr);
    // bitMovedFromTo(*bit, src, dst);
    int bestVal = negInfinite;
    BitMove bestMove;
    // std::string state = stateString();
    // _countMoves = 0;

    // Search through current legal moves
    for(auto move : _moves) {
        char boardSave = state[move.to];
        char pieceMoving = state[move.from];

        // Make the move on our state copy
        state[move.to] = pieceMoving;
        state[move.from] = '0';

        // Call negamax to evaluate this move
        int moveVal = -negamax(state, 3, negInfinite, posInfinite, WHITE);

        // Undo the move
        state[move.from] = pieceMoving;
        state[move.to] = boardSave;

        // Track the best move found
        if (moveVal > bestVal) {
            bestMove = move;
            bestVal = moveVal;
        }
    }

    // Execute the best move on the actual board
    // I’m kind of amazed this code works and will be improving it
    if(bestVal != negInfinite) {
        // std::cout << "Moves checked: " << _countMoves << std::endl;
        int srcSquare = bestMove.from;
        int dstSquare = bestMove.to;
        BitHolder& src = getHolderAt(srcSquare&7, srcSquare/8);
        BitHolder& dst = getHolderAt(dstSquare&7, dstSquare/8);
        Bit* bit = src.bit();
        dst.dropBitAtPoint(bit, ImVec2(0, 0));
        src.setBit(nullptr);
        bitMovedFromTo(*bit, src, dst);
    }
}

int Chess::negamax(std::string& state, int depth, int alpha, int beta, int playerColor)
{
    if (depth == 0) {
        return evaluateBoard(state) * playerColor;
    }
    auto newMoves = generateAllMoves(state, playerColor);

    if (newMoves.empty()) {
        return 0;
    }

    int bestVal = negInfinite;
    for (auto& move : newMoves) {
        char captured   = state[move.to];
        char pieceMoving = state[move.from];

        state[move.to]   = pieceMoving;
        state[move.from] = '0';

        bestVal = -negamax(state, depth - 1, -beta, -alpha, -playerColor);

        state[move.from] = pieceMoving;
        state[move.to]   = captured;

        alpha = std::max(alpha, bestVal);
        if (alpha >= beta) break;
    }
    return bestVal;
}

int Chess::evaluateBoard(const std::string& state) {
    static const std::map<char, int> evaluateScores = {
        {'P', 100}, {'p', -100},
        {'N', 200}, {'n', -200},
        {'B', 230}, {'b', -230},
        {'R', 400}, {'r', -400},
        {'Q', 900}, {'q', -900},
        {'K', 2000}, {'k', -2000},
        {'0', 0}
    };

    int value = 0;
    for (char ch : state) {
        auto it = evaluateScores.find(ch);
        if (it != evaluateScores.end()) {
            value += it->second;
        }
    }
    return value;
}

std::vector<BitMove> Chess::generateAllMoves(const std::string& state, int playerColor)
{
    std::vector<BitMove> moves;
    moves.reserve(64);

    auto index    = [](int x, int y){ return y * 8 + x; };
    auto inBounds = [](int x, int y){ return x >= 0 && x < 8 && y >= 0 && y < 8; };

    auto at = [&](int x, int y)->char {
        if (!inBounds(x, y)) return '#';
        return state[index(x, y)];
    };

    auto isWhitePiece = [](char c)->bool { return (c >= 'A' && c <= 'Z'); };
    auto isBlackPiece = [](char c)->bool { return (c >= 'a' && c <= 'z'); };

    const bool whiteToMove = (playerColor == WHITE);

    for (int sq = 0; sq < 64; ++sq) {
        int file = sq % 8;
        int rank = sq / 8;
        char piece = state[static_cast<size_t>(sq)];
        if (piece == '0') continue;

        if (whiteToMove && !isWhitePiece(piece)) continue;
        if (!whiteToMove && !isBlackPiece(piece)) continue;

        // pawns
        if (piece == 'P' || piece == 'p') {
            generatePawnMoves(state.c_str(), moves, file, rank, whiteToMove ? 0 : 1);
            continue;
        }

        auto tryAdd = [&](int toX, int toY, ChessPiece pieceType) {
            if (!inBounds(toX, toY)) return;
            char dst = at(toX, toY);
            if (dst == '#') return;

            // can't capture own pieces
            if (dst != '0') {
                if (whiteToMove && isWhitePiece(dst)) return;
                if (!whiteToMove && isBlackPiece(dst)) return;
            }

            int fromSq = index(file, rank);
            int toSq   = index(toX, toY);
            moves.emplace_back(fromSq, toSq, pieceType);
        };

        // knights
        if (piece == 'N' || piece == 'n') {
            static const std::pair<int,int> kOffsets[] = {
                {-2,-1},{-2, 1},{ 2,-1},{ 2, 1},
                {-1,-2},{-1, 2},{ 1,-2},{ 1, 2}
            };
            for (auto [dx, dy] : kOffsets) {
                tryAdd(file + dx, rank + dy, Knight);
            }
            continue;
        }

        // king
        if (piece == 'K' || piece == 'k') {
            static const std::pair<int,int> kOffsets[] = {
                {-1, 1},{ 0, 1},{ 1, 1},
                {-1, 0},        { 1, 0},
                {-1,-1},{ 0,-1},{ 1,-1}
            };
            for (auto [dx, dy] : kOffsets) {
                tryAdd(file + dx, rank + dy, King);
            }
            continue;
        }

        // same helper as before for the pieces that slide 
        auto isObstructed = [&](const std::vector<std::pair<int,int>>& dirs, ChessPiece pieceType){
            for (auto [dx, dy] : dirs) {
                int x = file + dx;
                int y = rank + dy;
                while (inBounds(x, y)) {
                    char dst = at(x, y);
                    if (dst == '0') {
                        int fromSq = index(file, rank);
                        int toSq   = index(x, y);
                        moves.emplace_back(fromSq, toSq, pieceType);
                    } else {
                        if (whiteToMove ? isBlackPiece(dst) : isWhitePiece(dst)) {
                            int fromSq = index(file, rank);
                            int toSq   = index(x, y);
                            moves.emplace_back(fromSq, toSq, pieceType);
                        }
                        break;
                    }
                    x += dx;
                    y += dy;
                }
            }
        };

        // rooks
        if (piece == 'R' || piece == 'r') {
            std::vector<std::pair<int,int>> dirs = {
                { 1,  0}, {-1,  0}, { 0,  1}, { 0, -1}
            };
            isObstructed(dirs, Rook);
            continue;
        }

        // bishops
        if (piece == 'B' || piece == 'b') {
            std::vector<std::pair<int,int>> dirs = {
                { 1,  1}, { 1, -1}, {-1,  1}, {-1, -1}
            };
            isObstructed(dirs, Bishop);
            continue;
        }

        // queens
        if (piece == 'Q' || piece == 'q') {
            std::vector<std::pair<int,int>> dirs = {
                { 1,  0}, {-1,  0}, { 0,  1}, { 0, -1},
                { 1,  1}, { 1, -1}, {-1,  1}, {-1, -1}
            };
            isObstructed(dirs, Queen);
            continue;
        }
    }

    return moves;
}