/*
 * Chess Engine - Benchmark (Interface)
 * ------------------------------------
 * Declares the list of benchmark FENs and the bench(depth) entry point.
 */

#pragma once

/*
 * bench_positions
 * ---------------
 * A curated set of FENs spanning opening, middlegame, and endgame.
 * Meant to exercise move generation, evaluation, and pruning behavior.
 */

inline const char* bench_positions[]={
"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -",
"r1bn1rk1/ppp1qppp/3pp3/3P4/2P1n3/2B2NP1/PP2PPBP/2RQK2R w K -",
"r2q1rk1/1bppbppp/p4n2/n2Np3/Pp2P3/1B1P1N2/1PP2PPP/R1BQ1RK1 w - -",
"rnb2rk1/1pq1bppp/p3pn2/3p4/3NPP2/2N1B3/PPP1B1PP/R3QRK1 w - -",
"2rq1rk1/p3bppp/bpn1pn2/2pp4/3P4/1P2PNP1/PBPN1PBP/R2QR1K1 w - -",
"rn3rk1/1p2ppbp/1pp3p1/3n4/3P1Bb1/2N1PN2/PP3PPP/2R1KB1R w K -",
"r1bq1rk1/3nbppp/p1p1pn2/1p4B1/3P4/2NBPN2/PP3PPP/2RQK2R w K -",
"r3kbnr/1bpq2pp/p2p1p2/1p2p3/3PP2N/1PN5/1PP2PPP/R1BQ1RK1 w kq -",
"r1b1k2r/pp1nqp1p/2p3p1/3p3n/3P4/2NBP3/PPQ2PPP/2KR2NR w kq -",
"r2q1rk1/1b2ppbp/ppnp1np1/2p5/P3P3/2PP1NP1/1P1N1PBP/R1BQR1K1 w - -",
"r2q1rk1/pp2ppbp/2n1bnp1/3p4/4PPP1/1NN1B3/PPP1B2P/R2QK2R w KQ -",
"2q1r1k1/1ppb4/r2p1Pp1/p4n1p/2P1n3/5NPP/PP3Q1K/2BRRB2 w - -",
"7r/1p2k3/2bpp3/p3np2/P1PR4/2N2PP1/1P4K1/3B4 b - -",
"4k3/p1P3p1/2q1np1p/3N4/8/1Q3PP1/6KP/8 w - -",
"2r1b1k1/R4pp1/4pb1p/1pBr4/1Pq2P2/3N4/2PQ2PP/5RK1 b - -",
"6k1/p1qb1p1p/1p3np1/2b2p2/2B5/2P3N1/PP2QPPP/4N1K1 b - -",
"1rr1nbk1/5ppp/3p4/1q1PpN2/np2P3/5Q1P/P1BB1PP1/2R1R1K1 w - -",
"3q4/pp3pkp/5npN/2bpr1B1/4r3/2P2Q2/PP3PPP/R4RK1 w - -",
"3rr1k1/pb3pp1/1p1q1b1p/1P2NQ2/3P4/P1NB4/3K1P1P/2R3R1 w - -",
"r1b1r1k1/p1p3pp/2p2n2/2bp4/5P2/3BBQPq/PPPK3P/R4N1R b - -",
"3r4/1b2k3/1pq1pp2/p3n1pr/2P5/5PPN/PP1N1QP1/R2R2K1 b - -",
"2r4k/pB4bp/6p1/6q1/1P1n4/2N5/P4PPP/2R1Q1K1 b - -",
"1N2k3/5p2/p2P2p1/3Pp3/pP3b2/5P1r/P7/1K4R1 b - -",
"2k2R2/6r1/8/B2pp2p/1p6/3P4/PP2b3/2K5 b - -",
"2k5/1pp5/2pb2p1/7p/6n1/P5N1/1PP3PP/2K1B3 b - -",
"2n5/1k6/3pNn2/3ppp2/7p/4P2P/1P4P1/5NK1 w - -",
"5nk1/B4p2/7p/6p1/3N3n/2r2PK1/5P1P/4R3 b - -",
"8/1p3pkp/p1r3p1/3P3n/3p1P2/3P4/PP3KP1/R3N3 b - -",
"8/2B2k2/p2p2pp/2pP1p2/2P2P2/2b1N1PP/P4K2/2n5 b - -",
"8/4p1kp/1n1p2p1/nPp5/b5P1/P5KP/3N1P2/4NB2 w - -",
"r1b3k1/2p4p/3p1p2/1p1P4/1P3P2/P5P1/5KNP/R7 b - -",
"1k2b3/1pp5/4r3/R3N1pp/1P3P2/p5P1/2P4P/1K6 w - -",
};
// Run the benchmark to the given search depth
int bench(int depth);
