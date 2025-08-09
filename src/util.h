/*
 * Chess Engine - Utilities (Interface)
 * ------------------------------------
 * Thread-safe console output helper (acout) and utility functions
 * for move formatting and build information.
 */

/*
 * Chess Engine - Utility Functions (Interface)
 * ---------------------------------------------
 * Declares miscellaneous helpers, the acout class for synchronized
 * output, and string/time utilities used across the engine.
 */

#pragma once
#include <iostream>
#include <mutex>
#include <random>
#include "position.h"

static std::mutex mutex_cout;

/*
 * acout
 * -----
 * Simple synchronized ostream-like helper:
 *   - Locks a global mutex on construction
 *   - Unlocks automatically on destruction
 * Use: acout() << "text" << std::endl;
 */

struct acout{
  std::unique_lock<std::mutex> lk;
  acout() : lk(mutex_cout){}

  template<typename t> acout& operator<<(const t& str){
    std::cout<<str;
    return *this;
  }

  acout& operator<<(std::ostream& (*fp)(std::ostream&)){
    std::cout<<fp;
    return *this;
  }
};

// Map from internal piece enum to display character
const std::string piece_to_char(" KPNBRQ  kpnbrq");
// Convert packed move to UCI string
std::string move_to_string(uint32_t move,const position& pos);
// Parse UCI or castle string to packed move
uint32_t move_from_string(const position& pos,std::string& str);

/*
 * random
 * ------
 * Thin wrapper around std::mt19937_64 to generate 64-bit values.
 * Provides static rand<T>() that returns a random value of type T.
 */

class random{
  static uint64_t rand64(){
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;
    return dis(gen);
  }
public:
  uint64_t s;

  explicit random(const uint64_t seed) : s(seed){}

  template<typename t> static t rand(){
    return t(rand64());
  }
};

// Print engine identification line
void engine_info();
// Print build date and time line
void build_info();
// Stream the board as ASCII
std::ostream& operator<<(std::ostream& os,const position& pos);
