// SPDX-License-Identifier: AGPL-3.0-only
#include "Generate.hpp"
#include <llama.h>
#include <algorithm>
#include <cstring>
#include <memory>
#include <vector>

namespace Slic3r::GUI::AtlasAssistant {
Result generate(const std::string& model_path, ChatExchange& chat)
{
    if (chat.protocol_version != version || chat.count < 1 || chat.count > max_messages ||
        chat.messages[chat.count - 1].role != Role::user)
        return Result::prompt;
    std::vector<llama_chat_message> messages;
    for (std::uint32_t i = 0; i < chat.count; ++i) {
        const auto& msg = chat.messages[i];
        if (!msg.bytes || msg.bytes > max_message || msg.text[msg.bytes] ||
            std::memchr(msg.text, '\0', msg.bytes))
            return Result::prompt;
        const char* role = nullptr;
        switch (msg.role) {
        case Role::system: role = "system"; break;
        case Role::user: role = "user"; break;
        case Role::assistant: role = "assistant"; break;
        }
        if (!role) return Result::prompt;
        messages.push_back({role, msg.text});
    }

    auto mp = llama_model_default_params();
    mp.n_gpu_layers = 0;
    std::unique_ptr<llama_model, decltype(&llama_model_free)> model(
        llama_model_load_from_file(model_path.c_str(), mp), llama_model_free);
    if (!model) return Result::model;
    const char* tmpl = llama_model_chat_template(model.get(), nullptr);
    if (!tmpl) return Result::model;
    const int length = llama_chat_apply_template(tmpl, messages.data(), messages.size(), true, nullptr, 0);
    if (length <= 0 || length > 32768) return Result::prompt;
    std::vector<char> formatted(length + 1);
    if (llama_chat_apply_template(tmpl, messages.data(), messages.size(), true,
                                  formatted.data(), static_cast<int>(formatted.size())) != length)
        return Result::prompt;

    constexpr int context = 1024, reply_tokens = 128;
    const llama_vocab* vocab = llama_model_get_vocab(model.get());
    std::vector<llama_token> input(context - reply_tokens);
    const int n = llama_tokenize(vocab, formatted.data(), length, input.data(),
                                 static_cast<int>(input.size()), true, true);
    if (n <= 0) return Result::prompt;

    auto cp = llama_context_default_params();
    cp.n_ctx = context;
    cp.n_batch = 256;
    cp.n_ubatch = 128;
    cp.n_threads = cp.n_threads_batch = 2;
    std::unique_ptr<llama_context, decltype(&llama_free)> ctx(llama_init_from_model(model.get(), cp), llama_free);
    if (!ctx) return Result::context;
    std::unique_ptr<llama_sampler, decltype(&llama_sampler_free)> sampler(llama_sampler_init_greedy(), llama_sampler_free);
    if (!sampler) return Result::context;
    std::unique_ptr<llama_batch_ext, decltype(&llama_batch_ext_free)> batch(
        llama_batch_ext_init(ctx.get()), llama_batch_ext_free);
    if (!batch) return Result::context;

    auto decode = [&](const llama_token* tokens, int count, llama_pos pos, bool logits) {
        llama_batch_ext_clear(batch.get());
        for (int i = 0; i < count; ++i) {
            const auto index = llama_batch_ext_add_token(batch.get(), 0, tokens[i]);
            const llama_pos offset = pos + i;
            if (index < 0 || !llama_batch_ext_set_pos(batch.get(), index, &offset)) return false;
        }
        if (logits && !llama_batch_ext_set_output_logits(batch.get(), count - 1, true)) return false;
        return llama_process(ctx.get(), LLAMA_PROCESS_TYPE_DECODE, batch.get()) == 0;
    };
    for (int start = 0; start < n;) {
        const auto count = std::min(128, n - start);
        if (!decode(input.data() + start, count, start, start + count == n))
            return Result::generation;
        start += count;
    }

    std::string reply;
    for (int i = 0; i < reply_tokens; ++i) {
        const auto token = llama_sampler_sample(sampler.get(), ctx.get(), -1);
        if (llama_vocab_is_eog(vocab, token)) break;
        char piece[256];
        const auto size = llama_token_to_piece(vocab, token, piece, sizeof(piece), 0, false);
        if (size < 0) return Result::generation;
        if (reply.size() + size > max_reply) break;
        reply.append(piece, size);
        if (i + 1 < reply_tokens && !decode(&token, 1, n + i, true)) return Result::generation;
    }
    // A token can end partway through a UTF-8 character at the reply limit.
    if (!reply.empty()) {
        std::size_t last = reply.size() - 1;
        while (last && (static_cast<unsigned char>(reply[last]) & 0xc0) == 0x80) --last;
        const auto lead = static_cast<unsigned char>(reply[last]);
        const unsigned needed = lead < 0x80 ? 1 : lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;
        if (reply.size() - last < needed) reply.resize(last);
    }
    if (reply.empty()) return Result::generation;
    std::memcpy(chat.reply, reply.data(), reply.size());
    chat.reply_bytes = static_cast<std::uint32_t>(reply.size());
    return Result::ok;
}
}
