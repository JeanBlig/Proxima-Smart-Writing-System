#include "Llama.h"
#include <string>
#include <vector>
#include <stdexcept>
#include <cstdint>
#include <llama.h>

// -------------------- Helpers --------------------

static void SuppressLlamaLogs(ggml_log_level, const char *, void *) {}

static std::vector<llama_token> TokenizePrompt(
    const llama_vocab * vocab,
    const std::string & text,
    bool add_special,
    bool parse_special)
{
    int32_t cap = (int32_t)text.size() + (add_special ? 8 : 0);
    if (cap < 32) cap = 32;

    std::vector<llama_token> tokens((size_t)cap);

    int32_t n = llama_tokenize(
        vocab,
        text.c_str(),
        (int32_t)text.size(),
        tokens.data(),
        (int32_t)tokens.size(),
        add_special,
        parse_special
    );

    if (n < 0) {
        tokens.resize((size_t)(-n));
        n = llama_tokenize(
            vocab,
            text.c_str(),
            (int32_t)text.size(),
            tokens.data(),
            (int32_t)tokens.size(),
            add_special,
            parse_special
        );
    }

    if (n < 0) {
        throw std::runtime_error("llama_tokenize failed");
    }

    tokens.resize((size_t)n);
    return tokens;
}

static std::string TokenToText(const llama_vocab * vocab, llama_token tok)
{
    std::string out;
    out.resize(8 * 1024);

    // lstrip:
    //   0 = don't strip anything
    //   1 = strip ONE leading space if present (recommended for clean output)
    const int32_t n = llama_token_to_piece(
        vocab,
        tok,
        out.data(),
        (int32_t)out.size(),
        /*lstrip=*/0,
        /*special=*/false
    );

    if (n <= 0) return std::string();

    out.resize((size_t)n);
    return out;
}
// -------------------- Main exported function --------------------

std::string RunLlamaPrompt(const std::string& model_path,
                           const std::string& prompt)
{
    llama_log_set(SuppressLlamaLogs, nullptr);
    llama_backend_init();

    // Load model
    llama_model_params mparams = llama_model_default_params();
    llama_model * model = llama_model_load_from_file(model_path.c_str(), mparams);
    if (!model) {
        llama_backend_free();
        return "Failed to load model: " + model_path;
    }

    // Create context
    llama_context_params cparams = llama_context_default_params();
    cparams.n_ctx = 1024;

    llama_context * ctx = llama_new_context_with_model(model, cparams);
    if (!ctx) {
        llama_model_free(model);
        llama_backend_free();
        return "Failed to create context.";
    }

    const llama_vocab * vocab = llama_model_get_vocab(model);

    // Tokenize prompt (parse_special=false for normal text prompts)
    std::vector<llama_token> prompt_tokens;
    try {
        prompt_tokens = TokenizePrompt(vocab, prompt, /*add_special=*/true, /*parse_special=*/false);
    } catch (...) {
        llama_free(ctx);
        llama_model_free(model);
        llama_backend_free();
        return "Tokenization failed.";
    }

    // Decode prompt tokens
    {
        llama_batch batch = llama_batch_get_one(prompt_tokens.data(), (int32_t)prompt_tokens.size());
        if (llama_decode(ctx, batch) != 0) {
            llama_free(ctx);
            llama_model_free(model);
            llama_backend_free();
            return "Decoding prompt failed.";
        }
    }

    // Greedy sampler
    llama_sampler * sampler = llama_sampler_init_greedy();

    std::string out;
    const int max_new_tokens = 64;

    for (int i = 0; i < max_new_tokens; ++i) {
        llama_token tok = llama_sampler_sample(sampler, ctx, -1);

        if (tok == llama_token_eos(vocab)) break;

        out += TokenToText(vocab, tok);

        llama_sampler_accept(sampler, tok);

        llama_batch batch = llama_batch_get_one(&tok, 1);
        if (llama_decode(ctx, batch) != 0) break;
    }

    llama_sampler_free(sampler);
    llama_free(ctx);
    llama_model_free(model);
    llama_backend_free();

    return out;
}
