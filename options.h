/*
-----------------------------------------------------------------------------
This source file is part of the Havoc chess engine
Copyright (c) 2020 Minniesoft
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
-----------------------------------------------------------------------------
*/

/* adapted and modified for use in fire via clang core cpp guidelines:
vars moved to inner scope
member function naming
removed template arguments
use 'default' for class destructor init
argc made const
removed redundant inline specifiers
initialized template vars
*/

#pragma once

#include <string>
#include <sstream>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <fstream>
#include <vector>

#include "search.h"
#include "util/util.h"

class options
{
	std::mutex m_;
	std::map<std::string, std::string> opts_;
	void load_args(int argc, char* argv[]);

public:
	options() = delete;
	options(const options&) = delete;
	options(const options&&) = delete;
	options& operator=(const options&) = delete;
	options& operator=(const options&&) = delete;

	options(const int argc, char* argv[])
	{
		load_args(argc, argv);
	}
	~options() = default;

	template<typename T>
	T value(const char* s)
	{
		std::unique_lock lock(m_);
		std::istringstream ss{ opts_[std::string{s}] };
		T v{};
		ss >> v;
		return v;
	}

	template<typename T>
	void set(const std::string key, const T value)
	{
		std::unique_lock lock(m_);

		if (std::string vs = std::to_string(value); opts_.find(key) == opts_.end())
		{
			opts_.emplace(key, vs);
		}
		else opts_[key] = vs;
	}

	bool read_param_file(std::string& filename);
	bool save_param_file(std::string& filename);
	void set_engine_params();
};

inline void options::load_args(const int argc, char* argv[])
{

	auto matches = [](const std::string& s1, const char* s2) { return strcmp(s1.c_str(), s2) == 0; };
	auto set = [this](const std::string& k, std::string& v) { this->opts_.emplace(k.substr(1), v); };

	for (int j = 1; j < argc - 1; j += 2)
	{
		std::string key = argv[j];
		if (std::string val = argv[j + 1];
			matches(key, "-threads")) set(key, val);
		else if (matches(key, "-book")) set(key, val);
		else if (matches(key, "-hash")) set(key, val);
		else if (matches(key, "-tune")) set(key, val);
		else if (matches(key, "-bench")) set(key, val);
		else if (matches(key, "-param")) set(key, val);
	}
}

inline bool options::read_param_file(std::string& filename)
{
	std::string line;

	if (filename.empty())
	{
		filename = value<std::string>("param");
		if (filename.empty())
		{
			filename = "engine.conf";
		}
	}
	else acout() << "info string...reading param file " << filename << std::endl;

	std::ifstream param_file(filename);
	auto set = [this](std::string& k, std::string& v) { this->opts_.emplace(k, v); };

	while (std::getline(param_file, line))
	{

		// assumed format "param-tag:param-value"
		std::vector<std::string> tokens = util::split(line, ' ');

		if (tokens.size() != 2)
		{
			continue;
		}

		set(tokens[0], tokens[1]);
		acout() << "loaded param: " << tokens[0] << " value: " << tokens[1] << std::endl;
	}

	set_engine_params();
	return true;
}

inline bool options::save_param_file(std::string& filename)
{

	if (filename.empty())
	{
		filename = value<std::string>("param");
		if (filename.empty()) {
			filename = "engine.conf"; // default
		}
	}

	std::ofstream param_file(filename, std::ofstream::out);

	for (const auto& [fst, snd] : opts_)
	{
		std::string line = fst + ":" += snd + "\n";
		param_file << line;
		//acout() << "saved " << line << " into engine.conf " << std::endl;
	}
	param_file.close();
	return true;
}

inline void options::set_engine_params()
{
	auto matches = [](const std::string& s1, const char* s2) { return strcmp(s1.c_str(), s2) == 0; };

	for (const auto& [fst, snd] : opts_)
	{
		if (matches(fst, "best_value_vp_mult")) { best_value_vp_mult = value<int>("best_value_vp_mult"); }
		else if (matches(fst, "counter_move_bonus_min_depth")) { search::counter_move_bonus_min_depth = value<int>("counter_move_bonus_min_depth"); }
		else if (matches(fst, "default_draw_value")) { default_draw_value = value<int>("default_draw_value"); }
		else if (matches(fst, "delta_alphabeta_value_add")) { delta_alphabeta_value_add = value<int>("delta_alphabeta_value_add"); }
		else if (matches(fst, "delta_alpha_margin")) { delta_alpha_margin = value<int>("delta_alpha_margin"); }
		else if (matches(fst, "delta_beta_margin")) { delta_beta_margin = value<int>("delta_beta_margin"); }
		else if (matches(fst, "dummy_null_move_threat_min_depth_mult")) { search::dummy_null_move_threat_min_depth_mult = value<int>("dummy_null_move_threat_min_depth_mult"); }
		else if (matches(fst, "excluded_move_hash_depth_reduction")) { search::excluded_move_hash_depth_reduction = value<int>("excluded_move_hash_depth_reduction"); }
		else if (matches(fst, "excluded_move_min_depth")) { search::excluded_move_min_depth = value<int>("excluded_move_min_depth"); }
		else if (matches(fst, "excluded_move_r_beta_hash_value_margin_div")) { search::excluded_move_r_beta_hash_value_margin_div = value<int>("excluded_move_r_beta_hash_value_margin_div"); }
		else if (matches(fst, "excluded_move_r_beta_hash_value_margin_mult")) { search::excluded_move_r_beta_hash_value_margin_mult = value<int>("excluded_move_r_beta_hash_value_margin_mult"); }
		else if (matches(fst, "fail_low_counter_move_bonus_min_depth")) { search::fail_low_counter_move_bonus_min_depth = value<int>("fail_low_counter_move_bonus_min_depth"); }
		else if (matches(fst, "fail_low_counter_move_bonus_min_depth_margin")) { search::fail_low_counter_move_bonus_min_depth_margin = value<int>("fail_low_counter_move_bonus_min_depth_margin"); }
		else if (matches(fst, "fail_low_counter_move_bonus_min_depth_mult")) { search::fail_low_counter_move_bonus_min_depth_mult = value<int>("fail_low_counter_move_bonus_min_depth_mult"); }
		else if (matches(fst, "futility_min_depth")) { search::futility_min_depth = value<int>("futility_min_depth"); }
		else if (matches(fst, "futility_value_0")) { search::futility_value_0 = value<int>("futility_value_0"); }
		else if (matches(fst, "futility_value_1")) { search::futility_value_1 = value<int>("futility_value_1"); }
		else if (matches(fst, "futility_value_2")) { search::futility_value_2 = value<int>("futility_value_2"); }
		else if (matches(fst, "futility_value_3")) { search::futility_value_3 = value<int>("futility_value_3"); }
		else if (matches(fst, "futility_value_4")) { search::futility_value_4 = value<int>("futility_value_4"); }
		else if (matches(fst, "futility_value_5")) { search::futility_value_5 = value<int>("futility_value_5"); }
		else if (matches(fst, "futility_value_6")) { search::futility_value_6 = value<int>("futility_value_6"); }
		else if (matches(fst, "futility_margin_ext_base")) { search::futility_margin_ext_base = value<int>("futility_margin_ext_base"); }
		else if (matches(fst, "futility_margin_ext_mult")) { search::futility_margin_ext_mult = value<int>("futility_margin_ext_mult"); }
		else if (matches(fst, "improvement_factor_min_base")) { improvement_factor_min_base = value<int>("improvement_factor_min_base"); }
		else if (matches(fst, "improvement_factor_max_base")) { improvement_factor_max_base = value<int>("improvement_factor_max_base"); }
		else if (matches(fst, "improvement_factor_max_mult")) { improvement_factor_max_mult = value<int>("improvement_factor_max_mult"); }
		else if (matches(fst, "improvement_factor_bv_mult")) { improvement_factor_bv_mult = value<int>("improvement_factor_bv_mult"); }
		else if (matches(fst, "late_move_count_max_depth")) { search::late_move_count_max_depth = value<int>("late_move_count_max_depth"); }
		else if (matches(fst, "lazy_margin")) { search::lazy_margin = value<int>("lazy_margin"); }
		else if (matches(fst, "limit_depth_min")) { search::limit_depth_min = value<int>("limit_depth_min"); }
		else if (matches(fst, "lmr_min_depth")) { search::lmr_min_depth = value<int>("lmr_min_depth"); }
		else if (matches(fst, "lmr_reduction_min")) { search::lmr_reduction_min = value<int>("lmr_reduction_min"); }
		else if (matches(fst, "max_gain_value")) { search::max_gain_value = value<int>("max_gain_value"); }
		else if (matches(fst, "no_early_pruning_min_depth")) { search::no_early_pruning_min_depth = value<int>("no_early_pruning_min_depth"); }
		else if (matches(fst, "no_early_pruning_non_pv_node_depth_min")) { search::no_early_pruning_non_pv_node_depth_min = value<int>("no_early_pruning_non_pv_node_depth_min"); }
		else if (matches(fst, "no_early_pruning_position_value_margin")) { search::no_early_pruning_position_value_margin = value<int>("no_early_pruning_position_value_margin"); }
		else if (matches(fst, "no_early_pruning_pv_node_depth_min")) { search::no_early_pruning_pv_node_depth_min = value<int>("no_early_pruning_pv_node_depth_min"); }
		else if (matches(fst, "no_early_pruning_strong_threat_min_depth")) { search::no_early_pruning_strong_threat_min_depth = value<int>("no_early_pruning_strong_threat_min_depth"); }
		else if (matches(fst, "non_root_node_max_depth")) { search::non_root_node_max_depth = value<int>("non_root_node_max_depth"); }
		else if (matches(fst, "non_root_node_see_test_base")) { search::non_root_node_see_test_base = value<int>("non_root_node_see_test_base"); }
		else if (matches(fst, "non_root_node_see_test_mult")) { search::non_root_node_see_test_mult = value<int>("non_root_node_see_test_mult"); }
		else if (matches(fst, "null_move_depth_greater_than_cut_node_mult")) { search::null_move_depth_greater_than_cut_node_mult = value<int>("null_move_depth_greater_than_cut_node_mult"); }
		else if (matches(fst, "null_move_depth_greater_than_div")) { search::null_move_depth_greater_than_div = value<int>("null_move_depth_greater_than_div"); }
		else if (matches(fst, "null_move_depth_greater_than_mult")) { search::null_move_depth_greater_than_mult = value<int>("null_move_depth_greater_than_mult"); }
		else if (matches(fst, "null_move_depth_greater_than_sub")) { search::null_move_depth_greater_than_sub = value<int>("null_move_depth_greater_than_sub"); }
		else if (matches(fst, "null_move_max_depth")) { search::null_move_max_depth = value<int>("null_move_max_depth"); }
		else if (matches(fst, "null_move_min_depth")) { search::null_move_min_depth = value<int>("null_move_min_depth"); }
		else if (matches(fst, "null_move_pos_val_less_than_beta_mult")) { search::null_move_pos_val_less_than_beta_mult = value<int>("null_move_pos_val_less_than_beta_mult"); }
		else if (matches(fst, "null_move_strong_threat_mult")) { search::null_move_strong_threat_mult = value<int>("null_move_strong_threat_mult"); }
		else if (matches(fst, "null_move_tempo_mult")) { search::null_move_tempo_mult = value<int>("null_move_tempo_mult"); }
		else if (matches(fst, "null_move_tm_base")) { search::null_move_tm_base = value<int>("null_move_tm_base"); }
		else if (matches(fst, "null_move_tm_mult")) { search::null_move_tm_mult = value<int>("null_move_tm_mult"); }
		else if (matches(fst, "only_quiet_check_moves_max_depth")) { search::only_quiet_check_moves_max_depth = value<int>("only_quiet_check_moves_max_depth"); }
		else if (matches(fst, "pc_beta_depth_margin")) { search::pc_beta_depth_margin = value<int>("pc_beta_depth_margin"); }
		else if (matches(fst, "pc_beta_margin")) { search::pc_beta_margin = value<int>("pc_beta_margin"); }
		else if (matches(fst, "predicted_depth_max_depth")) { search::predicted_depth_max_depth = value<int>("predicted_depth_max_depth"); }
		else if (matches(fst, "predicted_depth_see_test_base")) { search::predicted_depth_see_test_base = value<int>("predicted_depth_see_test_base"); }
		else if (matches(fst, "predicted_depth_see_test_mult")) { search::predicted_depth_see_test_mult = value<int>("predicted_depth_see_test_mult"); }
		else if (matches(fst, "qs_futility_value_0")) { search::qs_futility_value_0 = value<int>("qs_futility_value_0"); }
		else if (matches(fst, "qs_futility_value_1")) { search::qs_futility_value_1 = value<int>("qs_futility_value_1"); }
		else if (matches(fst, "qs_futility_value_2")) { search::qs_futility_value_2 = value<int>("qs_futility_value_2"); }
		else if (matches(fst, "qs_futility_value_3")) { search::qs_futility_value_3 = value<int>("qs_futility_value_3"); }
		else if (matches(fst, "qs_futility_value_4")) { search::qs_futility_value_4 = value<int>("qs_futility_value_4"); }
		else if (matches(fst, "qs_futility_value_5")) { search::qs_futility_value_5 = value<int>("qs_futility_value_5"); }
		else if (matches(fst, "qs_futility_value_5")) { search::qs_futility_value_6 = value<int>("qs_futility_value_6"); }
		else if (matches(fst, "qs_futility_value_7")) { search::qs_futility_value_7 = value<int>("qs_futility_value_7"); }
		else if (matches(fst, "qs_futility_basis_margin")) { search::qs_futility_basis_margin = value<int>("qs_futility_basis_margin"); }
		else if (matches(fst, "qs_futility_value_capture_history_add_div")) { search::qs_futility_value_capture_history_add_div = value<int>("qs_futility_value_capture_history_add_div"); }
		else if (matches(fst, "qs_stats_value_sortvalue")) { search::qs_stats_value_sortvalue = value<int>("qs_stats_value_sortvalue"); }
		else if (matches(fst, "qs_skip_see_test_value_greater_than_alpha_sort_value")) { search::qs_skip_see_test_value_greater_than_alpha_sort_value = value<int>("qs_skip_see_test_value_greater_than_alpha_sort_value"); }
		else if (matches(fst, "qs_skip_see_test_value_less_than_equal_to_alpha_sort_value")) { search::qs_skip_see_test_value_less_than_equal_to_alpha_sort_value = value<int>("qs_skip_see_test_value_less_than_equal_to_alpha_sort_value"); }
		else if (matches(fst, "quiet_moves_max_depth")) { search::quiet_moves_max_depth = value<int>("quiet_moves_max_depth"); }
		else if (matches(fst, "quiet_moves_max_gain_base")) { search::quiet_moves_max_gain_base = value<int>("quiet_moves_max_gain_base"); }
		else if (matches(fst, "quiet_moves_max_gain_mult")) { search::quiet_moves_max_gain_mult = value<int>("quiet_moves_max_gain_mult"); }
		else if (matches(fst, "r_stats_value_div")) { search::r_stats_value_div = value<int>("r_stats_value_div"); }
		else if (matches(fst, "r_stats_value_mult_div")) { search::r_stats_value_mult_div = value<int>("r_stats_value_mult_div"); }
		else if (matches(fst, "razor_margin")) { search::razor_margin = value<int>("razor_margin"); }
		else if (matches(fst, "razoring_min_depth")) { search::razoring_min_depth = value<int>("razoring_min_depth"); }
		else if (matches(fst, "razoring_qs_min_depth")) { search::razoring_qs_min_depth = value<int>("razoring_qs_min_depth"); }
		else if (matches(fst, "root_depth_mate_value_bv_add")) { root_depth_mate_value_bv_add = value<int>("root_depth_mate_value_bv_add"); }
		else if (matches(fst, "sort_cmp_sort_value")) { search::sort_cmp_sort_value = value<int>("sort_cmp_sort_value"); }
		else if (matches(fst, "stats_value_sort_value_add")) { search::stats_value_sort_value_add = value<int>("stats_value_sort_value_add"); }
		else if (matches(fst, "time_control_optimum_mult_1")) { time_control_optimum_mult_1 = value<int>("time_control_optimum_mult_1"); }
		else if (matches(fst, "time_control_optimum_mult_2")) { time_control_optimum_mult_2 = value<int>("time_control_optimum_mult_2"); }
		else if (matches(fst, "v_singular_margin")) { v_singular_margin = value<int>("v_singular_margin"); }
		else if (matches(fst, "value_greater_than_beta_max_depth")) { search::value_greater_than_beta_max_depth = value<int>("value_greater_than_beta_max_depth"); }
		else if (matches(fst, "value_less_than_beta_margin")) { search::value_less_than_beta_margin = value<int>("value_less_than_beta_margin"); }
		else if (matches(fst, "value_pawn_mult")) { value_pawn_mult = value<int>("value_pawn_mult"); }
	}
}

extern std::unique_ptr<options> params;
