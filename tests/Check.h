#pragma once
#include <cmath>
#include <stdexcept>
#include <string>
inline void Check(bool result, const char* message) {
    if (!result) throw std::runtime_error(message);
}
inline bool Near(float a, float b) { return std::abs(a-b) < 0.0001f; }
