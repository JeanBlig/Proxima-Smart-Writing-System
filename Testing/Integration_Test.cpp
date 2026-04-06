#include <cppunit/extensions/HelperMacros.h>

#include <filesystem>
#include <string>
#include <windows.h>
#include "../AppLayer/AppLayer.h"
#include "../Core/Tesseract-OCR/Test_Tesseract.h"
#include "../WindowAPI/Window.h"

namespace {
    class Integration_Test : public CppUnit::TestFixture {
        CPPUNIT_TEST_SUITE(Integration_Test);
        CPPUNIT_TEST(ScreenshotToOCR);
        CPPUNIT_TEST(LLMtoTextBox);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override
        {
            originalBmpPath = AppLayer::bmpPath;
            testBmpPath = (std::filesystem::temp_directory_path() / "proxima_integration_screenshot.bmp").string();
            std::filesystem::remove(testBmpPath); // remove any file at that path before test

            AppLayer::bmpPath = testBmpPath; // for testing the same handoff behavior
            AppLayer::Prompting = false;
            AppLayer::PromptInProgress = false;
            AppLayer::LLM_Response.clear();
        }

        void tearDown() override
        {
            AppLayer::bmpPath = originalBmpPath;
            AppLayer::LLM_Response.clear();
            std::filesystem::remove(testBmpPath); // remove file after test
        }

        void ScreenshotToOCR()
        {
            WindowAPI::window window; // runs a window
            PumpMessages(window); // tells window to process start up window messages

            HWND hwndMain = FindWindowW(L"Sample Window Class", L"Proxima Whiteboard"); // was the window created
            CPPUNIT_ASSERT_MESSAGE("Main window was not created.", hwndMain != nullptr);

            HWND hwndPromptButton = GetDlgItem(hwndMain, 3); // create the pompt button ID 3 is ID_SCREENSHOT_BUTTON
            CPPUNIT_ASSERT_MESSAGE("Prompt button was not created.", hwndPromptButton != nullptr);

            SendMessageW(hwndPromptButton, BM_CLICK, 0, 0); // trigger the pompt button
            PumpMessages(window); // tells window to process current messages like prompt button

            CPPUNIT_ASSERT_MESSAGE(
                "TakeScreenshot did not write the expected BMP file.",
                std::filesystem::exists(testBmpPath) // Is there a bmp at this path
            );

            const std::string ocrResult = PerformOCR(AppLayer::bmpPath); // call ocr on bmp created by take screenshot
            CPPUNIT_ASSERT_MESSAGE(
                "PerformOCR could not open the BMP produced by TakeScreenshot.", // If there is no bmp or it is broken OCR func will fail
                ocrResult != "ERROR: Could not load image!"
            );
        }

        void LLMtoTextBox()
        {
            WindowAPI::window window;
            PumpMessages(window);

            HWND hwndMain = FindWindowW(L"Sample Window Class", L"Proxima Whiteboard");
            CPPUNIT_ASSERT_MESSAGE("Main window was not created.", hwndMain != nullptr);

            {
                std::lock_guard<std::mutex> lock(AppLayer::StateMutex);
                AppLayer::LLM_Response = "Integration test LLM response"; // Predifined response
            }

            WindowAPI::ShowResponseTextBox(); // create response text box. Should automatically pass LLM_Response in
            PumpMessages(window);

            HWND hwndTextBox = FindChildWindowByClass(hwndMain, L"Edit"); // copies UI text handle through helper function and checks if it exists
            CPPUNIT_ASSERT_MESSAGE("Response text box was not created.", hwndTextBox != nullptr);

            char buffer[512] = {}; // empty char C string  zero-initialized
            GetWindowTextA(hwndTextBox, buffer, sizeof(buffer)); // copy text from text box to buffer as ANSI

            CPPUNIT_ASSERT_EQUAL( // compares texts
                std::string("Integration test LLM response"),
                std::string(buffer) // converts to c++ string
            );
        }

    private:
        static void PumpMessages(WindowAPI::window& window) //Gives window time to create controls and process button clicks while the tests are running
                                                            
        {
            for (int i = 0; i < 10; ++i) {
                window.ProcessMessages();
                Sleep(10);
            }
        }

        static HWND FindChildWindowByClass(HWND hwndParent, const wchar_t* className) // small helper function to locate a child window. deals with "edit" text box
        {
            return FindWindowExW(hwndParent, nullptr, className, nullptr);
        }

        std::string originalBmpPath;
        std::string testBmpPath;
    };
}

CPPUNIT_TEST_SUITE_REGISTRATION(Integration_Test);
