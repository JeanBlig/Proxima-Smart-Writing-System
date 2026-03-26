#pragma once
#include <atomic>
#include <mutex>
#include <string>

class AppLayer{
    public:
        static inline std::string bmpPath = "screenshot.bmp";
        static inline std::string OCR_Result;
        static inline std::string LLM_Response;
        static inline std::atomic<bool> Prompting = false;
        static inline std::atomic<bool> PromptInProgress = false;
        static inline std::mutex StateMutex;
        static inline std::string model = "C:/Users/Change me/llama.cpp/models/mistral-7b-instruct-v0.2.Q4_K_M.gguf";
};


