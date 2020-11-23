/*
Sapeli. Linux UCI Chess960 engine
Copyright (C) 2019-2020 Toni Helminen

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

// Headers

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

// Constants

#define NAME        "Sapeli 2.0"
#define MAX_MOVES   218 // Legal moves
#define MAX_TOKENS  800 // Ply
#define DEPTH_LIMIT 30
#define INF         1048576
#define STARTPOS    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0"
#define KOTHMIDDLE  0x0000001818000000ULL
#define HASH_KEY    ((1 << 22) - 1)

// Enums

enum MOVE_T {KILLER, GOOD, QUIET};

// Structures

struct BOARD_T {
  uint64_t
    white[6],  // White bitboards
    black[6];  // Black bitboards
  int8_t
    board[64], // Pieces black and white
    epsq;      // En passant square
  int32_t
    score;     // Sorting score
  uint8_t
    index,     // Sorting index
    from,      // From square
    to,        // To square
    type,      // Move type (0: Normal, 1: OOw, 2: OOOw, 3: OOb, 4: OOOb, 5: =n, 6: =b, 7: =r, 8: =q)
    castle,    // Castling rights (0x1: K, 0x2: Q, 0x4: k, 0x8: q)
    rule50;    // Rule 50 counter
};

struct HASH_T {uint64_t eval_hash, sort_hash; int32_t score; uint8_t killer, good, quiet;};

// Consts

static const int
  EVAL_PIECE_VALUE_MG[5] = {1000, 3200, 3330, 5400, 11150}, EVAL_PIECE_VALUE_EG[5] = {1230, 3750, 3890, 5900, 11900},
  EVAL_ATTACKS[6][6] = {{2,6,6,7,11,12}, {1,5,5,6,12,15}, {1,5,5,8,16,22}, {1,4,4,5,10,19}, {1,3,3,4,7,17}, {1,2,2,3,4,15}},
  EVAL_CENTER[64] = {-2,-1,1,2,2,1,-1,-2,  -1,0,2,3,3,2,0,-1,  1,2,4,5,5,4,2,1,  2,3,5,6,6,5,3,2,  2,3,5,6,6,5,3,2,  1,2,4,5,5,4,2,1,  -1,0,2,3,3,2,0,-1,  -2,-1,1,2,2,1,-1,-2},
  EVAL_PSQT_MG[6][64] =
    {{0,0,0,0,0,0,0,0,  22,35,21,0,0,21,35,22,  28,21,-20,0,0,-20,21,28,  -6,-27,-15,155,155,-15,-27,-6,  -26,-36,-21,-22,-22,-21,-36,-26,  3,5,6,8,8,6,5,3,  15,17,16,119,119,16,17,15,  0,0,0,0,0,0,0,0},
     {-236,-103,-97,-69,-69,-97,-103,-236,  -6,0,6,9,9,6,0,-6,  -1,7,12,15,15,12,7,-1,  6,9,35,18,18,35,9,6,  6,10,50,73,73,50,10,6,  1,6,12,30,30,12,6,1,  -5,0,6,9,9,6,0,-5,  -141,-28,-22,6,6,-22,-28,-141},
     {-161,-38,-34,-16,-16,-34,-38,-161,  -5,30,6,9,9,6,30,-5,  2,16,12,15,15,12,16,2,  6,9,16,18,18,16,9,6,  6,9,15,19,19,15,9,6,  3,6,12,16,16,12,6,3,  -4,0,6,9,9,6,0,-4,  -141,-11,-7,6,6,-7,-11,-141},
     {-96,-3,48,71,71,48,-3,-96,  -5,3,11,44,44,11,3,-5,  3,6,12,40,40,12,6,3,  6,9,15,18,18,15,9,6,  6,9,15,18,18,15,9,6,  3,6,12,15,15,12,6,3,  52,55,61,64,64,61,55,52,  -6,13,23,26,26,23,13,-6},
     {-96,-3,2,7,7,2,-3,-96,  -3,3,6,8,8,6,3,-3,  3,6,12,15,15,12,6,3,  6,9,15,18,18,15,9,6,  6,9,16,19,19,16,9,6,  3,6,12,15,15,12,6,3,  -3,0,6,8,8,6,0,-3,  -36,-3,3,6,6,3,-3,-36},
     {-56,100,3,-116,-116,3,100,-56,  -3,0,-75,-79,-79,-75,0,-3,  3,6,12,15,15,12,6,3,  6,9,15,19,19,15,9,6,  6,9,15,18,18,15,9,6,  3,6,12,15,15,12,6,3,  -3,0,6,9,9,6,0,-3,-   16,-3,3,6,6,3,-3,-16}},
  EVAL_PSQT_EG[6][64] =
    {{0,0,0,0,0,0,0,0,  -5,0,10,15,15,10,0,-5,  5,10,20,25,25,20,10,5,  45,50,60,95,95,60,50,45,  259,264,274,279,279,274,264,259,  725,730,740,745,745,740,730,725,  995,1000,1010,1015,1015,1010,1000,995,  0,0,0,0,0,0,0,0},
     {-195,-30,-20,5,5,-20,-30,-195,  -3,0,10,15,15,10,0,-3,  -10,10,20,25,25,20,10,-10,  10,18,25,30,30,25,18,10,  10,15,28,35,35,28,15,10,  -8,10,20,25,25,20,10,-8,  -2,0,10,15,15,10,0,-2,  -155,-10,-5,10,10,-5,-10,-155},
     {-220,-55,-45,-15,-15,-45,-55,-220,  -15,0,10,15,15,10,0,-15,  -10,10,20,25,25,20,10,-10,  10,15,25,30,30,25,15,10,  15,15,25,30,30,25,15,15,  -5,10,20,25,25,20,10,-5,  -10,0,10,15,15,10,0,-10,  -155,-30,-20,10,10,-20,-30,-155},
     {-70,-5,5,10,10,5,-5,-70,  -5,0,10,15,15,10,0,-5,  5,10,20,25,25,20,10,5,  10,15,25,30,30,25,15,10,  10,15,25,30,30,25,15,10,  5,10,20,25,25,20,10,5,  0,5,15,20,20,15,5,0,  -30,-5,5,10,10,5,-5,-30},
     {-50,-1,7,10,10,7,-1,-50,  -2,7,10,15,15,10,7,-2,  5,10,20,25,25,20,10,5,  10,15,25,50,50,25,15,10,  10,15,25,55,55,25,15,10,  5,10,20,25,25,20,10,5,  -2,7,10,15,15,10,7,-2,  -20,-1,8,10,10,8,-1,-20},
     {-70,-25,5,7,7,5,-25,-70,  -5,0,10,15,15,10,0,-5,  5,10,20,25,25,20,10,5,  10,15,45,57,57,45,15,10,  10,15,35,57,57,35,15,10,  5,10,20,27,27,20,10,5,  -5,0,10,15,15,10,0,-5,  -30,-5,5,10,10,5,-5,-30}},
  ROOK_VECTORS[8] = {1,0,0,1,0,-1,-1,0}, BISHOP_VECTORS[8] = {1,1,-1,-1,1,-1,-1,1}, KING_VECTORS[2 * 8] = {1,0,0,1,0,-1,-1,0,1,1,-1,-1,1,-1,-1,1}, KNIGHT_VECTORS[2 * 8] = {2,1,-2,1,2,-1,-2,-1,1,2,-1,2,1,-2,-1,-2};

static const uint64_t
  ROOK_MASK[64] =
    {0x101010101017eULL,0x202020202027cULL,0x404040404047aULL,0x8080808080876ULL,0x1010101010106eULL,0x2020202020205eULL,0x4040404040403eULL,0x8080808080807eULL,
     0x1010101017e00ULL,0x2020202027c00ULL,0x4040404047a00ULL,0x8080808087600ULL,0x10101010106e00ULL,0x20202020205e00ULL,0x40404040403e00ULL,0x80808080807e00ULL,
     0x10101017e0100ULL,0x20202027c0200ULL,0x40404047a0400ULL,0x8080808760800ULL,0x101010106e1000ULL,0x202020205e2000ULL,0x404040403e4000ULL,0x808080807e8000ULL,
     0x101017e010100ULL,0x202027c020200ULL,0x404047a040400ULL,0x8080876080800ULL,0x1010106e101000ULL,0x2020205e202000ULL,0x4040403e404000ULL,0x8080807e808000ULL,
     0x1017e01010100ULL,0x2027c02020200ULL,0x4047a04040400ULL,0x8087608080800ULL,0x10106e10101000ULL,0x20205e20202000ULL,0x40403e40404000ULL,0x80807e80808000ULL,
     0x17e0101010100ULL,0x27c0202020200ULL,0x47a0404040400ULL,0x8760808080800ULL,0x106e1010101000ULL,0x205e2020202000ULL,0x403e4040404000ULL,0x807e8080808000ULL,
     0x7e010101010100ULL,0x7c020202020200ULL,0x7a040404040400ULL,0x76080808080800ULL,0x6e101010101000ULL,0x5e202020202000ULL,0x3e404040404000ULL,0x7e808080808000ULL,
     0x7e01010101010100ULL,0x7c02020202020200ULL,0x7a04040404040400ULL,0x7608080808080800ULL,0x6e10101010101000ULL,0x5e20202020202000ULL,0x3e40404040404000ULL,0x7e80808080808000ULL},
  ROOK_MOVE_MAGICS[64] =
    {0x101010101017eULL,0x202020202027cULL,0x404040404047aULL,0x8080808080876ULL,0x1010101010106eULL,0x2020202020205eULL,0x4040404040403eULL,0x8080808080807eULL,
     0x1010101017e00ULL,0x2020202027c00ULL,0x4040404047a00ULL,0x8080808087600ULL,0x10101010106e00ULL,0x20202020205e00ULL,0x40404040403e00ULL,0x80808080807e00ULL,
     0x10101017e0100ULL,0x20202027c0200ULL,0x40404047a0400ULL,0x8080808760800ULL,0x101010106e1000ULL,0x202020205e2000ULL,0x404040403e4000ULL,0x808080807e8000ULL,
     0x101017e010100ULL,0x202027c020200ULL,0x404047a040400ULL,0x8080876080800ULL,0x1010106e101000ULL,0x2020205e202000ULL,0x4040403e404000ULL,0x8080807e808000ULL,
     0x1017e01010100ULL,0x2027c02020200ULL,0x4047a04040400ULL,0x8087608080800ULL,0x10106e10101000ULL,0x20205e20202000ULL,0x40403e40404000ULL,0x80807e80808000ULL,
     0x17e0101010100ULL,0x27c0202020200ULL,0x47a0404040400ULL,0x8760808080800ULL,0x106e1010101000ULL,0x205e2020202000ULL,0x403e4040404000ULL,0x807e8080808000ULL,
     0x7e010101010100ULL,0x7c020202020200ULL,0x7a040404040400ULL,0x76080808080800ULL,0x6e101010101000ULL,0x5e202020202000ULL,0x3e404040404000ULL,0x7e808080808000ULL,
     0x7e01010101010100ULL,0x7c02020202020200ULL,0x7a04040404040400ULL,0x7608080808080800ULL,0x6e10101010101000ULL,0x5e20202020202000ULL,0x3e40404040404000ULL,0x7e80808080808000ULL},
  BISHOP_MASK[64] =
    {0x40201008040200ULL,0x402010080400ULL,0x4020100a00ULL,0x40221400ULL,0x2442800ULL,0x204085000ULL,0x20408102000ULL,0x2040810204000ULL,
     0x20100804020000ULL,0x40201008040000ULL,0x4020100a0000ULL,0x4022140000ULL,0x244280000ULL,0x20408500000ULL,0x2040810200000ULL,0x4081020400000ULL,
     0x10080402000200ULL,0x20100804000400ULL,0x4020100a000a00ULL,0x402214001400ULL,0x24428002800ULL,0x2040850005000ULL,0x4081020002000ULL,0x8102040004000ULL,
     0x8040200020400ULL,0x10080400040800ULL,0x20100a000a1000ULL,0x40221400142200ULL,0x2442800284400ULL,0x4085000500800ULL,0x8102000201000ULL,0x10204000402000ULL,
     0x4020002040800ULL,0x8040004081000ULL,0x100a000a102000ULL,0x22140014224000ULL,0x44280028440200ULL,0x8500050080400ULL,0x10200020100800ULL,0x20400040201000ULL,
     0x2000204081000ULL,0x4000408102000ULL,0xa000a10204000ULL,0x14001422400000ULL,0x28002844020000ULL,0x50005008040200ULL,0x20002010080400ULL,0x40004020100800ULL,
     0x20408102000ULL,0x40810204000ULL,0xa1020400000ULL,0x142240000000ULL,0x284402000000ULL,0x500804020000ULL,0x201008040200ULL,0x402010080400ULL,
     0x2040810204000ULL,0x4081020400000ULL,0xa102040000000ULL,0x14224000000000ULL,0x28440200000000ULL,0x50080402000000ULL,0x20100804020000ULL,0x40201008040200ULL},
  BISHOP_MOVE_MAGICS[64] =
    {0x40201008040200ULL,0x402010080400ULL,0x4020100a00ULL,0x40221400ULL,0x2442800ULL,0x204085000ULL,0x20408102000ULL,0x2040810204000ULL,
     0x20100804020000ULL,0x40201008040000ULL,0x4020100a0000ULL,0x4022140000ULL,0x244280000ULL,0x20408500000ULL,0x2040810200000ULL,0x4081020400000ULL,
     0x10080402000200ULL,0x20100804000400ULL,0x4020100a000a00ULL,0x402214001400ULL,0x24428002800ULL,0x2040850005000ULL,0x4081020002000ULL,0x8102040004000ULL,
     0x8040200020400ULL,0x10080400040800ULL,0x20100a000a1000ULL,0x40221400142200ULL,0x2442800284400ULL,0x4085000500800ULL,0x8102000201000ULL,0x10204000402000ULL,
     0x4020002040800ULL,0x8040004081000ULL,0x100a000a102000ULL,0x22140014224000ULL,0x44280028440200ULL,0x8500050080400ULL,0x10200020100800ULL,0x20400040201000ULL,
     0x2000204081000ULL,0x4000408102000ULL,0xa000a10204000ULL,0x14001422400000ULL,0x28002844020000ULL,0x50005008040200ULL,0x20002010080400ULL,0x40004020100800ULL,
     0x20408102000ULL,0x40810204000ULL,0xa1020400000ULL,0x142240000000ULL,0x284402000000ULL,0x500804020000ULL,0x201008040200ULL,0x402010080400ULL,
     0x2040810204000ULL,0x4081020400000ULL,0xa102040000000ULL,0x14224000000000ULL,0x28440200000000ULL,0x50080402000000ULL,0x20100804020000ULL,0x40201008040200ULL},
  ROOK_MAGIC[64] = {
    0x548001400080106cULL,0x900184000110820ULL,0x428004200a81080ULL,0x140088082000c40ULL,0x1480020800011400ULL,0x100008804085201ULL,0x2a40220001048140ULL,0x50000810000482aULL,
    0x250020100020a004ULL,0x3101880100900a00ULL,0x200a040a00082002ULL,0x1004300044032084ULL,0x2100408001013ULL,0x21f00440122083ULL,0xa204280406023040ULL,0x2241801020800041ULL,
    0xe10100800208004ULL,0x2010401410080ULL,0x181482000208805ULL,0x4080101000021c00ULL,0xa250210012080022ULL,0x4210641044000827ULL,0x8081a02300d4010ULL,0x8008012000410001ULL,
    0x28c0822120108100ULL,0x500160020aa005ULL,0xc11050088c1000ULL,0x48c00101000a288ULL,0x494a184408028200ULL,0x20880100240006ULL,0x10b4010200081ULL,0x40a200260000490cULL,
    0x22384003800050ULL,0x7102001a008010ULL,0x80020c8010900c0ULL,0x100204082a001060ULL,0x8000118188800428ULL,0x58e0020009140244ULL,0x100145040040188dULL,0x44120220400980ULL,
    0x114001007a00800ULL,0x80a0100516304000ULL,0x7200301488001000ULL,0x1000151040808018ULL,0x3000a200010e0020ULL,0x1000849180802810ULL,0x829100210208080ULL,0x1004050021528004ULL,
    0x61482000c41820b0ULL,0x241001018a401a4ULL,0x45020c009cc04040ULL,0x308210c020081200ULL,0xa000215040040ULL,0x10a6024001928700ULL,0x42c204800c804408ULL,0x30441a28614200ULL,
    0x40100229080420aULL,0x9801084000201103ULL,0x8408622090484202ULL,0x4022001048a0e2ULL,0x280120020049902ULL,0x1200412602009402ULL,0x914900048020884ULL,0x104824281002402ULL},
  BISHOP_MAGIC[64] = {
    0x2890208600480830ULL,0x324148050f087ULL,0x1402488a86402004ULL,0xc2210a1100044bULL,0x88450040b021110cULL,0xc0407240011ULL,0xd0246940cc101681ULL,0x1022840c2e410060ULL,
    0x4a1804309028d00bULL,0x821880304a2c0ULL,0x134088090100280ULL,0x8102183814c0208ULL,0x518598604083202ULL,0x67104040408690ULL,0x1010040020d000ULL,0x600001028911902ULL,
    0x8810183800c504c4ULL,0x2628200121054640ULL,0x28003000102006ULL,0x4100c204842244ULL,0x1221c50102421430ULL,0x80109046e0844002ULL,0xc128600019010400ULL,0x812218030404c38ULL,
    0x1224152461091c00ULL,0x1c820008124000aULL,0xa004868015010400ULL,0x34c080004202040ULL,0x200100312100c001ULL,0x4030048118314100ULL,0x410000090018ULL,0x142c010480801ULL,
    0x8080841c1d004262ULL,0x81440f004060406ULL,0x400a090008202ULL,0x2204020084280080ULL,0xb820060400008028ULL,0x110041840112010ULL,0x8002080a1c84400ULL,0x212100111040204aULL,
    0x9412118200481012ULL,0x804105002001444cULL,0x103001280823000ULL,0x40088e028080300ULL,0x51020d8080246601ULL,0x4a0a100e0804502aULL,0x5042028328010ULL,0xe000808180020200ULL,
    0x1002020620608101ULL,0x1108300804090c00ULL,0x180404848840841ULL,0x100180040ac80040ULL,0x20840000c1424001ULL,0x82c00400108800ULL,0x28c0493811082aULL,0x214980910400080cULL,
    0x8d1a0210b0c000ULL,0x164c500ca0410cULL,0xc6040804283004ULL,0x14808001a040400ULL,0x180450800222a011ULL,0x600014600490202ULL,0x21040100d903ULL,0x10404821000420ULL},
  EVAL_FREE_COLUMNS[8] = {0x0202020202020202ULL,0x0505050505050505ULL,0x0A0A0A0A0A0A0A0AULL,0x1414141414141414ULL,0x2828282828282828ULL,0x5050505050505050ULL,0xA0A0A0A0A0A0A0A0ULL,0x4040404040404040ULL};

// Variables

static struct BOARD_T
  BOARD_TMP = {{0},{0},{0},0,0,0,0,0,0,0,0}, *BOARD = &BOARD_TMP, *MGEN_MOVES = 0, *BOARD_ORIGINAL = 0, ROOT_MOVES[MAX_MOVES] = {{{0},{0},{0},0,0,0,0,0,0,0,0}};

static int
  MAX_DEPTH = DEPTH_LIMIT, QS_DEPTH = 4, LEVEL = 100, EVAL_POS_MG = 0, EVAL_POS_EG = 0, EVAL_MAT_MG = 0, EVAL_MAT_EG = 0, EVAL_WHITE_KING_SQ = 0, EVAL_BLACK_KING_SQ = 0, EVAL_BOTH_N = 0,
  TOKENS_N = 0, TOKENS_I = 0, DEPTH = 0, BEST_SCORE = 0, ROOT_MOVES_N = 0, KING_W = 0, KING_B = 0, MGEN_MOVES_N = 0, EVAL_PSQT_MG_B[6][64] = {{0}}, EVAL_PSQT_EG_B[6][64] = {{0}},
  ROOK_W[2] = {0}, ROOK_B[2] = {0}, MOVEOVERHEAD = 15, MVV[6][6] = {{85,96,97,98,99,100}, {84,86,93,94,95,100}, {82,83,87,91,92,100}, {79,80,81,88,90,100}, {75,76,77,78,89,100}, {70,71,72,73,74,100}};

static char
  FEN[90] = STARTPOS, FEN_SPLITS[5][90] = {{0}}, TOKENS[MAX_TOKENS][90] = {{0}};

static float
  EVAL_DRAWISH_FACTOR = 1.0f;

static uint64_t
  EVAL_WHITE = 0, EVAL_BLACK = 0, EVAL_EMPTY = 0, EVAL_BOTH = 0, MGEN_BLACK = 0, MGEN_BOTH = 0, MGEN_EMPTY = 0, MGEN_GOOD = 0, MGEN_PAWN_SQ = 0, MGEN_WHITE = 0, STOP_SEARCH_TIME = 0, NODES = 0,
  PAWN_1_MOVES_W[64] = {0}, PAWN_1_MOVES_B[64] = {0}, PAWN_2_MOVES_W[64] = {0}, PAWN_2_MOVES_B[64] = {0}, ZOBRIST_EP[64]= {0}, ZOBRIST_CASTLE[16] = {0}, ZOBRIST_WTM[2] = {0}, ZOBRIST_BOARD[13][64] = {{0}},
  DRAWS[13] = {0}, CASTLE_W[2] = {0}, CASTLE_B[2] = {0}, CASTLE_EMPTY_W[2] = {0}, CASTLE_EMPTY_B[2] = {0}, EVAL_KING_RING[64] = {0}, EVAL_COLUMNS_UP[64] = {0}, EVAL_COLUMNS_DOWN[64] = {0},
  BISHOP_MOVES[64] = {0}, ROOK_MOVES[64] = {0}, QUEEN_MOVES[64] = {0}, KNIGHT_MOVES[64] = {0}, KING_MOVES[64] = {0}, PAWN_CHECKS_W[64] = {0}, PAWN_CHECKS_B[64] = {0}, REPETITION_POSITIONS[128] = {0},
  BISHOP_MAGIC_MOVES[64][512] = {{0}}, ROOK_MAGIC_MOVES[64][4096] = {{0}}, RANDOM_SEED = 131783;

static bool
  CHESS960 = 0, KOTH960 = 0, WTM = 0, STOP_SEARCH = 0, UNDERPROMOS = 1, ANALYZING = 0;

static struct HASH_T
  HASH[HASH_KEY + 1] = {{0,0,0,0,0,0}};

// Prototypes

static int SearchB(const int, int, const int, const int);
static int QSearchB(const int, int, const int);
static int Eval(const bool);
static void CreateTokens(char*);

// Utils

static inline int Max(const int x, const int y) {return x > y ? x : y;}
static inline int Min(const int x, const int y) {return x < y ? x : y;}
static int Between(const int x, const int y, const int z) {return Max(x, Min(y, z));}
static inline uint8_t Xcoord(const uint64_t bb) {return bb & 7;}
static inline uint8_t Ycoord(const uint64_t bb) {return bb >> 3;}
static int Abs(const int x) {return x < 0 ? -x : x;}
static uint64_t Nps(const uint64_t nodes, const uint64_t ms) {return (1000 * nodes) / (ms + 1);}
static inline uint64_t ClearBit(const uint64_t bb) {return bb & (bb - 1);}
static inline uint64_t Bit(const int nbits) {return 0x1ULL << nbits;}
static int Mirror(const int square) {return square ^ 56;}
static bool OnBoard(const int x, const int y) {return x >= 0 && x <= 7 && y >= 0 && y <= 7;}
static void StringJoin(char *str1, const char *str2) {strcpy(str1 + strlen(str1), str2);}
static inline int Ctz(const uint64_t bb) {return __builtin_ctzll(bb);}
static inline int PopCount(const uint64_t bb) {return __builtin_popcountll(bb);}
static uint64_t BishopMagicIndex(const int square, const uint64_t mask) {return ((mask & BISHOP_MASK[square]) * BISHOP_MAGIC[square]) >> 55;}
static uint64_t RookMagicIndex(const int square, const uint64_t mask) {return ((mask & ROOK_MASK[square]) * ROOK_MAGIC[square]) >> 52;}
static uint64_t BishopMagicMoves(const int square, const uint64_t mask) {return BISHOP_MAGIC_MOVES[square][BishopMagicIndex(square, mask)];}
static uint64_t RookMagicMoves(const int square, const uint64_t mask) {return ROOK_MAGIC_MOVES[square][RookMagicIndex(square, mask)];}

static void Print(const char *format, ...) {
  va_list va;
  va_start(va, format);
  vfprintf(stdout, format, va);
  va_end(va);
  fprintf(stdout, "\n");
  fflush(stdout);
}

static void Assert(const bool test, const char *msg) {
  if (test) return;
  Print(msg);
  exit(EXIT_FAILURE);
}

static const char *MoveStr(const int from, const int to) {
  static char move[6] = "";
  move[0] = 'a' + Xcoord(from);
  move[1] = '1' + Ycoord(from);
  move[2] = 'a' + Xcoord(to);
  move[3] = '1' + Ycoord(to);
  move[4] = '\0';
  return move;
}

static uint64_t Now(void) {
  struct timeval tv;
  if (gettimeofday(&tv, NULL)) return 0;
  return (uint64_t) (1000 * tv.tv_sec + tv.tv_usec / 1000);
}

static uint64_t Mixer(uint64_t val) {return (val << 7) ^ (val >> 5);}

static uint64_t RandomBB(void) {
  static uint64_t va = 0X12311227ULL, vb = 0X1931311ULL, vc = 0X13138141ULL;
  va ^= vb + vc;
  vb ^= vb * vc + 0x1717711ULL;
  vc  = (3 * vc) + 1;
  return Mixer(va) ^ Mixer(vb) ^ Mixer(vc);
}

static uint64_t Random8x64(void) {
  uint64_t ret = 0;
  for (int i = 0; i < 8; i++) ret ^= RandomBB() << (8 * i);
  return ret;
}

static int Random1(const int max) {
  const uint64_t rnum = (RANDOM_SEED ^ RandomBB()) & 0x1FFFULL; // 8191
  RANDOM_SEED = (RANDOM_SEED << 5) ^ (RANDOM_SEED + 1) ^ (RANDOM_SEED >> 3);
  return (int) (((float) max) * (0.00012207f * ((float) rnum)));
}

static int Random(const int x, const int y) {return x + Random1(y - x + 1);}

static void Input(void) {
  static char str[8192] = "";
  str[0] = '\0';
  Assert(fgets(str, sizeof(str), stdin) != NULL, "Error #1: Read line returns NULL !");
  CreateTokens(str);
}

static char PromoLetter(const char piece) {
  switch (Abs(piece)) {
  case 2:  return 'n';
  case 3:  return 'b';
  case 4:  return 'r';
  default: return 'q';}
}

static const char *MoveName(const struct BOARD_T *move) {
  static char str[6] = "";
  int from = move->from, to = move->to;
  switch (move->type) {
  case 1: from = KING_W; to = CHESS960 ? ROOK_W[0] : 6; break;
  case 2: from = KING_W; to = CHESS960 ? ROOK_W[1] : 2; break;
  case 3: from = KING_B; to = CHESS960 ? ROOK_B[0] : 56 + 6; break;
  case 4: from = KING_B; to = CHESS960 ? ROOK_B[1] : 56 + 2; break;
  case 5: case 6: case 7: case 8:
    strcpy(str, MoveStr(from, to));
    str[4] = PromoLetter(move->board[to]);
    str[5] = '\0';
    return str;}
  strcpy(str, MoveStr(from, to));
  return str;
}

static inline uint64_t White(void) {return BOARD->white[0] | BOARD->white[1] | BOARD->white[2] | BOARD->white[3] | BOARD->white[4] | BOARD->white[5];}
static inline uint64_t Black(void) {return BOARD->black[0] | BOARD->black[1] | BOARD->black[2] | BOARD->black[3] | BOARD->black[4] | BOARD->black[5];}
static inline uint64_t Both(void) {return White() | Black();}

// Hash

static inline uint64_t Hash(const uint8_t wtm) {
  uint64_t hash = ZOBRIST_EP[BOARD->epsq + 1] ^ ZOBRIST_WTM[wtm] ^ ZOBRIST_CASTLE[BOARD->castle], both = Both();
  for (; both; both = ClearBit(both)) {const uint8_t sq = Ctz(both); hash ^= ZOBRIST_BOARD[BOARD->board[sq] + 6][sq];}
  return hash;
}

// Tokenizer

static void TokenAdd(const char *token) {if (TOKENS_N >= MAX_TOKENS) return; strcpy(TOKENS[TOKENS_N], token); TOKENS_N++;}
static bool TokenOk(void) {return TOKENS_I < TOKENS_N;}
static const char *TokenCurrent(void) {return TokenOk() ? TOKENS[TOKENS_I] : "";}
static void TokenPop(const int howmany) {TOKENS_I += howmany;}
static bool TokenIs(const char *token) {return TokenOk() && !strcmp(token, TokenCurrent());}
static bool Token(const char *token) {if (TokenIs(token)) {TokenPop(1); return 1;} return 0;}
static bool TokenPeek(const char *str, const int index) {return TOKENS_I + index < TOKENS_N ? !strcmp(str, TOKENS[TOKENS_I + index]) : 0;}
static int TokenNumber(void) {return TokenOk() ? atoi(TOKENS[TOKENS_I]) : 0;}
static void CreateTokens(char *tokenstr) {TOKENS_I = TOKENS_N = 0; for (const char *token = strtok(tokenstr, " \n\t"); token != NULL; token = strtok(NULL, " \n\t")) TokenAdd(token);}

// Board stuff

static void BuildBitboards(void) {
  for (int i = 0; i < 6; i++) BOARD->white[i] = BOARD->black[i] = 0;
  for (int i = 0; i < 64; i++)
    if (     BOARD->board[i] > 0) BOARD->white[+BOARD->board[i] - 1] |= Bit(i);
    else if (BOARD->board[i] < 0) BOARD->black[-BOARD->board[i] - 1] |= Bit(i);
}

static uint64_t Fill(int from, const int to) {
  uint64_t ret   = Bit(from);
  const int diff = from > to ? -1 : 1;
  if (from < 0 || to < 0 || from > 63 || to > 63) return 0;
  if (from == to) return ret;
  do {
    from += diff;
    ret  |= Bit(from);
  } while (from != to);
  return ret;
}

static void FindKings(void) {for (int i = 0; i < 64; i++) if (BOARD->board[i] == +6) KING_W = i; else if (BOARD->board[i] == -6) KING_B = i;}

static void BuildCastlingBitboards(void) {
  CASTLE_W[0] = Fill(KING_W, 6);
  CASTLE_W[1] = Fill(KING_W, 2);
  CASTLE_B[0] = Fill(KING_B, 56 + 6);
  CASTLE_B[1] = Fill(KING_B, 56 + 2);
  CASTLE_EMPTY_W[0] = (CASTLE_W[0] | Fill(ROOK_W[0], 5     )) ^ (Bit(KING_W) | Bit(ROOK_W[0]));
  CASTLE_EMPTY_B[0] = (CASTLE_B[0] | Fill(ROOK_B[0], 56 + 5)) ^ (Bit(KING_B) | Bit(ROOK_B[0]));
  CASTLE_EMPTY_W[1] = (CASTLE_W[1] | Fill(ROOK_W[1], 3     )) ^ (Bit(KING_W) | Bit(ROOK_W[1]));
  CASTLE_EMPTY_B[1] = (CASTLE_B[1] | Fill(ROOK_B[1], 56 + 3)) ^ (Bit(KING_B) | Bit(ROOK_B[1]));
  for (int i = 0; i < 2; i++) {
    CASTLE_EMPTY_W[i] &= 0xFFULL;
    CASTLE_W[i]       &= 0xFFULL;
    CASTLE_EMPTY_B[i] &= 0xFF00000000000000ULL;
    CASTLE_B[i]       &= 0xFF00000000000000ULL;
  }
}

static int Piece(const char piece) {for (int i = 0; i < 6; i++) {if (piece == "pnbrqk"[i]) return -i - 1; else if (piece == "PNBRQK"[i]) return +i + 1;} return 0;}

static void FenBoard(const char *board) {
  for (int sq = 56; *board != '\0' && sq >= 0; board++) {
    if (*board == '/') {sq -= 16;} else if (*board >= '0' && *board <= '9') {sq += *board - '0';} else {BOARD->board[sq++] = Piece(*board);}
  }
}

static void FenKQkq(const char *kqkq) {
  for (; *kqkq != '\0'; kqkq++)
    if (     *kqkq == 'K') {ROOK_W[0] = 7;      BOARD->castle |= 1;}
    else if (*kqkq == 'Q') {ROOK_W[1] = 0;      BOARD->castle |= 2;}
    else if (*kqkq == 'k') {ROOK_B[0] = 56 + 7; BOARD->castle |= 4;}
    else if (*kqkq == 'q') {ROOK_B[1] = 56 + 0; BOARD->castle |= 8;}
    else if (*kqkq >= 'A' && *kqkq <= 'H') {
      const int tmp = *kqkq - 'A';
      if (tmp > KING_W) {ROOK_W[0] = tmp; BOARD->castle |= 1;} else if (tmp < KING_W) {ROOK_W[1] = tmp; BOARD->castle |= 2;}
    } else if (*kqkq >= 'a' && *kqkq <= 'h') {
      const int tmp = *kqkq - 'a';
      if (tmp > Xcoord(KING_B)) {ROOK_B[0] = 56 + tmp; BOARD->castle |= 4;} else if (tmp < Xcoord(KING_B)) {ROOK_B[1] = 56 + tmp; BOARD->castle |= 8;}
    }
}

static void FenEp(const char *ep) {
  if (*ep == '-') return;
  BOARD->epsq = Between(8, (*ep - 'a') + (8 * (*(ep + 1) - '1')), 56);
}

static void FenRule50(const char *rule50) {
  if (*rule50 == '-') return;
  BOARD->rule50 = Between(0, atoi(rule50), 100);
}

static int FenSplit(const char *fen) {
  for (int i = 0; i < 5; i++) FEN_SPLITS[i][0] = '\0';
  for (int split = 0; split < 5; split++) {
    while (*fen == ' ') fen++;
    if (*fen == '\0') return split;
    for (int len = 0; *fen != ' ' && *fen != '\0'; len++, fen++) {
      FEN_SPLITS[split][len]     = *fen;
      FEN_SPLITS[split][len + 1] = '\0';
    }
  }
  return 4;
}

static void FenCreate(const char *fen) {
  Assert(FenSplit(fen) == 4, "Error #2: Bad fen !");
  FenBoard(FEN_SPLITS[0]);
  WTM = FEN_SPLITS[1][0] == 'w';
  FindKings();
  FenKQkq(FEN_SPLITS[2]);
  BuildCastlingBitboards();
  FenEp(FEN_SPLITS[3]);
  FenRule50(FEN_SPLITS[4]);
}

static void FenReset(void) {
  const struct BOARD_T brd = {{0},{0},{0},0,0,0,0,0,0,0,0};
  BOARD_TMP   = brd;
  BOARD       = &BOARD_TMP;
  WTM         = 1;
  BOARD->epsq = -1;
  KING_W = KING_B = 0;
  for (int i = 0; i < 2; i++) ROOK_W[i] = ROOK_B[i] = 0;
}

static void Fen(const char *fen) {
  FenReset();
  FenCreate(fen);
  BuildBitboards();
  Assert(PopCount(BOARD->white[5]) == 1 && PopCount(BOARD->black[5]) == 1, "Error #3: Bad king count !");
}

// Checks

static bool ChecksHereW(const int square) {
  const uint64_t both = Both();
  return ((PAWN_CHECKS_B[square] & BOARD->white[0]) | (KNIGHT_MOVES[square] & BOARD->white[1]) | (BishopMagicMoves(square, both) & (BOARD->white[2] | BOARD->white[4]))
         | (RookMagicMoves(square, both) & (BOARD->white[3] | BOARD->white[4])) | (KING_MOVES[square] & BOARD->white[5]));
}

static bool ChecksHereB(const int square) {
  const uint64_t both = Both();
  return ((PAWN_CHECKS_W[square] & BOARD->black[0]) | (KNIGHT_MOVES[square] & BOARD->black[1]) | (BishopMagicMoves(square, both) & (BOARD->black[2] | BOARD->black[4]))
         | (RookMagicMoves(square, both) & (BOARD->black[3] | BOARD->black[4])) | (KING_MOVES[square] & BOARD->black[5]));
}

static bool ChecksCastleW(uint64_t squares) {for (; squares; squares = ClearBit(squares)) {if (ChecksHereW(Ctz(squares))) return 1;} return 0;}
static bool ChecksCastleB(uint64_t squares) {for (; squares; squares = ClearBit(squares)) {if (ChecksHereB(Ctz(squares))) return 1;} return 0;}
static inline bool ChecksW(void) {return ChecksHereW(Ctz(BOARD->black[5]));}
static inline bool ChecksB(void) {return ChecksHereB(Ctz(BOARD->white[5]));}

// Sorting

static inline void Swap(struct BOARD_T *brda, struct BOARD_T *brdb) {const struct BOARD_T tmp = *brda; *brda = *brdb; *brdb = tmp;}
static void SortNthMoves(const int nth) {for (int i = 0; i < nth; i++) {for (int j = i + 1; j < MGEN_MOVES_N; j++) {if (MGEN_MOVES[j].score > MGEN_MOVES[i].score) {Swap(MGEN_MOVES + j, MGEN_MOVES + i);}}}}
static int EvaluateMoves(void) {int tactics = 0; for (int i = 0; i < MGEN_MOVES_N; i++) {if (MGEN_MOVES[i].score) {tactics++;} MGEN_MOVES[i].index = i;} return tactics;}
static void SortAll(void) {SortNthMoves(MGEN_MOVES_N);}

static void SortByHash(const struct HASH_T *entry, const uint64_t hash) {
  if (entry->sort_hash == hash) {
    if (entry->killer) MGEN_MOVES[entry->killer - 1].score += 10000; else if (entry->good) MGEN_MOVES[entry->good - 1].score += 500;
    if (entry->quiet) MGEN_MOVES[entry->quiet - 1].score += 1000;
  }
  SortNthMoves(EvaluateMoves());
}

static void EvaluateRootMoves(void) {
  struct BOARD_T *tmp = BOARD;
  for (int i = 0; i < ROOT_MOVES_N; i++) {
    BOARD = ROOT_MOVES + i;
    BOARD->score += (BOARD->type >= 5 && BOARD->type <= 7 ? -10000 : 0) + (BOARD->type >= 1 && BOARD->type <= 4 ? 5000 : 0) + ((WTM ? 1 : -1) * Eval(WTM)) + Random(-5, +5);
  }
  BOARD = tmp;
}

static void SortRoot(const int index) {
  if (!index) return;
  const struct BOARD_T tmp = ROOT_MOVES[index];
  for (int i = index; i > 0; i--) ROOT_MOVES[i] = ROOT_MOVES[i - 1];
  ROOT_MOVES[0] = tmp;
}

// Move generator

static void HandleCastlingW(const int mtype, const int from, const int to) {
  MGEN_MOVES[MGEN_MOVES_N] = *BOARD;
  BOARD          = &MGEN_MOVES[MGEN_MOVES_N];
  BOARD->score   = 0;
  BOARD->epsq    = -1;
  BOARD->from    = from;
  BOARD->to      = to;
  BOARD->type    = mtype;
  BOARD->castle &= 4 | 8;
  BOARD->rule50  = 0;
}

static void AddCastleOOW(void) {
  if (ChecksCastleB(CASTLE_W[0])) return;
  HandleCastlingW(1, KING_W, 6);
  BOARD->board[ROOK_W[0]] = 0;
  BOARD->board[KING_W]    = 0;
  BOARD->board[5]         = 4;
  BOARD->board[6]         = 6;
  BOARD->white[3]         = (BOARD->white[3] ^ Bit(ROOK_W[0])) | Bit(5);
  BOARD->white[5]         = (BOARD->white[5] ^ Bit(KING_W))    | Bit(6);
  if (ChecksB()) return;
  MGEN_MOVES_N++;
}

static void AddCastleOOOW(void) {
  if (ChecksCastleB(CASTLE_W[1])) return;
  HandleCastlingW(2, KING_W, 2);
  BOARD->board[ROOK_W[1]] = 0;
  BOARD->board[KING_W]    = 0;
  BOARD->board[3]         = 4;
  BOARD->board[2]         = 6;
  BOARD->white[3]         = (BOARD->white[3] ^ Bit(ROOK_W[1])) | Bit(3);
  BOARD->white[5]         = (BOARD->white[5] ^ Bit(KING_W))    | Bit(2);
  if (ChecksB()) return;
  MGEN_MOVES_N++;
}

static void MgenCastlingMovesW(void) {
  if ((BOARD->castle & 1) && !(CASTLE_EMPTY_W[0] & MGEN_BOTH)) {AddCastleOOW();  BOARD = BOARD_ORIGINAL;}
  if ((BOARD->castle & 2) && !(CASTLE_EMPTY_W[1] & MGEN_BOTH)) {AddCastleOOOW(); BOARD = BOARD_ORIGINAL;}
}

static void HandleCastlingB(const int mtype, const int from, const int to) {
  MGEN_MOVES[MGEN_MOVES_N] = *BOARD;
  BOARD          = &MGEN_MOVES[MGEN_MOVES_N];
  BOARD->score   = 0;
  BOARD->epsq    = -1;
  BOARD->from    = from;
  BOARD->to      = to;
  BOARD->type    = mtype;
  BOARD->castle &= 1 | 2;
  BOARD->rule50  = 0;
}

static void AddCastleOOB(void) {
  if (ChecksCastleW(CASTLE_B[0])) return;
  HandleCastlingB(3, KING_B, 56 + 6);
  BOARD->board[ROOK_B[0]] = 0;
  BOARD->board[KING_B]    = 0;
  BOARD->board[56 + 5]    = -4;
  BOARD->board[56 + 6]    = -6;
  BOARD->black[3]         = (BOARD->black[3] ^ Bit(ROOK_B[0])) | Bit(56 + 5);
  BOARD->black[5]         = (BOARD->black[5] ^ Bit(KING_B))    | Bit(56 + 6);
  if (ChecksW()) return;
  MGEN_MOVES_N++;
}

static void AddCastleOOOB(void) {
  if (ChecksCastleW(CASTLE_B[1])) return;
  HandleCastlingB(4, KING_B, 56 + 2);
  BOARD->board[ROOK_B[1]] = 0;
  BOARD->board[KING_B]    = 0;
  BOARD->board[56 + 3]    = -4;
  BOARD->board[56 + 2]    = -6;
  BOARD->black[3]         = (BOARD->black[3] ^ Bit(ROOK_B[1])) | Bit(56 + 3);
  BOARD->black[5]         = (BOARD->black[5] ^ Bit(KING_B))    | Bit(56 + 2);
  if (ChecksW()) return;
  MGEN_MOVES_N++;
}

static void MgenCastlingMovesB(void) {
  if ((BOARD->castle & 4) && !(CASTLE_EMPTY_B[0] & MGEN_BOTH)) {AddCastleOOB();  BOARD = BOARD_ORIGINAL;}
  if ((BOARD->castle & 8) && !(CASTLE_EMPTY_B[1] & MGEN_BOTH)) {AddCastleOOOB(); BOARD = BOARD_ORIGINAL;}
}

static void CheckCastlingRightsW(void) {
  if (BOARD->board[KING_W]    != 6) {BOARD->castle &= 4 | 8; return;}
  if (BOARD->board[ROOK_W[0]] != 4) {BOARD->castle &= 2 | 4 | 8;}
  if (BOARD->board[ROOK_W[1]] != 4) {BOARD->castle &= 1 | 4 | 8;}
}

static void CheckCastlingRightsB(void) {
  if (BOARD->board[KING_B]    != -6) {BOARD->castle &= 1 | 2; return;}
  if (BOARD->board[ROOK_B[0]] != -4) {BOARD->castle &= 1 | 2 | 8;}
  if (BOARD->board[ROOK_B[1]] != -4) {BOARD->castle &= 1 | 2 | 4;}
}

static void HandleCastlingRights(void) {
  if (!BOARD->castle) return;
  CheckCastlingRightsW();
  CheckCastlingRightsB();
}

static void ModifyPawnStuffW(const int from, const int to) {
  BOARD->rule50 = 0;
  if (to == BOARD_ORIGINAL->epsq) {
    BOARD->score         = 85;
    BOARD->board[to - 8] = 0;
    BOARD->black[0]     ^= Bit(to - 8);
  } else if (Ycoord(to) - Ycoord(from) == 2) {BOARD->epsq = to - 8;}
    else if (Ycoord(to) == 6) {BOARD->score = 102;} // Pawn on 7th is tactical
}

static void AddPromotionW(const int from, const int to, const int piece) {
  const int eat = BOARD->board[to];
  MGEN_MOVES[MGEN_MOVES_N] = *BOARD;
  BOARD = &MGEN_MOVES[MGEN_MOVES_N];
  BOARD->from        = from;
  BOARD->to          = to;
  BOARD->score       = 100;
  BOARD->type        = 3 + piece;
  BOARD->epsq        = -1;
  BOARD->rule50      = 0;
  BOARD->board[to]   = piece;
  BOARD->board[from] = 0;
  BOARD->white[0]   ^= Bit(from);
  BOARD->white[piece - 1] |= Bit(to);
  if (eat <= -1) BOARD->black[-eat - 1] ^= Bit(to);
  if (ChecksB()) return;
  HandleCastlingRights();
  MGEN_MOVES_N++;
}

static void AddPromotionStuffW(const int from, const int to) {
  if (!UNDERPROMOS) {AddPromotionW(from, to, 5); return;} // =q
  struct BOARD_T *tmp = BOARD;
  for (int piece = 2; piece <= 5; piece++) {AddPromotionW(from, to, piece); BOARD = tmp;} // =nbrq
}

static void AddNormalStuffW(const int from, const int to) {
  const int me = BOARD->board[from], eat = BOARD->board[to];
  MGEN_MOVES[MGEN_MOVES_N] = *BOARD;
  BOARD = &MGEN_MOVES[MGEN_MOVES_N];
  BOARD->from          = from;
  BOARD->to            = to;
  BOARD->score         = 0;
  BOARD->type          = 0;
  BOARD->epsq          = -1;
  BOARD->board[from]   = 0;
  BOARD->board[to]     = me;
  BOARD->white[me - 1] = (BOARD->white[me - 1] ^ Bit(from)) | Bit(to);
  BOARD->rule50++;
  if (eat <= -1) {
    BOARD->black[-eat - 1] ^= Bit(to);
    BOARD->score  = MVV[me - 1][-eat - 1];
    BOARD->rule50 = 0;
  }
  if (BOARD->board[to] == 1) ModifyPawnStuffW(from, to);
  if (ChecksB()) return;
  HandleCastlingRights();
  MGEN_MOVES_N++;
}

static void AddW(const int from, const int to) {if (BOARD->board[from] == 1 && Ycoord(from) == 6) AddPromotionStuffW(from, to); else AddNormalStuffW(from, to);}

static void ModifyPawnStuffB(const int from, const int to) {
  BOARD->rule50 = 0;
  if (to == BOARD_ORIGINAL->epsq) {
    BOARD->score         = 85;
    BOARD->board[to + 8] = 0;
    BOARD->white[0]     ^= Bit(to + 8);
  } else if (Ycoord(to) - Ycoord(from) == -2) {BOARD->epsq = to + 8;}
    else if (Ycoord(to) == 1) {BOARD->score = 102;}
}

static void AddNormalStuffB(const int from, const int to) {
  const int me = BOARD->board[from], eat = BOARD->board[to];
  MGEN_MOVES[MGEN_MOVES_N] = *BOARD;
  BOARD                 = &MGEN_MOVES[MGEN_MOVES_N];
  BOARD->from           = from;
  BOARD->to             = to;
  BOARD->score          = 0;
  BOARD->type           = 0;
  BOARD->epsq           = -1;
  BOARD->board[to]      = me;
  BOARD->board[from]    = 0;
  BOARD->black[-me - 1] = (BOARD->black[-me - 1] ^ Bit(from)) | Bit(to);
  BOARD->rule50++;
  if (eat >= 1) {
    BOARD->white[eat - 1] ^= Bit(to);
    BOARD->score  = MVV[-me - 1][eat - 1];
    BOARD->rule50 = 0;
  }
  if (BOARD->board[to] == -1) ModifyPawnStuffB(from, to);
  if (ChecksW()) return;
  HandleCastlingRights();
  MGEN_MOVES_N++;
}

static void AddPromotionB(const int from, const int to, const int piece) {
  const int eat = BOARD->board[to];
  MGEN_MOVES[MGEN_MOVES_N] = *BOARD;
  BOARD = &MGEN_MOVES[MGEN_MOVES_N];
  BOARD->from        = from;
  BOARD->to          = to;
  BOARD->score       = 100;
  BOARD->type        = 3 + (-piece);
  BOARD->epsq        = -1;
  BOARD->rule50      = 0;
  BOARD->board[from] = 0;
  BOARD->board[to]   = piece;
  BOARD->black[0]   ^= Bit(from);
  BOARD->black[-piece - 1] |= Bit(to);
  if (eat >= 1) BOARD->white[eat - 1] ^= Bit(to);
  if (ChecksW()) return;
  HandleCastlingRights();
  MGEN_MOVES_N++;
}

static void AddPromotionStuffB(const int from, const int to) {
  if (!UNDERPROMOS) {AddPromotionB(from, to, -5); return;}
  struct BOARD_T *tmp = BOARD;
  for (int piece = 2; piece <= 5; piece++) {AddPromotionB(from, to, -piece); BOARD = tmp;}
}

static void AddB(const int from, const int to) {if (BOARD->board[from] == -1 && Ycoord(from) == 1) AddPromotionStuffB(from, to); else AddNormalStuffB(from, to);}
static void AddMovesW(const int from, uint64_t moves) {for (; moves; moves = ClearBit(moves)) {AddW(from, Ctz(moves)); BOARD = BOARD_ORIGINAL;}}
static void AddMovesB(const int from, uint64_t moves) {for (; moves; moves = ClearBit(moves)) {AddB(from, Ctz(moves)); BOARD = BOARD_ORIGINAL;}}

static void MgenSetupW(void) {
  MGEN_WHITE   = White();
  MGEN_BLACK   = Black();
  MGEN_BOTH    = MGEN_WHITE | MGEN_BLACK;
  MGEN_EMPTY   = ~MGEN_BOTH;
  MGEN_PAWN_SQ = BOARD->epsq > 0 ? MGEN_BLACK | (Bit(BOARD->epsq) & 0x0000FF0000000000ULL) : MGEN_BLACK;
}

static void MgenSetupB(void) {
  MGEN_WHITE   = White();
  MGEN_BLACK   = Black();
  MGEN_BOTH    = MGEN_WHITE | MGEN_BLACK;
  MGEN_EMPTY   = ~MGEN_BOTH;
  MGEN_PAWN_SQ = BOARD->epsq > 0 ? MGEN_WHITE | (Bit(BOARD->epsq) & 0x0000000000FF0000ULL) : MGEN_WHITE;
}

static void MgenPawnsW(void) {
  for (uint64_t pieces = BOARD->white[0]; pieces; pieces = ClearBit(pieces)) {
    const int sq = Ctz(pieces);
    AddMovesW(sq, PAWN_CHECKS_W[sq] & MGEN_PAWN_SQ);
    if (Ycoord(sq) == 1) {if (PAWN_1_MOVES_W[sq] & MGEN_EMPTY) AddMovesW(sq, PAWN_2_MOVES_W[sq] & MGEN_EMPTY);} else {AddMovesW(sq, PAWN_1_MOVES_W[sq] & MGEN_EMPTY);}
  }
}

static void MgenPawnsB(void) {
  for (uint64_t pieces = BOARD->black[0]; pieces; pieces = ClearBit(pieces)) {
    const int sq = Ctz(pieces);
    AddMovesB(sq, PAWN_CHECKS_B[sq] & MGEN_PAWN_SQ);
    if (Ycoord(sq) == 6) {if (PAWN_1_MOVES_B[sq] & MGEN_EMPTY) AddMovesB(sq, PAWN_2_MOVES_B[sq] & MGEN_EMPTY);} else {AddMovesB(sq, PAWN_1_MOVES_B[sq] & MGEN_EMPTY);}
  }
}

static void MgenPawnsOnlyCapturesW(void) {
  for (uint64_t pieces = BOARD->white[0]; pieces; pieces = ClearBit(pieces)) {const int sq = Ctz(pieces); AddMovesW(sq, Ycoord(sq) == 6 ? PAWN_1_MOVES_W[sq] & (~MGEN_BOTH) : PAWN_CHECKS_W[sq] & MGEN_PAWN_SQ);}
}
static void MgenPawnsOnlyCapturesB(void) {
  for (uint64_t pieces = BOARD->black[0]; pieces; pieces = ClearBit(pieces)) {const int sq = Ctz(pieces); AddMovesB(sq, Ycoord(sq) == 1 ? PAWN_1_MOVES_B[sq] & (~MGEN_BOTH) : PAWN_CHECKS_B[sq] & MGEN_PAWN_SQ);}
}
static void MgenKnightsW(void) {for (uint64_t pieces = BOARD->white[1]; pieces; pieces = ClearBit(pieces)) {const int sq = Ctz(pieces); AddMovesW(sq, KNIGHT_MOVES[sq] & MGEN_GOOD);}}
static void MgenKnightsB(void) {for (uint64_t pieces = BOARD->black[1]; pieces; pieces = ClearBit(pieces)) {const int sq = Ctz(pieces); AddMovesB(sq, KNIGHT_MOVES[sq] & MGEN_GOOD);}}
static void MgenBishopsPlusQueensW(void) {for (uint64_t pieces = BOARD->white[2] | BOARD->white[4]; pieces; pieces = ClearBit(pieces)) {const int sq = Ctz(pieces); AddMovesW(sq, BishopMagicMoves(sq, MGEN_BOTH) & MGEN_GOOD);}}
static void MgenBishopsPlusQueensB(void) {for (uint64_t pieces = BOARD->black[2] | BOARD->black[4]; pieces; pieces = ClearBit(pieces)) {const int sq = Ctz(pieces); AddMovesB(sq, BishopMagicMoves(sq, MGEN_BOTH) & MGEN_GOOD);}}
static void MgenRooksPlusQueensW(void) {for (uint64_t pieces = BOARD->white[3] | BOARD->white[4]; pieces; pieces = ClearBit(pieces)) {const int sq = Ctz(pieces); AddMovesW(sq, RookMagicMoves(sq, MGEN_BOTH) & MGEN_GOOD);}}
static void MgenRooksPlusQueensB(void) {for (uint64_t pieces = BOARD->black[3] | BOARD->black[4]; pieces; pieces = ClearBit(pieces)) {const int sq = Ctz(pieces); AddMovesB(sq, RookMagicMoves(sq, MGEN_BOTH) & MGEN_GOOD);}}
static void MgenKingW(void) {const int sq = Ctz(BOARD->white[5]); AddMovesW(sq, KING_MOVES[sq] & MGEN_GOOD);}
static void MgenKingB(void) {const int sq = Ctz(BOARD->black[5]); AddMovesB(sq, KING_MOVES[sq] & MGEN_GOOD);}

static void MgenAllW(void) {
  MgenSetupW();
  MGEN_GOOD = ~MGEN_WHITE;
  MgenPawnsW();
  MgenKnightsW();
  MgenBishopsPlusQueensW();
  MgenRooksPlusQueensW();
  MgenKingW();
  MgenCastlingMovesW();
}

static void MgenAllB(void) {
  MgenSetupB();
  MGEN_GOOD = ~MGEN_BLACK;
  MgenPawnsB();
  MgenKnightsB();
  MgenBishopsPlusQueensB();
  MgenRooksPlusQueensB();
  MgenKingB();
  MgenCastlingMovesB();
}

static void MgenAllCapturesW(void) {
  MgenSetupW();
  MGEN_GOOD = MGEN_BLACK;
  MgenPawnsOnlyCapturesW();
  MgenKnightsW();
  MgenBishopsPlusQueensW();
  MgenRooksPlusQueensW();
  MgenKingW();
}

static void MgenAllCapturesB(void) {
  MgenSetupB();
  MGEN_GOOD = MGEN_WHITE;
  MgenPawnsOnlyCapturesB();
  MgenKnightsB();
  MgenBishopsPlusQueensB();
  MgenRooksPlusQueensB();
  MgenKingB();
}

static int MgenW(struct BOARD_T *moves) {
  MGEN_MOVES_N   = 0;
  MGEN_MOVES     = moves;
  BOARD_ORIGINAL = BOARD;
  MgenAllW();
  return MGEN_MOVES_N;
}

static int MgenB(struct BOARD_T *moves) {
  MGEN_MOVES_N   = 0;
  MGEN_MOVES     = moves;
  BOARD_ORIGINAL = BOARD;
  MgenAllB();
  return MGEN_MOVES_N;
}

static int MgenCapturesW(struct BOARD_T *moves) {
  MGEN_MOVES_N   = 0;
  MGEN_MOVES     = moves;
  BOARD_ORIGINAL = BOARD;
  MgenAllCapturesW();
  return MGEN_MOVES_N;
}

static int MgenCapturesB(struct BOARD_T *moves) {
  MGEN_MOVES_N   = 0;
  MGEN_MOVES     = moves;
  BOARD_ORIGINAL = BOARD;
  MgenAllCapturesB();
  return MGEN_MOVES_N;
}

static int MgenTacticalW(struct BOARD_T *moves) {return ChecksB() ? MgenW(moves) : MgenCapturesW(moves);}
static int MgenTacticalB(struct BOARD_T *moves) {return ChecksW() ? MgenB(moves) : MgenCapturesB(moves);}
static void MgenRoot(void) {ROOT_MOVES_N = WTM ? MgenW(ROOT_MOVES) : MgenB(ROOT_MOVES);}

static void MgenRootAll(void) {
  MgenRoot();
  EvaluateRootMoves();
  SortAll();
}

// Evaluation

static int EvalClose(const int sq_a, const int sq_b) {const int ret = 7 - Max(Abs(Xcoord(sq_a) - Xcoord(sq_b)), Abs(Ycoord(sq_a) - Ycoord(sq_b))); return ret * ret;}
static void MixScoreW(const int mg, const int eg) {EVAL_POS_MG += mg; EVAL_POS_EG += eg;}
static void MixScoreB(const int mg, const int eg) {EVAL_POS_MG -= mg; EVAL_POS_EG -= eg;}
static void ScoreW(const int score, const int mg, const int eg) {MixScoreW(mg * score, eg * score);}
static void ScoreB(const int score, const int mg, const int eg) {MixScoreB(mg * score, eg * score);}
static void MaterialW(const int piece) {EVAL_MAT_MG += EVAL_PIECE_VALUE_MG[piece]; EVAL_MAT_EG += EVAL_PIECE_VALUE_EG[piece];}
static void MaterialB(const int piece) {EVAL_MAT_MG -= EVAL_PIECE_VALUE_MG[piece]; EVAL_MAT_EG -= EVAL_PIECE_VALUE_EG[piece];}
static void PsqtW(const int piece, const int index) {MixScoreW(EVAL_PSQT_MG[piece][index], EVAL_PSQT_EG[piece][index]);}
static void PsqtB(const int piece, const int index) {MixScoreB(EVAL_PSQT_MG_B[piece][index], EVAL_PSQT_EG_B[piece][index]);}
static void MobilityW(const uint64_t moves, const int mg, const int eg) {ScoreW(PopCount(moves & (~EVAL_WHITE)), mg, eg);}
static void MobilityB(const uint64_t moves, const int mg, const int eg) {ScoreB(PopCount(moves & (~EVAL_BLACK)), mg, eg);}
static void EvalBonusChecks(void) {if (ChecksB()) MixScoreB(350, 80); else if (ChecksW()) MixScoreW(350, 80);}
static void AttacksW(const int me, uint64_t moves, const int mg, const int eg) {
  int score = 0;
  for (moves &= EVAL_BLACK; moves; moves = ClearBit(moves)) score += EVAL_ATTACKS[me][Max(0, -BOARD->board[Ctz(moves)] - 1)];
  ScoreW(score, mg, eg);
}
static void AttacksB(const int me, uint64_t moves, const int mg, const int eg) {
  int score = 0;
  for (moves &= EVAL_WHITE; moves; moves = ClearBit(moves)) score += EVAL_ATTACKS[me][Max(0, BOARD->board[Ctz(moves)] - 1)];
  ScoreB(score, mg, eg);
}

static void EvalPawnsW(const int sq) {
  MaterialW(0);
  PsqtW(0, sq);
  AttacksW(0, PAWN_CHECKS_W[sq], 2, 1);
  ScoreW(PopCount(0xFFFFFFFFULL & EVAL_COLUMNS_UP[sq] & BOARD->white[0]), -35, -55);
  if (!(EVAL_FREE_COLUMNS[Xcoord(sq)] & BOARD->white[0])) MixScoreW(-55, 0);
  if (PAWN_CHECKS_W[sq] & (BOARD->white[0] | BOARD->white[1] | BOARD->white[2])) MixScoreW(55, 15);
  if (!(EVAL_COLUMNS_UP[sq] & (BOARD->black[0] | BOARD->white[0]))) ScoreW(Ycoord(sq), 23, 57);
}

static void EvalPawnsB(const int sq) {
  MaterialB(0);
  PsqtB(0, sq);
  AttacksB(0, PAWN_CHECKS_B[sq], 2, 1);
  ScoreB(PopCount(0xFFFFFFFF00000000ULL & EVAL_COLUMNS_DOWN[sq] & BOARD->black[0]), -35, -55);
  if (!(EVAL_FREE_COLUMNS[Xcoord(sq)] & BOARD->black[0])) MixScoreB(-55, 0);
  if (PAWN_CHECKS_B[sq] & (BOARD->black[0] | BOARD->black[1] | BOARD->black[2])) MixScoreB(55, 15);
  if (!(EVAL_COLUMNS_DOWN[sq] & (BOARD->white[0] | BOARD->black[0]))) ScoreB(7 - Ycoord(sq), 23, 57);
}

static void EvalKnightsW(const int sq) {
  MaterialW(1);
  PsqtW(1, sq);
  MobilityW(KNIGHT_MOVES[sq], 22, 18);
  AttacksW(1, KNIGHT_MOVES[sq] | Bit(sq), 2, 1);
}

static void EvalKnightsB(const int sq) {
  MaterialB(1);
  PsqtB(1, sq);
  MobilityB(KNIGHT_MOVES[sq], 22, 18);
  AttacksB(1, KNIGHT_MOVES[sq] | Bit(sq), 2, 1);
}

static void BonusBishopAndPawnsEg(const int me, const int bonus, const uint64_t own_pawns, const uint64_t enemy_pawns) {
  if (Bit(me) & 0x55AA55AA55AA55AAULL) EVAL_POS_EG += bonus * PopCount(0x55AA55AA55AA55AAULL & own_pawns) + 2 * bonus * PopCount(0x55AA55AA55AA55AAULL & enemy_pawns);
  else                                 EVAL_POS_EG += bonus * PopCount(0xAA55AA55AA55AA55ULL & own_pawns) + 2 * bonus * PopCount(0xAA55AA55AA55AA55ULL & enemy_pawns);
}

static void EvalBishopsW(const int sq) {
  MaterialW(2);
  PsqtW(2, sq);
  MobilityW(BishopMagicMoves(sq, EVAL_BOTH), 29, 21);
  AttacksW(2, BISHOP_MOVES[sq] | Bit(sq), 5, 1);
  BonusBishopAndPawnsEg(sq, +30, BOARD->white[0], BOARD->black[0]);
}

static void EvalBishopsB(const int sq) {
  MaterialB(2);
  PsqtB(2, sq);
  MobilityB(BishopMagicMoves(sq, EVAL_BOTH), 29, 21);
  AttacksB(2, BISHOP_MOVES[sq] | Bit(sq), 5, 1);
  BonusBishopAndPawnsEg(sq, -30, BOARD->black[0], BOARD->white[0]);
}

static void EvalRooksW(const int sq) {
  MaterialW(3);
  PsqtW(3, sq);
  MobilityW(RookMagicMoves(sq, EVAL_BOTH), 21, 17);
  AttacksW(3, ROOK_MOVES[sq] | Bit(sq), 3, 2);
  EVAL_POS_MG += 5 * PopCount(EVAL_COLUMNS_UP[sq] & EVAL_EMPTY);
  if (EVAL_COLUMNS_UP[sq] & (BOARD->white[3] | (BOARD->white[0] & 0xFFFFFFFF00000000ULL))) EVAL_POS_MG += 50;
  if (EVAL_COLUMNS_DOWN[sq] & BOARD->black[0] & 0x00000000FFFFFFFFULL) EVAL_POS_EG += 30;
}

static void EvalRooksB(const int sq) {
  MaterialB(3);
  PsqtB(3, sq);
  MobilityB(RookMagicMoves(sq, EVAL_BOTH), 21, 17);
  AttacksB(3, ROOK_MOVES[sq] | Bit(sq), 3, 2);
  EVAL_POS_MG -= 5 * PopCount(EVAL_COLUMNS_DOWN[sq] & EVAL_EMPTY);
  if (EVAL_COLUMNS_DOWN[sq] & (BOARD->black[3] | (BOARD->black[0] & 0x00000000FFFFFFFFULL))) EVAL_POS_MG -= 50;
  if (EVAL_COLUMNS_UP[sq] & BOARD->white[0] & 0xFFFFFFFF00000000ULL) EVAL_POS_EG -= 30;
}

static void EvalQueensW(const int sq) {
  MaterialW(4);
  PsqtW(4, sq);
  MobilityW(BishopMagicMoves(sq, EVAL_BOTH) | RookMagicMoves(sq, EVAL_BOTH), 7, 21);
  AttacksW(4, QUEEN_MOVES[sq] | Bit(sq), 1, 3);
}

static void EvalQueensB(const int sq) {
  MaterialB(4);
  PsqtB(4, sq);
  MobilityB(BishopMagicMoves(sq, EVAL_BOTH) | RookMagicMoves(sq, EVAL_BOTH), 7, 21);
  AttacksB(4, QUEEN_MOVES[sq] | Bit(sq), 1, 3);
}

static void BonusKingShield(const int sq, const int color, const bool own_shield) {
  if (own_shield) EVAL_POS_MG += 42 * color;
  if (BOARD->board[sq + 8 * color] == 1 * color) EVAL_POS_MG += 100 * color;
  if (BOARD->board[sq + 8 * color] == 3 * color) EVAL_POS_MG += 50 * color;
}

static void EvalKingsW(const int sq) {
  PsqtW(5, sq);
  MobilityW(KING_MOVES[sq], 7, 35);
  AttacksW(5, KING_MOVES[sq] | Bit(sq), 0, 5);
  ScoreW(PopCount(EVAL_KING_RING[sq] & EVAL_BLACK), -200, 5);
  ScoreW((KOTH960 ? 10 : 1) * EVAL_CENTER[sq], 1, 17);
  if (KING_MOVES[sq] & (EVAL_EMPTY & 0x00FFFFFFFFFFFF00ULL)) MixScoreW(112, 25);
  if (EVAL_BOTH_N < 10) return;
  switch (sq) {
  case 1: BonusKingShield(1, 1, (Bit(8)  | Bit(9)  | Bit(10)) & EVAL_WHITE); break;  // B1
  case 2: BonusKingShield(2, 1, (Bit(9)  | Bit(10) | Bit(11)) & EVAL_WHITE); break;  // C1
  case 6: BonusKingShield(6, 1, (Bit(13) | Bit(14) | Bit(15)) & EVAL_WHITE); break;} // G1
}

static void EvalKingsB(const int sq) {
  PsqtB(5, sq);
  MobilityB(KING_MOVES[sq], 7, 35);
  AttacksB(5, KING_MOVES[sq] | Bit(sq), 0, 5);
  ScoreB(PopCount(EVAL_KING_RING[sq] & EVAL_WHITE), -200, 5);
  ScoreB((KOTH960 ? 10 : 1) * EVAL_CENTER[sq], 1, 17);
  if (KING_MOVES[sq] & (EVAL_EMPTY & 0x00FFFFFFFFFFFF00ULL)) MixScoreB(112, 25);
  if (EVAL_BOTH_N < 10) return;
  switch (sq) {
  case 57: BonusKingShield(57, -1, (Bit(48) | Bit(49) | Bit(50)) & EVAL_BLACK); break;  // B8
  case 58: BonusKingShield(58, -1, (Bit(49) | Bit(50) | Bit(51)) & EVAL_BLACK); break;  // C8
  case 62: BonusKingShield(62, -1, (Bit(53) | Bit(54) | Bit(55)) & EVAL_BLACK); break;} // G8
}

static void MatingW(void) {
  ScoreW(-EVAL_CENTER[EVAL_BLACK_KING_SQ], 5, 5);
  ScoreW(EvalClose(EVAL_WHITE_KING_SQ, EVAL_BLACK_KING_SQ), 17, 17);
}

static void MatingB(void) {
  ScoreB(-EVAL_CENTER[EVAL_WHITE_KING_SQ], 5, 5);
  ScoreB(EvalClose(EVAL_BLACK_KING_SQ, EVAL_WHITE_KING_SQ), 17, 17);
}

static void EvalSetup(void) {
  EVAL_POS_MG = EVAL_POS_EG = EVAL_MAT_MG = EVAL_MAT_EG = 0;
  EVAL_DRAWISH_FACTOR = 1.0f;
  EVAL_WHITE_KING_SQ  = Ctz(BOARD->white[5]);
  EVAL_BLACK_KING_SQ  = Ctz(BOARD->black[5]);
  EVAL_WHITE          = White();
  EVAL_BLACK          = Black();
  EVAL_BOTH           = EVAL_WHITE | EVAL_BLACK;
  EVAL_EMPTY          = ~EVAL_BOTH;
  EVAL_BOTH_N         = PopCount(EVAL_BOTH);
}

static float EvalCalculateScale(const bool wtm) {
  float scale = (EVAL_BOTH_N - 2.0f) * (1.0f / ((2.0f * 16.0f) - 2.0f));
  scale = 0.5f * (1.0f + (scale > 1.0f ? 1.0f : scale));
  if (wtm) {return scale * scale * (BOARD->black[4] ? 1.0f : 0.9f);} else {return scale * scale * (BOARD->white[4] ? 1.0f : 0.9f);} // Endgameish
}

static void EvalBonusPair(const int piece, const int mg, const int eg) {
  if (PopCount(BOARD->white[piece]) >= 2) MixScoreW(mg, eg);
  if (PopCount(BOARD->black[piece]) >= 2) MixScoreB(mg, eg);
}

static void EvalEndgame(void) {
  if (EVAL_BOTH_N > 6) return;
  if (PopCount(EVAL_BLACK) == 1) MatingW(); else if (PopCount(EVAL_WHITE) == 1) MatingB(); else if (!(BOARD->white[0] | BOARD->black[0])) EVAL_DRAWISH_FACTOR = 0.95f;
}

static void EvalPieces(void) {
  for (uint64_t both = EVAL_BOTH; both; both = ClearBit(both)) {
    const int sq = Ctz(both);
    switch (BOARD->board[sq]) {
    case +1: EvalPawnsW(sq); break;
    case +2: EvalKnightsW(sq); break;
    case +3: EvalBishopsW(sq); break;
    case +4: EvalRooksW(sq); break;
    case +5: EvalQueensW(sq); break;
    case +6: EvalKingsW(sq); break;
    case -1: EvalPawnsB(sq);  break;
    case -2: EvalKnightsB(sq); break;
    case -3: EvalBishopsB(sq); break;
    case -4: EvalRooksB(sq); break;
    case -5: EvalQueensB(sq); break;
    case -6: EvalKingsB(sq); break;}
  }
}

static int EvalCalculateScore(const bool wtm) {
  const float scale = EvalCalculateScale(wtm);
  return (int) ((scale         * (0.82f * (float) (EVAL_POS_MG + EVAL_MAT_MG)))   // Middlegame
             + ((1.0f - scale) * (0.82f * (float) (EVAL_POS_EG + EVAL_MAT_EG)))); // Endgame
}

static int EvalAll(const bool wtm) {
  EvalSetup();
  EvalPieces();
  EvalEndgame();
  EvalBonusPair(1,  95,  70); // N
  EvalBonusPair(2, 300, 500); // B
  EvalBonusPair(3,  50, 200); // R
  EvalBonusChecks();
  return EvalCalculateScore(wtm);
}

static uint64_t DrawKey(const int n_knights_w, const int n_bishops_w, const int n_knights_b, const int n_bishops_b) {
  return ZOBRIST_BOARD[0][n_knights_w] ^ ZOBRIST_BOARD[1][n_bishops_w] ^ ZOBRIST_BOARD[2][n_knights_b] ^ ZOBRIST_BOARD[3][n_bishops_b];
}

static bool DrawMaterial(void) {
  if ((BOARD->white[0] | BOARD->black[0] | BOARD->white[3] | BOARD->black[3] | BOARD->white[4] | BOARD->black[4]) || KOTH960) return 0;
  if (!(BOARD->white[1] |  BOARD->white[2] | BOARD->black[1] | BOARD->black[2])) return 1;
  const uint64_t hash = DrawKey(PopCount(BOARD->white[1]), PopCount(BOARD->white[2]), PopCount(BOARD->black[1]), PopCount(BOARD->black[2])); // Draw detection by N+B
  for (int i = 0; i < 13; i++) {if (hash == DRAWS[i]) return 1;}
  return 0;
}

static int Eval(const bool wtm) {
  if (KOTH960) {if (BOARD->black[5] & KOTHMIDDLE) return -INF / 4; if (BOARD->white[5] & KOTHMIDDLE) return +INF / 4;}
  const uint64_t hash = Hash(wtm);
  struct HASH_T *entry = &HASH[(uint32_t) (hash & HASH_KEY)];
  if (entry->eval_hash == hash) return entry->score;
  const int noise = LEVEL == 100 ? 0 : 10 * Random(LEVEL - 100, 100 - LEVEL);
  entry->eval_hash = hash;
  entry->score = DrawMaterial() ? 0 : ((int) (EVAL_DRAWISH_FACTOR * EvalAll(wtm)) + (wtm ? +20 : -20));
  return (int) ((1.0f - (((float) BOARD->rule50) / 100.0f)) * (float) entry->score) + noise;
}

// Search

static bool Draw(void) {
  if (BOARD->rule50 >= 100) return 1;
  const uint64_t hash = REPETITION_POSITIONS[BOARD->rule50];
  for (int i = BOARD->rule50 - 2, reps = 0; i >= 0; i -= 2) {if (REPETITION_POSITIONS[i] == hash && ++reps >= 2) return 1;}
  return 0;
}

static void Speak(const int score, const uint64_t search_time) {
  Print("info depth %i nodes %llu time %llu nps %llu score cp %i pv %s", Min(MAX_DEPTH, DEPTH + 1), NODES, search_time, Nps(NODES, search_time),
       (WTM ? 1 : -1) * (int) ((Abs(score) >= INF ? 0.01f : 0.1f) * score), MoveName(&ROOT_MOVES[0]));
}

#if defined WINDOWS
#include <conio.h>
static bool InputAvailable(void) {return _kbhit();}
#else
static bool InputAvailable(void) {
  fd_set fd;
  struct timeval tv;
  FD_ZERO(&fd);
  FD_SET(STDIN_FILENO, &fd);
  tv.tv_sec = tv.tv_usec = 0;
  select(STDIN_FILENO + 1, &fd, 0, 0, &tv);
  return FD_ISSET(STDIN_FILENO, &fd) > 0;
}
#endif

static bool UserStop(void) {
  if (!ANALYZING || !InputAvailable()) return 0;
  Input();
  if (Token("stop")) return 1;
  return 0;
}

static bool TimeCheckSearch(void) {
  static uint64_t ticks = 0;
  if (++ticks & 0xFFULL) return 0;
  if ((Now() >= STOP_SEARCH_TIME) || UserStop()) STOP_SEARCH = 1;
  return STOP_SEARCH;
}

static int QSearchW(int alpha, const int beta, const int depth) {
  NODES++;
  if (STOP_SEARCH || TimeCheckSearch()) return 0;
  alpha = Max(alpha, Eval(1));
  if (depth <= 0 || alpha >= beta) return alpha;
  struct BOARD_T moves[64];
  const int moves_n = MgenTacticalW(moves);
  SortAll();
  for (int i = 0; i < moves_n; i++) {BOARD = moves + i; if ((alpha = Max(alpha, QSearchB(alpha, beta, depth - 1))) >= beta) return alpha;}
  return alpha;
}

static int QSearchB(const int alpha, int beta, const int depth) {
  NODES++;
  if (STOP_SEARCH) return 0;
  beta = Min(beta, Eval(0));
  if (depth <= 0 || alpha >= beta) return beta;
  struct BOARD_T moves[64];
  const int moves_n = MgenTacticalB(moves);
  SortAll();
  for (int i = 0; i < moves_n; i++) {BOARD = moves + i; if (alpha >= (beta = Min(beta, QSearchW(alpha, beta, depth - 1)))) return beta;}
  return beta;
}

static void UpdateSort(struct HASH_T *entry, const enum MOVE_T type, const uint64_t hash, const uint8_t index) {
  entry->sort_hash = hash;
  switch (type) {case KILLER: entry->killer = index + 1; break; case GOOD: entry->good = index + 1; break; case QUIET: entry->quiet = index + 1; break;}
}

static int SearchMovesW(int alpha, const int beta, int depth, const int ply) {
  const uint64_t hash = REPETITION_POSITIONS[BOARD->rule50];
  struct BOARD_T moves[MAX_MOVES];
  const bool checks = ChecksB();
  const int moves_n = MgenW(moves);
  if (!moves_n) return checks ? -INF : 0; else if (moves_n == 1 || (ply < 5 && checks)) depth++;
  bool ok_lmr = moves_n >= 5 && depth >= 2 && !checks;
  struct HASH_T *entry = &HASH[(uint32_t) (hash & HASH_KEY)];
  SortByHash(entry, hash);
  for (int i = 0; i < moves_n; i++) {
    BOARD = moves + i;
    if (ok_lmr && i >= 2 && !BOARD->score && !ChecksW()) {if (SearchB(alpha, beta, depth - 2 - Min(1, i / 23), ply + 1) <= alpha) continue; BOARD = moves + i;} // LMR
    const int score = SearchB(alpha, beta, depth - 1, ply + 1);
    if (score > alpha) {
      alpha  = score;
      ok_lmr = 0;
      if (alpha >= beta) {UpdateSort(entry, KILLER, hash, moves[i].index); return alpha;}
      UpdateSort(entry, moves[i].score ? GOOD : QUIET, hash, moves[i].index);
    }
  }
  return alpha;
}

static int SearchW(int alpha, const int beta, const int depth, const int ply) {
  NODES++;
  if (STOP_SEARCH || TimeCheckSearch()) return 0;
  if (KOTH960) {if (BOARD->black[5] & KOTHMIDDLE) return -INF; if (BOARD->white[5] & KOTHMIDDLE) return +INF;}
  if (depth <= 0 || ply >= DEPTH_LIMIT) return QSearchW(alpha, beta, QS_DEPTH);
  const uint8_t rule50 = BOARD->rule50;
  const uint64_t tmp = REPETITION_POSITIONS[rule50];
  REPETITION_POSITIONS[rule50] = Hash(1);
  alpha = Draw() ? 0 : SearchMovesW(alpha, beta, depth, ply);
  REPETITION_POSITIONS[rule50] = tmp;
  return alpha;
}

static int SearchMovesB(const int alpha, int beta, int depth, const int ply) {
  const uint64_t hash = REPETITION_POSITIONS[BOARD->rule50];
  struct BOARD_T moves[MAX_MOVES];
  const bool checks = ChecksW();
  const int moves_n = MgenB(moves);
  if (!moves_n) return checks ? INF : 0; else if (moves_n == 1 || (ply < 5 && checks)) depth++;
  bool ok_lmr = moves_n >= 5 && depth >= 2 && !checks;
  struct HASH_T *entry = &HASH[(uint32_t) (hash & HASH_KEY)];
  SortByHash(entry, hash);
  for (int i = 0; i < moves_n; i++) {
    BOARD = moves + i;
    if (ok_lmr && i >= 2 && !BOARD->score && !ChecksB()) {if (SearchW(alpha, beta, depth - 2 - Min(1, i / 23), ply + 1) >= beta) continue; BOARD = moves + i;}
    const int score = SearchW(alpha, beta, depth - 1, ply + 1);
    if (score < beta) {
      beta   = score;
      ok_lmr = 0;
      if (alpha >= beta) {UpdateSort(entry, KILLER, hash, moves[i].index); return beta;}
      UpdateSort(entry, moves[i].score ? GOOD : QUIET, hash, moves[i].index);
    }
  }
  return beta;
}

static int SearchB(const int alpha, int beta, const int depth, const int ply) {
  NODES++;
  if (STOP_SEARCH) return 0;
  if (KOTH960) {if (BOARD->black[5] & KOTHMIDDLE) return -INF; if (BOARD->white[5] & KOTHMIDDLE) return +INF;}
  if (depth <= 0 || ply >= DEPTH_LIMIT) return QSearchB(alpha, beta, QS_DEPTH);
  const uint8_t rule50 = BOARD->rule50;
  const uint64_t tmp = REPETITION_POSITIONS[rule50];
  REPETITION_POSITIONS[rule50] = Hash(0);
  beta = Draw() ? 0 : SearchMovesB(alpha, beta, depth, ply);
  REPETITION_POSITIONS[rule50] = tmp;
  return beta;
}

static int BestW(void) {
  int score = 0, best_i = 0, alpha = -INF;
  for (int i = 0; i < ROOT_MOVES_N; i++) {
    BOARD = ROOT_MOVES + i;
    // First stable score for alpha ! -> Smaller window search ! -> Unstable ? -> Research !
    if (DEPTH >= 1 && i >= 1) {
      if ((score = SearchB(alpha, alpha + 1, DEPTH, 0)) > alpha) {BOARD = ROOT_MOVES + i; score = SearchB(alpha, INF, DEPTH, 0);}
    } else {
      score = SearchB(alpha, INF, DEPTH, 0);
    }
    if (STOP_SEARCH) return BEST_SCORE;
    if (score > alpha) {alpha = score; best_i = i;}
  }
  SortRoot(best_i);
  return alpha;
}

static int BestB(void) {
  int score = 0, best_i = 0, beta = INF;
  for (int i = 0; i < ROOT_MOVES_N; i++) {
    BOARD = ROOT_MOVES + i;
    if (DEPTH >= 1 && i >= 1) {
      if ((score = SearchW(beta - 1, beta, DEPTH, 0)) < beta) {BOARD = ROOT_MOVES + i; score = SearchW(-INF, beta, DEPTH, 0);}
    } else {
      score = SearchW(-INF, beta, DEPTH, 0);
    }
    if (STOP_SEARCH) return BEST_SCORE;
    if (score < beta) {beta = score; best_i = i;}
  }
  SortRoot(best_i);
  return beta;
}

static void ThinkSetup(const int think_time) {
  STOP_SEARCH = 0;
  BEST_SCORE = NODES = DEPTH = 0;
  QS_DEPTH = 4;
  STOP_SEARCH_TIME = Now() + (uint64_t) Max(0, think_time);
}

static void RandomMove(void) {
  if (!ROOT_MOVES_N) return;
  const int root_i = Random(0, ROOT_MOVES_N - 1);
  if (root_i) Swap(ROOT_MOVES, ROOT_MOVES + root_i);
}

static bool ThinkRandomMove(void) {
  if (LEVEL) return 0;
  RandomMove();
  return 1;
}

static void Think(const int think_time) {
  struct BOARD_T *tmp = BOARD;
  const uint64_t start = Now();
  ThinkSetup(think_time);
  MgenRootAll();
  if (ROOT_MOVES_N <= 1 || ThinkRandomMove()) {Speak(0, 0); return;}
  UNDERPROMOS = 0;
  for (; Abs(BEST_SCORE) < INF / 2 && DEPTH < MAX_DEPTH && !STOP_SEARCH; DEPTH++) {
    BEST_SCORE = WTM ? BestW() : BestB();
    Speak(BEST_SCORE, Now() - start);
    QS_DEPTH = Min(QS_DEPTH + 2, 12);
  }
  UNDERPROMOS = 1;
  BOARD = tmp;
  Speak(BEST_SCORE, Now() - start);
}

// UCI

static void MakeMove(const int root_i) {
  REPETITION_POSITIONS[BOARD->rule50] = Hash(WTM);
  BOARD_TMP = ROOT_MOVES[root_i];
  BOARD = &BOARD_TMP;
  WTM = !WTM;
}

static void UciMove(void) {
  const char *move = TokenCurrent();
  MgenRoot();
  for (int i = 0; i < ROOT_MOVES_N; i++) {if (!strcmp(MoveName(ROOT_MOVES + i), move)) {MakeMove(i); return;}}
  Assert(0, "Error #4: Bad move !");
}

static void UciFen(void) {
  if (Token("startpos")) return;
  TokenPop(1);
  for (FEN[0] = '\0'; TokenOk() && !TokenIs("moves"); TokenPop(1)) {StringJoin(FEN, TokenCurrent()); StringJoin(FEN, " ");}
  Fen(FEN);
}

static void UciMoves(void) {while (TokenOk()) {UciMove(); TokenPop(1);}}

static void UciPosition(void) {
  Fen(STARTPOS);
  UciFen();
  if (Token("moves")) UciMoves();
}

static void UciSetoption(void) {
  if (     TokenPeek("name", 0) && TokenPeek("UCI_Chess960", 1)      && TokenPeek("value", 2)) {CHESS960 = TokenPeek("true", 3); TokenPop(4);}
  else if (TokenPeek("name", 0) && TokenPeek("UCI_Kingofthehill", 1) && TokenPeek("value", 2)) {KOTH960 = TokenPeek("true", 3); TokenPop(4);}
  else if (TokenPeek("name", 0) && TokenPeek("Level", 1)             && TokenPeek("value", 2)) {TokenPop(3); LEVEL = Between(0, TokenNumber(), 100); TokenPop(1);}
  else if (TokenPeek("name", 0) && TokenPeek("MoveOverhead", 1)      && TokenPeek("value", 2)) {TokenPop(3); MOVEOVERHEAD = Between(0, TokenNumber(), 5000); TokenPop(1);}
}

static void PrintBestMove(void) {Print("bestmove %s", ROOT_MOVES_N <= 0 ? "0000" : MoveName(&ROOT_MOVES[0]));}

static void UciGo(void) {
  int wtime = 0, btime = 0, winc  = 0, binc = 0, mtg = 30;
  for (; TokenOk(); TokenPop(1)) {
    if (     Token("infinite"))  {ANALYZING = 1; Think(INF); ANALYZING = 0; PrintBestMove(); return;}
    else if (Token("wtime"))     {wtime = Max(0, TokenNumber() - MOVEOVERHEAD);}
    else if (Token("btime"))     {btime = Max(0, TokenNumber() - MOVEOVERHEAD);}
    else if (Token("winc"))      {winc = TokenNumber(); winc = Max(0, winc ? winc - MOVEOVERHEAD : winc);}
    else if (Token("binc"))      {binc = TokenNumber(); binc = Max(0, binc ? binc - MOVEOVERHEAD : binc);}
    else if (Token("movestogo")) {mtg = Between(1, TokenNumber(), 30);}
    else if (Token("movetime"))  {Think(TokenNumber()); TokenPop(1); PrintBestMove(); return;}
    else if (Token("depth"))     {MAX_DEPTH = Between(1, TokenNumber(), DEPTH_LIMIT); Think(INF); MAX_DEPTH = DEPTH_LIMIT; TokenPop(1); PrintBestMove(); return;}
  }
  Think(Max(0, WTM ? wtime / mtg + winc : btime / mtg + binc));
  PrintBestMove();
}

static void UciUci(void) {
  Print("id name %s", NAME);
  Print("id author Toni Helminen");
  Print("option name UCI_Chess960 type check default %s", CHESS960 ? "true" : "false");
  Print("option name UCI_Kingofthehill type check default %s", KOTH960 ? "true" : "false");
  Print("option name Level type spin default %i min 0 max 100", LEVEL);
  Print("option name MoveOverhead type spin default %i min 0 max 5000", MOVEOVERHEAD);
  Print("uciok");
}

static bool UciCommands(void) {
  if (TokenOk()) {
    if (     Token("position"))  UciPosition();
    else if (Token("go"))        UciGo();
    else if (Token("isready"))   Print("readyok");
    else if (Token("setoption")) UciSetoption();
    else if (Token("uci"))       UciUci();
    else if (Token("quit"))      return 0;
  }
  while (TokenOk()) TokenPop(1); // Ignore the rest
  return 1;
}

static bool Uci(void) {
  Input();
  return UciCommands();
}

static void UciLoop(void) {
  Print("%s by Toni Helminen", NAME);
  while (Uci());
}

// Init

static uint64_t PermutateBb(const uint64_t moves, const int index) {
  int total = 0, good[64] = {0};
  uint64_t permutations = 0;
  for (int i = 0; i < 64; i++) if (moves & Bit(i)) good[total++] = i;
  const int popn = PopCount(moves);
  for (int i = 0; i < popn; i++) if ((1 << i) & index) permutations |= Bit(good[i]);
  return permutations & moves;
}

static uint64_t MakeSliderMagicMoves(const int *slider_vectors, const int square, const uint64_t moves) {
  uint64_t possible_moves = 0;
  const int x_square = Xcoord(square), y_square = Ycoord(square);
  for (int i = 0; i < 4; i++)
    for (int j = 1; j < 8; j++) {
      const int x = x_square + j * slider_vectors[2 * i], y = y_square + j * slider_vectors[2 * i + 1];
      if (!OnBoard(x, y)) break;
      const uint64_t tmp = Bit(8 * y + x);
      possible_moves    |= tmp;
      if (tmp & moves) break;
    }
  return possible_moves & (~Bit(square));
}

static void InitBishopMagics(void) {
  for (int i = 0; i < 64; i++) {
    const uint64_t magics = BISHOP_MOVE_MAGICS[i] & (~Bit(i));
    for (int j = 0; j < 512; j++) {
      const uint64_t allmoves = PermutateBb(magics, j);
      BISHOP_MAGIC_MOVES[i][BishopMagicIndex(i, allmoves)] = MakeSliderMagicMoves(BISHOP_VECTORS, i, allmoves);
    }
  }
}

static void InitRookMagics(void) {
  for (int i = 0; i < 64; i++) {
    const uint64_t magics = ROOK_MOVE_MAGICS[i] & (~Bit(i));
    for (int j = 0; j < 4096; j++) {
      const uint64_t allmoves = PermutateBb(magics, j);
      ROOK_MAGIC_MOVES[i][RookMagicIndex(i, allmoves)] = MakeSliderMagicMoves(ROOK_VECTORS, i, allmoves);
    }
  }
}

static uint64_t MakeSliderMoves(const int square, const int *slider_vectors) {
  uint64_t moves = 0;
  const int x_square = Xcoord(square), y_square = Ycoord(square);
  for (int i = 0; i < 4; i++) {
    const int dx = slider_vectors[2 * i], dy = slider_vectors[2 * i + 1];
    uint64_t tmp = 0;
    for (int j = 1; j < 8; j++) {
      const int x = x_square + j * dx, y = y_square + j * dy;
      if (!OnBoard(x, y)) break;
      tmp |= Bit(8 * y + x);
    }
    moves |= tmp;
  }
  return moves;
}

static void InitSliderMoves(void) {
  for (int i = 0; i < 64; i++) {
    ROOK_MOVES[i]   = MakeSliderMoves(i, ROOK_VECTORS);
    BISHOP_MOVES[i] = MakeSliderMoves(i, BISHOP_VECTORS);
    QUEEN_MOVES[i]  = ROOK_MOVES[i] | BISHOP_MOVES[i];
  }
}

static uint64_t MakeJumpMoves(const int square, const int len, const int dy, const int *jump_vectors) {
  uint64_t moves = 0;
  const int x_square = Xcoord(square), y_square = Ycoord(square);
  for (int i = 0; i < len; i++) {
    const int x = x_square + jump_vectors[2 * i], y = y_square + dy * jump_vectors[2 * i + 1];
    if (OnBoard(x, y)) moves |= Bit(8 * y + x);
  }
  return moves;
}

static void InitJumpMoves(void) {
  const int pawn_check_vectors[2 * 2] = {-1,1,1,1}, pawn_1_vectors[1 * 2] = {0,1};
  for (int i = 0; i < 64; i++) {
    KING_MOVES[i]     = MakeJumpMoves(i, 8, +1, KING_VECTORS);
    KNIGHT_MOVES[i]   = MakeJumpMoves(i, 8, +1, KNIGHT_VECTORS);
    PAWN_CHECKS_W[i]  = MakeJumpMoves(i, 2, +1, pawn_check_vectors);
    PAWN_CHECKS_B[i]  = MakeJumpMoves(i, 2, -1, pawn_check_vectors);
    PAWN_1_MOVES_W[i] = MakeJumpMoves(i, 1, +1, pawn_1_vectors);
    PAWN_1_MOVES_B[i] = MakeJumpMoves(i, 1, -1, pawn_1_vectors);
  }
  for (int i = 0; i < 8; i++) {
    PAWN_2_MOVES_W[ 8 + i] = MakeJumpMoves( 8 + i, 1, +1, pawn_1_vectors) | MakeJumpMoves( 8 + i, 1, +2, pawn_1_vectors);
    PAWN_2_MOVES_B[48 + i] = MakeJumpMoves(48 + i, 1, -1, pawn_1_vectors) | MakeJumpMoves(48 + i, 1, -2, pawn_1_vectors);
  }
}

static void InitEvalStuff(void) {
  for (int i = 0; i < 64; i++) {
    for (int j = 0; j < 8; j++) {
      const int x = Xcoord(i) + KING_VECTORS[2 * j], y = Ycoord(i) + KING_VECTORS[2 * j + 1];
      if (OnBoard(x, y)) EVAL_KING_RING[i] |= Bit(8 * y + x);
    }
    for (int y = i + 8; y < 64; y += 8) EVAL_COLUMNS_UP[i]   |= Bit(y);
    for (int y = i - 8; y > -1; y -= 8) EVAL_COLUMNS_DOWN[i] |= Bit(y);
  }
  for (int i = 0; i < 6; i++)
    for (int j = 0; j < 64; j++) {
      EVAL_PSQT_MG_B[i][Mirror(j)] = EVAL_PSQT_MG[i][j];
      EVAL_PSQT_EG_B[i][Mirror(j)] = EVAL_PSQT_EG[i][j];
    }
}

static void InitDraws(void) {
  const int draws[6 * 4] = {1,0,0,0, 0,1,0,0, 2,0,0,0, 1,0,0,1, 2,0,1,0, 2,0,0,1}; // KNK, KBK, KNNK, KNKB, KNNKN, KNNKB
  int len = 0;
  for (int i = 0; i < 6; i++) {
    DRAWS[len++] = DrawKey(draws[4 * i],     draws[4 * i + 1], draws[4 * i + 2], draws[4 * i + 3]);
    DRAWS[len++] = DrawKey(draws[4 * i + 2], draws[4 * i + 3], draws[4 * i],     draws[4 * i + 1]); // Vice versa
  }
  DRAWS[len++] = DrawKey(0,1,0,1); // KBKB
}

static void InitZobrist(void) {
  for (int i = 0; i < 13; i++) for (int j = 0; j < 64; j++) ZOBRIST_BOARD[i][j] = Random8x64();
  for (int i = 0; i < 64; i++) ZOBRIST_EP[i]     = Random8x64();
  for (int i = 0; i < 16; i++) ZOBRIST_CASTLE[i] = Random8x64();
  for (int i = 0; i <  2; i++) ZOBRIST_WTM[i]    = Random8x64();
}

static void Init(void) {
  RANDOM_SEED += (uint64_t) time(NULL);
  InitEvalStuff();
  InitBishopMagics();
  InitRookMagics();
  InitZobrist();
  InitDraws();
  InitSliderMoves();
  InitJumpMoves();
  Fen(STARTPOS);
}

// "Wisdom begins in wonder." -- Socrates
int main() {
  Init();
  UciLoop();
  return EXIT_SUCCESS;
}
