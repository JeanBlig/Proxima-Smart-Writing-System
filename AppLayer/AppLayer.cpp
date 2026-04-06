#include <iostream>
#include "..\WindowAPI\Window.h"
#include <windows.h> 
#include "AppLayer.h"
#include "..\Core\Tesseract-OCR\Test_Tesseract.h"
#include "..\Core\LLM\Llama.h"
#include <string>
#include <thread>

namespace { // Makes it local to this file
    void RunPromptWorkflow()
    {
        AppLayer::PromptInProgress = true; // Prevents another 

        const std::string ocrResult = PerformOCR(AppLayer::bmpPath); // Const so that it does not change
        const std::string llmResponse = RunLlamaPrompt(AppLayer::model, ocrResult);

        { // creates a bloch scope
            std::lock_guard<std::mutex> lock(AppLayer::StateMutex); // Locks values so that other threads dont access and change it while one thread is running
            AppLayer::OCR_Result = ocrResult;                      // Creates mutex objec
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
            std::thread promptWorker; // Thread Object. initially empty
            bool running = true; // Controls main loop
            while (running) {
                if (!pwindow->ProcessMessages()) { // send to the window to process messages function
                    std::cout << "Closing window..." << std::endl;
                    running = false;
                }

                if (AppLayer::Prompting.exchange(false) && !AppLayer::PromptInProgress.load()) {
                    // Immidiately switches value of Prompting to false if is was True. Because we are in a while loop.
                    // && No prompt is running
                    if (promptWorker.joinable()) { // If previous thread exists
                        promptWorker.join(); // Wait for it to finish befor reusing
                    }
                    promptWorker = std::thread(RunPromptWorkflow); // Start the new background thread, running OCR and LLM
                }

                Sleep(16); // ~60 FPS
            }

            if (promptWorker.joinable()) { // If thread still exists when exiting
                promptWorker.join(); // wait for it to finish
            }

            delete pwindow; // Free memory for window object

        } catch (const std::exception& e) {
            std::cerr << "Fatal error: " << e.what() << std::endl;
            return EXIT_FAILURE;
        }
    
        return 0;
}
