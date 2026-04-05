#include <cppunit/extensions/HelperMacros.h>

#include <cctype>
#include <filesystem>
#include <string>

#include "../Core/Tesseract-OCR/Test_Tesseract.h"

namespace {
    class Unit_Test : public CppUnit::TestFixture {
        CPPUNIT_TEST_SUITE(Unit_Test);
        CPPUNIT_TEST(PerformOCRTest);
        CPPUNIT_TEST_SUITE_END();

    public:
        void PerformOCRTest()
        {
            const std::filesystem::path testImagePath = std::filesystem::path("Testing") / "TestPrompt.bmp";
            CPPUNIT_ASSERT_MESSAGE(
                "TestPrompt.bmp was not found in the Testing directory.",
                std::filesystem::exists(testImagePath)
            );

            std::string ocrResult = PerformOCR(testImagePath.string());
            TrimTrailingWhitespace(ocrResult);

            CPPUNIT_ASSERT_EQUAL(std::string("vector multiplication"), ocrResult);
        }

    private:
        static void TrimTrailingWhitespace(std::string& value)
        {
            while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
                value.pop_back();
            }
        }
    };
}

CPPUNIT_TEST_SUITE_REGISTRATION(Unit_Test);



