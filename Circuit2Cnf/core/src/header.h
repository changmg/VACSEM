#pragma once


#include <algorithm>
#include <bitset>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/dynamic_bitset.hpp>
#include <boost/functional/hash.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/timer/progress_display.hpp>
#include <boost/timer/timer.hpp>
#include <boost/random.hpp>
#include <concepts>
#include <filesystem>
#include <iostream>
#include <limits.h>
#include <list>
#include <memory>
#include <queue>
#include <random>
#include <regex>
#include <set>
#include <sstream>
#include <stack>
#include <stdint.h>
#include <termios.h>
#include <thread>
#include <tuple>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>


using ll = long long;
using ull = unsigned long;
using LLVec = std::vector<ll>;
using ULLVec = std::vector<ull>;
using IntVec = std::vector<int>;
using DblVec = std::vector<double>;
using LLSet = std::set<ll>;
using IntSet = std::set<int>;
using DblSet = std::set<double>;
using LLPair = std::pair<ll, ll>;
using IntPair = std::pair<int, int>;
using DblPair = std::pair<double, double>;
using BigFlt = boost::multiprecision::cpp_dec_float_100;
using BigInt = boost::multiprecision::int512_t;
using BitVec = boost::dynamic_bitset<ull>;


#define DEBUG