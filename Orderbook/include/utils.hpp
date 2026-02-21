#pragma once
#include <cstddef>
#include <cmath>
#include <string>

inline size_t nextPowerOf2(size_t n)
{
    if (n == 0) return 1;
    if ((n & (n - 1)) == 0) return n;
    return (size_t)(std::pow(2, std::ceil(std::log2(n))));
}

inline std::string repeatStr(const char* s, int n) {
  std::string result;
  for (int i = 0; i < n; i++) result += s;
  return result;
}