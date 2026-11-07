// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <cstdint>
#include <cstddef>

namespace Slic3r::AtlasVoice {
constexpr std::uint32_t version = 1;
constexpr std::uint32_t sample_rate = 16000;
constexpr std::uint32_t min_samples = sample_rate / 4;
constexpr std::uint32_t max_samples = 15 * sample_rate;
constexpr std::size_t text_capacity = 8192;
struct Exchange {
    std::uint32_t protocol_version;
    std::uint32_t samples;
    std::uint32_t text_bytes;
    std::int16_t pcm[max_samples];
    char text[text_capacity];
};
constexpr bool valid_samples(std::uint32_t n) { return n >= min_samples && n <= max_samples; }
constexpr bool valid_text(std::uint32_t n) { return n < text_capacity; }
}
