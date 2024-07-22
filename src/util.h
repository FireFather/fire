#pragma once
#include <iostream>
#include <mutex>
#include <random>

#include "position.h"

static std::mutex mutex_cout;

struct acout {
  std::unique_lock<std::mutex> lk;
  acout() : lk(std::unique_lock(mutex_cout)) {}

  template <typename T>
  acout& operator<<(const T& t) {
    std::cout << t;
    return *this;
  }

  acout& operator<<(std::ostream& (*fp)(std::ostream&)) {
    std::cout << fp;
    return *this;
  }
};

const std::string piece_to_char(" KPNBRQ  kpnbrq");
std::string move_to_string(uint32_t move, const position& pos);
uint32_t move_from_string(const position& pos, std::string& str);

class random {
  static uint64_t rand64() {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;
    return dis(gen);
  }

public:
  uint64_t s;
  explicit random(const uint64_t seed) : s(seed) { assert(seed); }

  template <typename T>
  static T rand() { return T(rand64()); }
};

void engine_info();
void build_info();
std::ostream& operator<<(std::ostream& os, const position& pos);
