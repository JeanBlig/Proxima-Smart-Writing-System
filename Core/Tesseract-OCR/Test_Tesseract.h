#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <string>

std::string PerformOCR(const std::string& imagePath) {
    tesseract::TessBaseAPI api;

    // 1) Initialize Tesseract
    api.Init("C:/Program Files/Tesseract-OCR/tessdata", "eng");

    // 2) Load image
    Pix* image = pixRead(imagePath.c_str());
    if (!image) {
        return "ERROR: Could not load image!";
    }

    // 3) Set image & extract text
    api.SetImage(image);
    char* outText = api.GetUTF8Text();

    std::string result = (outText ? std::string(outText) : "");

    // 4) Clean memory
    delete[] outText;
    pixDestroy(&image);
    api.End();

    return result;
}
/* Test function
int main() {
    std::string imagePath = "test_image.png"; // Replace with your image path
    std::string ocrResult = PerformOCR(imagePath);
    printf("OCR Result:\n%s\n", ocrResult.c_str());
    return 0;
}
*/