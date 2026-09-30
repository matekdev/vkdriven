#pragma once

#include <expected>
#include <stdexcept>
#include <string>
#include <utility>

template <typename T> T orThrow(std::expected<T, std::string> result)
{
    if (!result)
        throw std::runtime_error{result.error()};
    return std::move(*result);
}
