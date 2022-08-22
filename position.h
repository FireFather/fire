/*
  Fire is a freeware UCI chess playing engine authored by Norman Schmidt.

  Fire utilizes many state-of-the-art chess programming ideas and techniques
  which have been documented in detail at https://www.chessprogramming.org/
  and demonstrated via the very strong open-source chess engine Stockfish...
  https://github.com/official-stockfish/Stockfish.

  Fire is free software: you can redistribute it and/or modify it under the
  terms of the GNU General Public License as published by the Free Software
  Foundation, either version 3 of the License, or any later version.

  You should have received a copy of the GNU General Public License with
  this program: copying.txt.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once
#include "bitboard.h"
#include "fire.h"

class position;
class thread;

struct s_move;
struct threadinfo;
struct cmhinfo;

template <int max_plus, int max_min>
struct piece_square_stats;

typedef piece_square_stats<24576, 24576> counter_move_values;

constexpr int delayed_number{ 7 };

enum ptype : uint8_t
{
	no_piece,
	w_king = 1,
	w_pawn,
	w_knight,
	w_bishop,
	w_rook,
	w_queen,
	b_king = 9,
	b_pawn,
	b_knight,
	b_bishop,
	b_rook,
	b_queen,
	num_pieces = 16
};

constexpr uint8_t piece_type(const ptype piece)
{
	return static_cast<uint8_t>(piece & 7);
}

constexpr side piece_color(const ptype piece)
{
	assert(piece != no_piece);
	return static_cast<side>(piece >> 3);
}

constexpr ptype make_piece(const side color, const uint8_t piece)
{
	return static_cast<ptype>((color << 3) + piece);
}

constexpr int material_value[num_pieces] =
{
	mat_0, mat_0, mat_0, mat_knight, mat_bishop, mat_rook, mat_queen, mat_0,
	mat_0, mat_0, mat_0, mat_knight, mat_bishop, mat_rook, mat_queen, mat_0
};

constexpr int piece_phase[num_pieces] =
{
	0, 0, 0, 1, 1, 3, 6, 0,
	0, 0, 0, 1, 1, 3, 6, 0
};

constexpr int see_value_simple[num_pieces] =
{
	see_0, see_0, see_pawn, see_knight, see_bishop, see_rook, see_queen, see_0,
	see_0, see_0, see_pawn, see_knight, see_bishop, see_rook, see_queen, see_0
};

namespace pst
{
	extern int psq[num_pieces][num_squares];
}

struct attack_info
{
	uint64_t attack[num_sides][num_piecetypes];
	uint64_t double_attack[num_sides];
	uint64_t pinned[num_sides];
	uint64_t mobility_mask[num_sides];
	int k_attack_score[num_sides];
	bool strong_threat[num_sides];
	uint64_t k_zone[num_sides];
};

struct position_info
{
	uint64_t pawn_key;
	uint64_t material_key, bishop_color_key;
	int non_pawn_material[num_sides];
	int psq;
	int position_value;
	uint8_t castle_possibilities, phase, strong_threat;
	square enpassant_square;
	uint8_t dummy[4];

	uint64_t key;
	int draw50_moves, distance_to_null_move, ply, move_number;
	uint32_t previous_move;
	ptype captured_piece, moved_piece, dummy_x;
	bool eval_is_exact;
	counter_move_values* move_counter_values;
	uint64_t in_check;
	uint64_t x_ray[num_sides];
	uint64_t check_squares[num_piecetypes];
	uint32_t* pv;
	uint32_t killers[2];
	uint32_t excluded_move;
	int stats_value;
	int eval_positional;
	uint8_t eval_factor, lmr_reduction;
	bool no_early_pruning, move_repetition;

	s_move* mp_current_move, * mp_end_list, * mp_end_bad_capture;
	stage mp_stage;
	uint32_t mp_hash_move, mp_counter_move;
	int mp_depth;
	square mp_capture_square;
	bool mp_only_quiet_check_moves, dummy_y[2];
	int mp_threshold;
	uint8_t mp_delayed_number, mp_delayed_current;
	uint16_t mp_delayed[delayed_number];

	square pin_by[num_squares];
};

static_assert(offsetof(position_info, key) == 48, "offset wrong");

class position
{
public:
	static void init();
	static void init_hash_move50(int fifty_move_distance);

	position() = default;
	position(const position&) = default;
	position& operator=(const position&) = delete;

	position& set(const std::string& fen_str, bool is_chess960, thread* th);

	[[nodiscard]] uint64_t pieces() const;
	[[nodiscard]] uint64_t pieces(uint8_t piece) const;
	[[nodiscard]] uint64_t pieces(uint8_t piece1, uint8_t piece2) const;
	[[nodiscard]] uint64_t pieces(side color) const;
	[[nodiscard]] uint64_t pieces(side color, uint8_t piece) const;
	[[nodiscard]] uint64_t pieces(side color, uint8_t piece1, uint8_t piece2) const;
	[[nodiscard]] uint64_t pieces(side color, uint8_t piece1, uint8_t piece2, uint8_t piece3) const;
	[[nodiscard]] uint64_t pieces_excluded(side color, uint8_t piece) const;
	[[nodiscard]] ptype piece_on_square(square sq) const;
	[[nodiscard]] square enpassant_square() const;
	[[nodiscard]] bool empty_square(square sq) const;
	[[nodiscard]] int number(side color, uint8_t piece) const;
	[[nodiscard]] int number(ptype piece) const;
	[[nodiscard]] const square* piece_list(side color, uint8_t piece) const;
	[[nodiscard]] square piece_square(side color, uint8_t piece) const;
	[[nodiscard]] square king(side color) const;
	[[nodiscard]] int total_num_pieces() const;

	[[nodiscard]] int castling_possible(side color) const;
	[[nodiscard]] int castling_possible(uint8_t castle) const;
	[[nodiscard]] bool castling_impossible(uint8_t castle) const;
	[[nodiscard]] square castle_rook_square(square king_square) const;

	[[nodiscard]] uint64_t is_in_check() const;
	[[nodiscard]] uint64_t discovered_check_possible() const;
	[[nodiscard]] uint64_t pinned_pieces() const;
	void calculate_check_pins() const;
	template <side color>
	void calculate_pins() const;

	[[nodiscard]] uint64_t attack_to(square sq) const;
	[[nodiscard]] uint64_t attack_to(square sq, uint64_t occupied) const;
	[[nodiscard]] uint64_t attack_from(uint8_t piece_t, square sq) const;
	template <uint8_t>
	[[nodiscard]] uint64_t attack_from(square sq) const;
	template <uint8_t>
	[[nodiscard]] uint64_t attack_from(square sq, side color) const;

	[[nodiscard]] bool legal_move(uint32_t move) const;
	[[nodiscard]] bool valid_move(uint32_t move) const;
	[[nodiscard]] bool is_capture_move(uint32_t move) const;
	[[nodiscard]] bool capture_or_promotion(uint32_t move) const;
	[[nodiscard]] bool give_check(uint32_t move) const;
	[[nodiscard]] bool advanced_pawn(uint32_t move) const;
	[[nodiscard]] bool passed_pawn_advance(uint32_t move, rank r) const;
	[[nodiscard]] ptype moved_piece(uint32_t move) const;
	[[nodiscard]] bool material_or_castle_changed() const;
	[[nodiscard]] square calculate_threat() const;

	[[nodiscard]] bool is_passed_pawn(side color, square sq) const;
	[[nodiscard]] bool different_color_bishops() const;

	void play_move(uint32_t move, bool gives_check);
	void play_move(uint32_t move);
	void take_move_back(uint32_t move);
	void play_null_move();
	void take_null_back();

	[[nodiscard]] static const int* see_values();
	[[nodiscard]] bool see_test(uint32_t move, int limit) const;

	[[nodiscard]] uint64_t key() const;
	[[nodiscard]] uint64_t key_after_move(uint32_t move) const;
	[[nodiscard]] static uint64_t exception_key(uint32_t move);
	[[nodiscard]] uint64_t material_key() const;
	[[nodiscard]] uint64_t bishop_color_key() const;
	[[nodiscard]] uint64_t pawn_key() const;
	[[nodiscard]] uint64_t draw50_key() const;

	[[nodiscard]] side on_move() const;
	[[nodiscard]] int game_phase() const;
	[[nodiscard]] int game_ply() const;
	void increase_game_ply();
	void increase_tb_hits();
	[[nodiscard]] bool is_chess960() const;
	[[nodiscard]] thread* my_thread() const;
	[[nodiscard]] threadinfo* thread_info() const;
	[[nodiscard]] cmhinfo* cmh_info() const;
	[[nodiscard]] uint64_t visited_nodes() const;
	[[nodiscard]] uint64_t tb_hits() const;
	[[nodiscard]] int fifty_move_counter() const;
	[[nodiscard]] int psq_score() const;
	[[nodiscard]] int non_pawn_material(side color) const;
	[[nodiscard]] position_info* info() const
	{
		return pos_info_;
	}
	void copy_position(const position* pos, thread* th, const position_info* copy_state);
	double epd_result;
private:
	void set_castling_possibilities(side color, square from_r);
	void set_position_info(position_info* si) const;
	void calculate_bishop_color_key() const;

	void move_piece(side color, ptype piece, square sq);
	void delete_piece(side color, ptype piece, square sq);
	void relocate_piece(side color, ptype piece, square from, square to);
	template <bool yes>
	void do_castle_move(side me, square from, square to, square& from_r, square& to_r);
	[[nodiscard]] bool is_draw() const;

	position_info* pos_info_;
	side on_move_;
	thread* this_thread_;
	threadinfo* thread_info_;
	cmhinfo* cmh_info_;
	ptype board_[num_squares];
	uint64_t piece_bb_[num_pieces];
	uint64_t color_bb_[num_sides];
	uint8_t piece_number_[num_pieces];
	square piece_list_[num_pieces][16];
	uint8_t piece_index_[num_squares];
	uint8_t castle_mask_[num_squares];
	square castle_rook_square_[num_squares];
	uint64_t castle_path_[castle_possible_n];
	uint64_t nodes_, tb_hits_;
	int game_ply_;
	bool chess960_;
	char filler_[32];
};

inline void position::move_piece(const side color, const ptype piece, const square sq)
{
	board_[sq] = piece;
	piece_bb_[piece] |= sq;
	color_bb_[color] |= sq;
	piece_index_[sq] = piece_number_[piece]++;
	piece_list_[piece][piece_index_[sq]] = sq;
}

inline void position::delete_piece(const side color, const ptype piece, const square sq)
{
	piece_bb_[piece] ^= sq;
	color_bb_[color] ^= sq;
	const auto last_square = piece_list_[piece][--piece_number_[piece]];
	piece_index_[last_square] = piece_index_[sq];
	piece_list_[piece][piece_index_[last_square]] = last_square;
	piece_list_[piece][piece_number_[piece]] = no_square;
}

inline void position::relocate_piece(const side color, const ptype piece, const square from, const square to)
{
	const auto van_to_bb = square_bb[from] ^ square_bb[to];
	piece_bb_[piece] ^= van_to_bb;
	color_bb_[color] ^= van_to_bb;
	board_[from] = no_piece;
	board_[to] = piece;
	piece_index_[to] = piece_index_[from];
	piece_list_[piece][piece_index_[to]] = to;
}

inline bool position::advanced_pawn(const uint32_t move) const
{
	return piece_type(moved_piece(move)) == pt_pawn
		&& relative_rank(on_move_, to_square(move)) >= rank_6;
}

template <uint8_t piece_type>
inline uint64_t position::attack_from(const square sq) const
{
	return piece_type == pt_bishop
		? attack_bishop_bb(sq, pieces())
		: piece_type == pt_rook
		? attack_rook_bb(sq, pieces())
		: piece_type == pt_queen
		? attack_from<pt_rook>(sq) | attack_from<pt_bishop>(sq)
		: empty_attack[piece_type][sq];
}

template <>
inline uint64_t position::attack_from<pt_pawn>(const square sq, const side color) const
{
	return pawnattack[color][sq];
}

inline uint64_t position::attack_from(const uint8_t piece_t, const square sq) const
{
	return attack_bb(piece_t, sq, pieces());
}

inline uint64_t position::attack_to(const square sq) const
{
	return attack_to(sq, pieces());
}

inline uint64_t position::bishop_color_key() const
{
	return pos_info_->bishop_color_key;
}

inline bool position::capture_or_promotion(const uint32_t move) const
{
	assert(is_ok(move));
	return move < static_cast<uint32_t>(castle_move) ? !empty_square(to_square(move)) : move >= static_cast<uint32_t>(enpassant);
}

inline square position::castle_rook_square(const square king_square) const
{
	return castle_rook_square_[king_square];
}

inline bool position::castling_impossible(const  uint8_t castle) const
{
	return pieces() & castle_path_[castle];
}

inline int position::castling_possible(const  uint8_t castle) const
{
	return pos_info_->castle_possibilities & castle;
}

inline int position::castling_possible(const side color) const
{
	return pos_info_->castle_possibilities & (white_short | white_long) << (2 * color);
}

inline cmhinfo* position::cmh_info() const
{
	return cmh_info_;
}

inline bool position::different_color_bishops() const
{
	return piece_number_[w_bishop] == 1
		&& piece_number_[b_bishop] == 1
		&& different_color(piece_square(white, pt_bishop), piece_square(black, pt_bishop));
}

inline uint64_t position::discovered_check_possible() const
{
	return pos_info_->x_ray[~on_move_] & pieces(on_move_);
}

inline bool position::empty_square(const square sq) const
{
	return board_[sq] == no_piece;
}

inline square position::enpassant_square() const
{
	return pos_info_->enpassant_square;
}

inline int position::fifty_move_counter() const
{
	return pos_info_->draw50_moves;
}

inline int position::game_ply() const
{
	return game_ply_;
}

inline void position::increase_game_ply()
{
	++game_ply_;
}

inline void position::increase_tb_hits()
{
	++tb_hits_;
}

inline bool position::is_capture_move(const uint32_t move) const
{
	assert(is_ok(move));
	return !empty_square(to_square(move)) && move_type(move) != castle_move || move_type(move) == enpassant;
}

inline bool position::is_chess960() const
{
	return chess960_;
}

inline uint64_t position::is_in_check() const
{
	return pos_info_->in_check;
}

inline bool position::is_passed_pawn(const side color, const square sq) const
{
	return !(pieces(~color, pt_pawn) & passedpawn_mask(color, sq));
}

inline uint64_t position::key() const
{
	return pos_info_->key;
}

inline square position::king(const side color) const
{
	return piece_list_[make_piece(color, pt_king)][0];
}

inline uint64_t position::material_key() const
{
	return pos_info_->material_key;
}

inline bool position::material_or_castle_changed() const
{
	return pos_info_->material_key != (pos_info_ - 1)->material_key || pos_info_->castle_possibilities != (pos_info_ - 1)->castle_possibilities;
}

inline ptype position::moved_piece(const uint32_t move) const
{
	return board_[from_square(move)];
}

inline thread* position::my_thread() const
{
	return this_thread_;
}

inline int position::non_pawn_material(const side color) const
{
	return pos_info_->non_pawn_material[color];
}

inline int position::number(const side color, const uint8_t piece) const
{
	return piece_number_[make_piece(color, piece)];
}

inline int position::number(const ptype piece) const
{
	return piece_number_[piece];
}

inline side position::on_move() const
{
	return on_move_;
}

inline bool position::passed_pawn_advance(const uint32_t move, const rank r) const
{
	const auto piece = moved_piece(move);
	return piece_type(piece) == pt_pawn && relative_rank(on_move_, to_square(move)) >= r && is_passed_pawn(piece_color(piece), to_square(move));
}

inline uint64_t position::pawn_key() const
{
	return pos_info_->pawn_key;
}

inline const square* position::piece_list(const side color, const uint8_t piece) const
{
	return piece_list_[make_piece(color, piece)];
}

inline ptype position::piece_on_square(const square sq) const
{
	return board_[sq];
}

inline square position::piece_square(const side color, const uint8_t piece) const
{
	assert(piece_number_[make_piece(color, piece)] == 1);
	return piece_list_[make_piece(color, piece)][0];
}

inline uint64_t position::pieces_excluded(const side color, const uint8_t piece) const
{
	return color_bb_[color] ^ piece_bb_[make_piece(color, piece)];
}

inline uint64_t position::pieces() const
{
	return piece_bb_[all_pieces];
}

inline uint64_t position::pieces(const uint8_t piece) const
{
	return piece_bb_[make_piece(white, piece)] | piece_bb_[make_piece(black, piece)];
}

inline uint64_t position::pieces(const uint8_t piece1, const uint8_t piece2) const
{
	return pieces(piece1) | pieces(piece2);
}

inline uint64_t position::pieces(const side color) const
{
	return color_bb_[color];
}

inline uint64_t position::pieces(const side color, const uint8_t piece) const
{
	return piece_bb_[make_piece(color, piece)];
}

inline uint64_t position::pieces(const side color, const uint8_t piece1, const uint8_t piece2) const
{
	return pieces(color, piece1) | pieces(color, piece2);
}

inline uint64_t position::pieces(const side color, const uint8_t piece1, const uint8_t piece2, const uint8_t piece3) const
{
	return pieces(color, piece1) | pieces(color, piece2) | pieces(color, piece3);
}

inline uint64_t position::pinned_pieces() const
{
	return pos_info_->x_ray[on_move_] & pieces(on_move_);
}

inline int position::psq_score() const
{
	return pos_info_->psq;
}

inline uint64_t position::tb_hits() const
{
	return tb_hits_;
}

inline threadinfo* position::thread_info() const
{
	return thread_info_;
}

inline int position::total_num_pieces() const
{
	return popcnt(pieces());
}

inline uint64_t position::visited_nodes() const
{
	return nodes_;
}






