#pragma once

#include <concepts>

template <typename T>
concept Numeral = (std::floating_point<T> || std::integral<T>) && !std::same_as<long double, T>;
