#pragma once
#include "../position.h"

void syzygy_path_init(const std::string& path);
int syzygy_probe_wdl(position& pos, int* success);
int syzygy_probe_dtz(position& pos, int* success);
extern int tb_num_piece, tb_num_pawn, tb_max_men;
