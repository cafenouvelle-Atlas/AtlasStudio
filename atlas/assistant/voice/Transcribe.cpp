// SPDX-License-Identifier: AGPL-3.0-only
#include "Transcribe.hpp"
#include <whisper.h>
#include <cstring>
#include <memory>
#include <vector>

namespace Slic3r::AtlasVoice {
int transcribe(Exchange& data, const char* model_path)
{
    if (data.protocol_version != version || !valid_samples(data.samples))
        return 2;
    auto config = whisper_context_default_params();
    config.use_gpu = false;
    std::unique_ptr<whisper_context, decltype(&whisper_free)> ctx(
        whisper_init_from_file_with_params(model_path, config), whisper_free);
    if (!ctx)
        return 3;

    std::vector<float> samples(data.samples);
    for (std::size_t i = 0; i < samples.size(); ++i)
        samples[i] = data.pcm[i] / 32768.f;

    auto params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.n_threads = 2;
    params.language = "en";
    params.no_context = true;
    params.print_progress = params.print_realtime = params.print_timestamps = false;
    if (whisper_full(ctx.get(), params, samples.data(), static_cast<int>(samples.size())) != 0)
        return 4;

    std::size_t size = 0;
    for (int i = 0; i < whisper_full_n_segments(ctx.get()); ++i) {
        const char* segment = whisper_full_get_segment_text(ctx.get(), i);
        if (!segment)
            return 4;
        const auto length = std::strlen(segment);
        if (length >= text_capacity - size)
            return 5;
        std::memcpy(data.text + size, segment, length);
        size += length;
    }
    data.text_bytes = static_cast<std::uint32_t>(size);
    return 0;
}

}
