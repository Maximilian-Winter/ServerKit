//
// Created by maxim on 27.08.2024.
//
#include "HTTPServer.h"
#include "HTTPClient.h"
#include "ggml.h"
#include "clip.h"
#include "llava.h"
#include "llama.h"
#include "base64.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <vector>
#include <mutex>

using json = nlohmann::json;

struct LLaVAServerContext {
    gpt_params params;
    std::unique_ptr<llava_context> ctx_llava;
    std::mutex model_mutex;
};

static LLaVAServerContext g_context;

// Helper functions (implement these based on the CLI code)
static bool init_llava_context(const std::string& model_path, const std::string& mmproj_path, int n_ctx);
static llava_image_embed* process_image(const std::string& base64_image);
static std::string generate_response(llava_image_embed* image_embed, const std::string& prompt);

// Request handler for image analysis
HTTPMessage handle_analyze_image(const HTTPMessage& request) {
    HTTPMessage response;
    response.setVersion("HTTP/1.1");

    try {
        json req_body = json::parse(request.getBody().getContent());
        std::string base64_image = req_body["image"];
        std::string prompt = req_body["prompt"];

        std::lock_guard<std::mutex> lock(g_context.model_mutex);

        auto image_embed = process_image(base64_image);
        if (!image_embed) {
            response.setStatusCode(400);
            response.setStatusMessage("Bad Request");
            response.addHeader("Content-Type", "application/json");
            json error_response = {{"error", "Failed to process image"}};
            response.getBody().setContent(error_response.dump());
            return response;
        }

        std::string result = generate_response(image_embed, prompt);
        llava_image_embed_free(image_embed);

        response.setStatusCode(200);
        response.setStatusMessage("OK");
        response.addHeader("Content-Type", "application/json");
        json success_response = {{"result", result}};
        response.getBody().setContent(success_response.dump());
    }
    catch (const std::exception& e) {
        response.setStatusCode(500);
        response.setStatusMessage("Internal Server Error");
        response.addHeader("Content-Type", "application/json");
        json error_response = {{"error", e.what()}};
        response.getBody().setContent(error_response.dump());
    }

    return response;
}

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <model_path> <mmproj_path> <port> [n_ctx]" << std::endl;
        return 1;
    }

    std::string model_path = argv[1];
    std::string mmproj_path = argv[2];
    int port = std::stoi(argv[3]);
    int n_ctx = (argc > 4) ? std::stoi(argv[4]) : 2048;

    if (!init_llava_context(model_path, mmproj_path, n_ctx)) {
        std::cerr << "Failed to initialize LLaVA context" << std::endl;
        return 1;
    }

    HTTPServer server("config.json");  // Assuming you have a config file

    server.setRequestHandler(HTTPHeader::Method::POST_METHOD, "/analyze", handle_analyze_image);

    std::cout << "Starting LLaVA HTTP server on port " << port << std::endl;
    server.start();

    return 0;
}

// Implement these helper functions based on the CLI code
static bool init_llava_context(const std::string& model_path, const std::string& mmproj_path, int n_ctx) {
    g_context.params.model = model_path;
    g_context.params.mmproj = mmproj_path;
    g_context.params.n_ctx = n_ctx;

    auto model = llava_init(&g_context.params);
    if (!model) {
        return false;
    }

    g_context.ctx_llava.reset(llava_init_context(&g_context.params, model));
    return g_context.ctx_llava != nullptr;
}

static llava_image_embed* process_image(const std::string& base64_image) {
    std::vector<unsigned char> image_data;
    base64::decode(base64_image.begin(), base64_image.end(), std::back_inserter(image_data));

    return llava_image_embed_make_with_bytes(
            g_context.ctx_llava->ctx_clip,
            g_context.params.n_threads,
            image_data.data(),
            image_data.size()
    );
}

static std::string generate_response(llava_image_embed* image_embed, const std::string& prompt) {
    int n_past = 0;
    const int max_tokens = 256;  // You might want to make this configurable

    eval_string(g_context.ctx_llava->ctx_llama, prompt.c_str(), g_context.params.n_batch, &n_past, true);
    llava_eval_image_embed(g_context.ctx_llava->ctx_llama, image_embed, g_context.params.n_batch, &n_past);

    std::string response;
    struct llama_sampling_context* ctx_sampling = llama_sampling_init(g_context.params.sparams);

    for (int i = 0; i < max_tokens; i++) {
        const char* token = sample(ctx_sampling, g_context.ctx_llava->ctx_llama, &n_past);
        if (strcmp(token, "</s>") == 0 || strstr(token, "###") || strstr(response.c_str(), "<|im_end|>")) {
            break;
        }
        response += token;
    }

    llama_sampling_free(ctx_sampling);
    return response;
}