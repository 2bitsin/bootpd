#pragma once

#include <cstdint>
#include <cstddef>
#include <span>

auto crc32 (std::span<const uint8_t> buffer, uint32_t crc = 0u) -> uint32_t;