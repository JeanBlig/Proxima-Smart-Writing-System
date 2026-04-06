#include <cppunit/extensions/HelperMacros.h>
#include <cctype>
#include <filesystem>
#include <string>
#include <iostream>
#include <string>
#include <algorithm> 

#include "../AppLayer/AppLayer.h"
#include "../Core/LLM/Llama.h"
#include "../Core/Tesseract-OCR/Test_Tesseract.h"


namespace {
    class Unit_Test : public CppUnit::TestFixture {
        CPPUNIT_TEST_SUITE(Unit_Test);
        CPPUNIT_TEST(PerformOCRTest);
        CPPUNIT_TEST(RunLlamaPromptTest);
        CPPUNIT_TEST_SUITE_END();

    public:
        void PerformOCRTest()
        {
            const std::filesystem::path repoRoot = ResolveRepoRoot();
            const std::filesystem::path testImagePath = repoRoot / "Testing" / "TestPrompt.bmp";
            CPPUNIT_ASSERT_MESSAGE(
                "TestPrompt.bmp was not found in the Testing directory.",
                std::filesystem::exists(testImagePath)
            );

            std::string ocrResult = PerformOCR(testImagePath.string());
            TrimTrailingWhitespace(ocrResult);

            // Convert OCR Result to all lower case
            std::transform(ocrResult.begin(), ocrResult.end(), ocrResult.begin(),

                   [](unsigned char c){ return std::tolower(c); });

            // Checking if OCR result is accurate
            CPPUNIT_ASSERT_MESSAGE("Expected OCR Result: vector multiplication \n Actual OCR Result: " + ocrResult, ocrResult == "vector multiplication");
        }

        void RunLlamaPromptTest()
        {
            CPPUNIT_ASSERT_MESSAGE(
                "Configured llama model file was not found.",
                std::filesystem::exists(AppLayer::model)
            );

            std::string llmResponse = RunLlamaPrompt(AppLayer::model, "Einstein E=mc2");
            TrimTrailingWhitespace(llmResponse);

            std::cout << llmResponse << "\n";

            CPPUNIT_ASSERT_MESSAGE(
                "RunLlamaPrompt returned an empty response.",
                !llmResponse.empty()
            );

            CPPUNIT_ASSERT_MESSAGE(
                "RunLlamaPrompt failed to load the configured model.",
                llmResponse.rfind("Failed to load model:", 0) != 0
            );

            CPPUNIT_ASSERT_MESSAGE(
                "RunLlamaPrompt failed while generating a response.",
                llmResponse != "Failed to create context." &&
                llmResponse != "Tokenization failed." &&
                llmResponse != "Decoding prompt failed."
            );
        }

    private:
        static std::filesystem::path ResolveRepoRoot()
        {
            const std::filesystem::path currentPath = std::filesystem::current_path();
            if (std::filesystem::exists(currentPath / "Testing" / "TestPrompt.bmp")) {
                return currentPath;
            }

            if (currentPath.filename() == "Dependencies" &&
                std::filesystem::exists(currentPath.parent_path() / "Testing" / "TestPrompt.bmp")) {
                return currentPath.parent_path();
            }

            return currentPath;
        }

        static void TrimTrailingWhitespace(std::string& value)
        {
            while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
                value.pop_back();
            }
        }
    };
}

CPPUNIT_TEST_SUITE_REGISTRATION(Unit_Test);