#include <cstddef>
#include <cmath>

size_t nextPowerOf2(size_t n)
{
    if (n == 0) return 1;
    if ((n & (n - 1)) == 0) return n;
    return (size_t)(std::pow(2, std::ceil(std::log2(n))));
}