#include <iostream>
#include "..\WindowAPI\Window.h"
#include <windows.h> 
#include "AppLayer.h"
#include "..\Core\Tesseract-OCR\Test_Tesseract.h"
#include "..\Core\LLM\Llama.h"
#include <string>
#include <thread>

namespace {
    void RunPromptWorkflow()
    {
        AppLayer::PromptInProgress = true;

        const std::string ocrResult = PerformOCR(AppLayer::bmpPath);
        const std::string llmResponse = RunLlamaPrompt(AppLayer::model, ocrResult);

        {
            std::lock_guard<std::mutex> lock(AppLayer::StateMutex);
            AppLayer::OCR_Result = ocrResult;
            AppLayer::LLM_Response = llmResponse;
        }

        std::cout << "\n--- OCR Output ---\n" << ocrResult << std::endl;
        std::cout << "\n--- MODEL OUTPUT ---\n" << llmResponse << "\n";

        AppLayer::PromptInProgress = false;
        WindowAPI::ShowResponseTextBox();
    }
}


int main()
{
        std::cout << "Creating window..." << std::endl;
        try {
            WindowAPI::window* pwindow = new WindowAPI::window(); // Dynamically allocate window object everything is connected with linker
            std::thread promptWorker;
            bool running = true;
            while (running) {
                if (!pwindow->ProcessMessages()) { // send to the window to process messages function
                    std::cout << "Closing window..." << std::endl;
                    running = false;
                }

                if (AppLayer::Prompting.exchange(false) && !AppLayer::PromptInProgress.load()) {
                    if (promptWorker.joinable()) {
                        promptWorker.join();
                    }
                    promptWorker = std::thread(RunPromptWorkflow);
                }

                Sleep(16); // ~60 FPS
            }

            if (promptWorker.joinable()) {
                promptWorker.join();
            }

            delete pwindow;
        } catch (const std::exception& e) {
            std::cerr << "Fatal error: " << e.what() << std::endl;
            return EXIT_FAILURE;
        }
    
        return 0;
}
