#pragma once
#include <cstdint>

using Price = uint64_t; // uint64_t is used to get maximum precision on floating point numbers
using Quantity = uint64_t;
using OrderId = uint64_t;

enum struct Side { Buy, Sell };

enum RequestType
{
    Add,
    Cancel,
    Modify,
    Stop
};
