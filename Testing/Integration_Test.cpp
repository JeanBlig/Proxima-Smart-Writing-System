#include <cppunit/extensions/HelperMacros.h>

#include <filesystem>
#include <string>
#include <windows.h>

#include "../AppLayer/AppLayer.h"
#include "../Core/Tesseract-OCR/Test_Tesseract.h"
#include "../WindowAPI/Window.h"

namespace {
    class ScreenshotToOCRIntegrationTest : public CppUnit::TestFixture {
        CPPUNIT_TEST_SUITE(ScreenshotToOCRIntegrationTest);
        CPPUNIT_TEST(ScreenshotToOCR);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override
        {
            originalBmpPath = AppLayer::bmpPath;
            testBmpPath = (std::filesystem::temp_directory_path() / "proxima_integration_screenshot.bmp").string();
            std::filesystem::remove(testBmpPath);

            AppLayer::bmpPath = testBmpPath; // for testing the same handoff behavior
            AppLayer::Prompting = false;
            AppLayer::PromptInProgress = false;
        }

        void tearDown() override
        {
            AppLayer::bmpPath = originalBmpPath;
            std::filesystem::remove(testBmpPath);
        }

        void ScreenshotToOCR()
        {
            WindowAPI::window window; // runs a window
            PumpMessages(window);

            HWND hwndMain = FindWindowW(L"Sample Window Class", L"Proxima Whiteboard"); // was the window created
            CPPUNIT_ASSERT_MESSAGE("Main window was not created.", hwndMain != nullptr);

            HWND hwndPromptButton = GetDlgItem(hwndMain, 3); // was the prompt button created
            CPPUNIT_ASSERT_MESSAGE("Prompt button was not created.", hwndPromptButton != nullptr);

            SendMessageW(hwndPromptButton, BM_CLICK, 0, 0);
            PumpMessages(window);

            CPPUNIT_ASSERT_MESSAGE(
                "TakeScreenshot did not write the expected BMP file.",
                std::filesystem::exists(testBmpPath) // Is there a bmp at this path
            );

            const std::string ocrResult = PerformOCR(AppLayer::bmpPath);
            CPPUNIT_ASSERT_MESSAGE(
                "PerformOCR could not open the BMP produced by TakeScreenshot.", // If there is no bmp or it is broken OCR func will fail
                ocrResult != "ERROR: Could not load image!"
            );
        }

    private:
        static void PumpMessages(WindowAPI::window& window)
        {
            for (int i = 0; i < 10; ++i) {
                window.ProcessMessages();
                Sleep(10);
            }
        }

        std::string originalBmpPath;
        std::string testBmpPath;
    };
}

CPPUNIT_TEST_SUITE_REGISTRATION(ScreenshotToOCRIntegrationTest);
