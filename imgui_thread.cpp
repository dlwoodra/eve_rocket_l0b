//This deals with running the demo. Main runs this in a separate thread.

#include "eve_l0b.hpp"
#include "commonFunctions.hpp"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include "implot.h"
#include "eve_esp_x_angle.h"
#include "eve_esp_y_angle.h"
#include <stdio.h>
#include <vector>
#include <iostream>
#include <string>
#include <filesystem>

#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif

#include <GLFW/glfw3.h> 

extern ProgramState globalState;
extern std::mutex mtx;

int Image_Display_Scale_MA = 0;
int Image_Display_Scale_MB = 0;
const char* Image_Display_Scale_Items[] = { "Mod 256", "Full Scale", "HistEqual" };

enum LimitState {
    NoCheck,
    Green,
    Yellow,
    Red
};

// ImVec4 GetAdaptiveColor(ImVec4 color) {
//     ImVec4 windowBg = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
//     float brightnessFactor = (windowBg.x + windowBg.y + windowBg.z) / 3.0f; // Average brightness of the background

//     // Adjust brightness of the color based on the background color
//     return ImVec4(color.x * brightnessFactor, color.y * brightnessFactor, color.z * brightnessFactor, color.w);
// }

// Function to determine the color based on state
ImVec4 getColorForState(LimitState state) {
    switch (state) {
        //case Green: return GetAdaptiveColor(ImVec4(0.0f, 1.0f, 0.0f, 1.0f));  // Green
        //case Yellow: return GetAdaptiveColor(ImVec4(1.0f, 0.8f, 0.0f, 1.0f)); // Yellow
        //case Red: return GetAdaptiveColor(ImVec4(1.0f, 0.0f, 0.0f, 1.0f));    // Red
        case Green: return ImVec4(0.0f, 0.5f, 0.0f, 1.0f);  // Green
        case Yellow: return ImVec4(0.4f, 0.4f, 0.0f, 1.0f); // Yellow
        case Red: return ImVec4(0.5f, 0.0f, 0.0f, 1.0f);    // Red
        default: return ImGui::GetStyleColorVec4(ImGuiCol_FrameBg); // Default color
    }
}

float mazoom = 0.2f;         // Zoom level (1.0 = full resolution)
float mbzoom = 0.2f;         // Zoom level (1.0 = full resolution)


// Function to populate the image with an asymetric pattern, 4 quadrants, lowest quad is a gradian, second is a checkerboard, third is a constant value, fourth is a sinusoidal pattern
// topleft is gradient, topright is checkerboard, bottomleft is 128, bottom right is sine wave
void populatePattern(uint16_t image[MEGS_IMAGE_WIDTH][MEGS_IMAGE_HEIGHT]) {
    // Define the quadrant boundaries
    const int midX = MEGS_IMAGE_WIDTH / 2;
    const int midY = MEGS_IMAGE_HEIGHT / 2;

    for (uint32_t y = 0; y < MEGS_IMAGE_HEIGHT; ++y) {
        for (uint32_t x = 0; x < MEGS_IMAGE_WIDTH; ++x) {
            if (x < midX && y < midY) {
                // First quadrant: gradient from 0 to 255
                image[x][y] = static_cast<uint16_t>((x / static_cast<float>(midX)) * 255);
            } else if (x >= midX && y < midY) {
                // Second quadrant: checkerboard pattern
                image[x][y] = static_cast<uint16_t>((x / 256 + y / 256) % 2 == 0 ? 255 : 0);
            } else if (x < midX && y >= midY) {
                // Third quadrant: constant value
                image[x][y] = 128;  // Constant gray value
            } else {
                // Fourth quadrant: sinusoidal pattern
                image[x][y] = static_cast<uint16_t>(128 + 127 * std::sin((y / static_cast<float>(MEGS_IMAGE_HEIGHT)) * 2 * M_PI));
            }
        }
    }
}

void histogramEqualization(uint16_t (*image)[MEGS_IMAGE_HEIGHT][MEGS_IMAGE_WIDTH], std::vector<uint8_t>& textureData) {
    constexpr uint32_t halfHeight = MEGS_IMAGE_HEIGHT / 2;
    constexpr uint32_t topHalfPixels = MEGS_IMAGE_WIDTH * halfHeight;
    constexpr uint32_t bottomHalfPixels = MEGS_IMAGE_WIDTH * halfHeight;

    // brace initialization for zeroing out the arrays is the fastest way to initialize a large array to zero
    // since the compiler can optimize this to either a memset or rep stosd call
    int topHistogram[16384] = {0};
    int bottomHistogram[16384] = {0};

    // Step 1: Calculate the histogram for the top and bottom halves
    // use all threads available to calculate the histogram

    // #pragma omp parallel for
    // for (uint32_t y = 0; y < MEGS_IMAGE_HEIGHT; ++y) {
    //     // Determine whether to add to topHistogram or bottomHistogram
    //     int* currentHistogram = (y < halfHeight) ? topHistogram : bottomHistogram;

    //     for (uint32_t x = 0; x < MEGS_IMAGE_WIDTH; ++x) {
    //         uint16_t pixelValue = (*image)[y][x] & 0x3FFF;
    //         #pragma omp atomic
    //         currentHistogram[pixelValue]++;
    //     }
    // }

    // -faster code-
    #pragma omp parallel for
    for (uint32_t idx = 0; idx < MEGS_TOTAL_PIXELS; ++idx) {
        // Calculate x and y coordinates
        uint32_t y = idx / MEGS_IMAGE_WIDTH;
        uint32_t x = idx % MEGS_IMAGE_WIDTH;

        // Determine which histogram to use based on the row
        int* currentHistogram = (y < halfHeight) ? topHistogram : bottomHistogram;

        // Extract pixel value and increment the appropriate histogram
        uint16_t pixelValue = (*image)[y][x] & 0x3FFF;

        // Use atomic increment to avoid race conditions
        #pragma omp atomic
        currentHistogram[pixelValue]++;
    }

    // Step 2: Compute the cumulative distribution function (CDF) for each half
    int topCDF[16384] = {0};
    int bottomCDF[16384] = {0};

    topCDF[0] = topHistogram[0];
    bottomCDF[0] = bottomHistogram[0];

    for (int i = 1; i < 16384; ++i) {
        topCDF[i] = topCDF[i - 1] + topHistogram[i];
        bottomCDF[i] = bottomCDF[i - 1] + bottomHistogram[i];
    }

    // Step 3: Normalize the CDF for each half
    float top_cdf_min = *std::find_if(topCDF, topCDF + 16384, [](int value) { return value > 0; });
    float bottom_cdf_min = *std::find_if(bottomCDF, bottomCDF + 16384, [](int value) { return value > 0; });

    float top_cdf_range = topHalfPixels - top_cdf_min;
    float bottom_cdf_range = bottomHalfPixels - bottom_cdf_min;

    // Step 4: Scale the CDF to 0-255 and map to textureData
    float topScale = 255.0f / top_cdf_range;
    float bottomScale = 255.0f / bottom_cdf_range;

    // - faster code-
    #pragma omp parallel for
    for (uint32_t idx = 0; idx < MEGS_TOTAL_PIXELS; ++idx) {
        uint32_t y = idx / MEGS_IMAGE_WIDTH;  // Compute the row
        uint32_t x = idx % MEGS_IMAGE_WIDTH;  // Compute the column

        bool isTopHalf = (y < halfHeight);  // Determine if this pixel is in the top half

        // Access pixel and apply histogram equalization scaling
        int pixelValue = (*image)[y][x] & 0x3FFF;
        textureData[idx] = isTopHalf
            ? static_cast<uint8_t>(topScale * (topCDF[pixelValue] - top_cdf_min))
            : static_cast<uint8_t>(bottomScale * (bottomCDF[pixelValue] - bottom_cdf_min));
    }

}

void scaleImageToTexture(uint16_t (*megsImage)[MEGS_IMAGE_HEIGHT][MEGS_IMAGE_WIDTH], std::vector<uint8_t>& textureData, int Image_Display_Scale) {
    // Populate textureData directly from the 2D array
    if (Image_Display_Scale == 2) {
        histogramEqualization(megsImage, textureData);
    } else if (Image_Display_Scale == 1) {
        // Scale 14-bits to 8-bits

        #pragma omp parallel for
        for (uint32_t y = 0; y < MEGS_IMAGE_HEIGHT; ++y) {
            uint32_t rowIndex = y * MEGS_IMAGE_WIDTH; // Precompute row start index for 1D array
            for (uint32_t x = 0; x < MEGS_IMAGE_WIDTH; ++x) {
                textureData[rowIndex + x] = static_cast<uint8_t>(((*megsImage)[y][x] & 0x3FFF) >> 6);
            }
        }

    } else {
        // assume Image_Display_Scale == 0
        // Show LSB 8-bits

        #pragma omp parallel for
        for (uint32_t y = 0; y < MEGS_IMAGE_HEIGHT; ++y) {
            uint32_t rowIndex = y * MEGS_IMAGE_WIDTH; // Precompute row start index for 1D array
            for (uint32_t x = 0; x < MEGS_IMAGE_WIDTH; ++x) {
                textureData[rowIndex + x] = static_cast<uint8_t>((*megsImage)[y][x] & 0xFF);
            }
        }
    }
}

void renderInputTextWithColor(const char* label, long value, size_t bufferSize, bool limitCheck, float yHiLimit, float rHiLimit, float yLoLimit = -100., float rLoLimit = -200.) {
    LimitState state = NoCheck;
    
    float_t itemWidthValue = ImGui::GetFontSize() * 4;
    ImGui::PushItemWidth(itemWidthValue);

    char strval[bufferSize];
    snprintf(strval, bufferSize, "%ld", value);

    if (limitCheck) {
        const float fvalue = static_cast<float>(value); // fvalue is only used for limit checking
        if (fvalue > rHiLimit) {
            state = Red;
        } else if (fvalue > yHiLimit) { // Near violation (adjust the threshold as needed)
            state = Yellow;
        } else if (fvalue < rLoLimit) {
            state = Red;
        } else if (fvalue < yLoLimit) { // Near violation (adjust the threshold as needed)
            state = Yellow;
        } else {
            state = Green;
        }
    }

    // Set color before rendering
    ImGui::PushStyleColor(ImGuiCol_FrameBg, getColorForState(state));

    // Render the InputText
    ImGui::InputText(label, strval, bufferSize);

    // Restore default color
    ImGui::PopStyleColor();
}

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// initialize a texture for a MEGS image
GLuint createProperTextureFromMEGSImage(uint16_t (*data)[MEGS_IMAGE_HEIGHT][MEGS_IMAGE_WIDTH], int Image_Display_Scale) {
    std::vector<uint8_t> textureData(MEGS_TOTAL_PIXELS); // 8-bit data for display

    scaleImageToTexture(data, textureData, Image_Display_Scale);

    // Generate and bind a new texture
    GLuint textureID;
    glGenTextures(1, &textureID);        // Generate the texture ID
    glBindTexture(GL_TEXTURE_2D, textureID);  // Bind the texture

    // Set texture filtering parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Upload the texture data to OpenGL
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, MEGS_IMAGE_WIDTH, MEGS_IMAGE_HEIGHT, 0, GL_RED, GL_UNSIGNED_BYTE, textureData.data());

    // Unbind the texture
    glBindTexture(GL_TEXTURE_2D, 0);

    return textureID;  // Return the generated texture ID
}



//update the texture whenever MEGS-A changes
void renderUpdatedTextureFromMEGSAImage(GLuint textureID)
{
    int width=MEGS_IMAGE_WIDTH;
    int height=MEGS_IMAGE_HEIGHT;
    std::vector<uint8_t> textureData(width * height);

    scaleImageToTexture(&globalState.megsa.image, textureData, Image_Display_Scale_MA);

    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, MEGS_IMAGE_WIDTH, MEGS_IMAGE_HEIGHT, GL_RED, GL_UNSIGNED_BYTE, textureData.data());
    glBindTexture(GL_TEXTURE_2D, textureID);
}

void renderUpdatedTextureFromMEGSBImage(GLuint megsBTextureID)
{
    int width=MEGS_IMAGE_WIDTH;
    int height=MEGS_IMAGE_HEIGHT;
    std::vector<uint8_t> textureData(width * height);

    scaleImageToTexture(&globalState.megsb.image, textureData, Image_Display_Scale_MB);

    glBindTexture(GL_TEXTURE_2D, megsBTextureID);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, MEGS_IMAGE_WIDTH, MEGS_IMAGE_HEIGHT, GL_RED, GL_UNSIGNED_BYTE, textureData.data());
    glBindTexture(GL_TEXTURE_2D, megsBTextureID);
}


void displayMAImageWithControls(GLuint megsATextureID)
{
    ImGui::Begin("MEGS-A Image Viewer");
    
    // Zoom slider
    ImGui::SetNextItemWidth(120);
    ImGui::InputFloat("MA Zoom", &mazoom, 0.2f, 0.2f, "%.2f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::Combo("Scaletype", &Image_Display_Scale_MA, Image_Display_Scale_Items, IM_ARRAYSIZE(Image_Display_Scale_Items));

    mtx.lock();
    std::string iso8601 = tai_to_iso8601(globalState.megsa.tai_time_seconds);
    mtx.unlock();

    ImGui::Text("%s",iso8601.c_str());

    mtx.lock();
    int32_t yPosHi = globalState.MAypos;
    mtx.unlock();

    int32_t yPosLo = 1024-yPosHi;
    ImGui::BeginGroup();
    {
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.0f, 1.0f, 0.0f, 0.5f)); // Green
        ImGui::VSliderInt("##MAY1", ImVec2(30,MEGS_IMAGE_HEIGHT*mazoom*0.5), &yPosHi, 512, 1023);
        ImGui::PopStyleColor();
        // remove spacing between sliders
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetStyle().ItemSpacing.y);
        // Set the color for the second slider
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.0f, 0.0f, 1.0f, 1.0f)); // Blue
        ImGui::VSliderInt("##MAY2", ImVec2(30,MEGS_IMAGE_HEIGHT*(mazoom)*0.5), &yPosLo, 0, 511);
        ImGui::PopStyleColor();
    }
    ImGui::EndGroup();
    ImGui::SameLine();

    float value = 1.0f; // changing this will crop the image
    ImGui::Image((void*)(intptr_t)megsATextureID, ImVec2(MEGS_IMAGE_WIDTH*mazoom,MEGS_IMAGE_HEIGHT*mazoom), ImVec2(0.0f,0.0f), ImVec2(value,value));

    // dislpay the value of one pixel from each half
    uint16_t hiRowValues[MEGS_IMAGE_WIDTH];
    uint16_t lowRowValues[MEGS_IMAGE_WIDTH];
    uint16_t maxValue = 0;
    uint16_t minValue = 0xFFFF;

    mtx.lock();
    for (uint32_t x = 0; x < MEGS_IMAGE_WIDTH; ++x) {
        hiRowValues[x] = globalState.megsa.image[yPosHi+1][x];
        lowRowValues[x] = globalState.megsa.image[yPosLo-1][x];
        if (hiRowValues[x] > maxValue) maxValue = hiRowValues[x];
        if (hiRowValues[x] < minValue) minValue = hiRowValues[x];
        if (lowRowValues[x] > maxValue) maxValue =lowRowValues[x];
        if (lowRowValues[x] < minValue) minValue = lowRowValues[x];   
    }
    mtx.unlock();

    //std::cout<< "Minvalue: " << minValue << " Maxvalue: " << maxValue << std::endl;
    ImGui::End();

    {
        ImGui::Begin("MA Row Plots");
        ImPlot::SetNextAxesToFit();
        ImPlot::BeginPlot("Pixel Values", ImVec2(450, 180));
        std::string label = "Row "+std::to_string(yPosHi+1);
        ImPlot::PlotLine(label.c_str(), hiRowValues, MEGS_IMAGE_WIDTH);
        label = "Row "+std::to_string(yPosLo-1);
        ImPlot::PlotLine(label.c_str(), lowRowValues, MEGS_IMAGE_WIDTH);
        ImPlot::EndPlot();
        ImGui::End();
    }

}

void displayMBImageWithControls(GLuint megsBTextureID)
{
    ImGui::Begin("MEGS-B Image Viewer");

    // Zoom slider
    ImGui::SetNextItemWidth(120);
    //ImGui::SliderFloat("MB Zoom", &mbzoom, 0.25f, 1.0f, "MB Zoom %.1fx");
    ImGui::InputFloat("MB Zoom", &mbzoom, 0.2f, 0.2f, "%.2f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::Combo("Scaletype", &Image_Display_Scale_MB, Image_Display_Scale_Items, IM_ARRAYSIZE(Image_Display_Scale_Items));

    mtx.lock();
    std::string iso8601 = tai_to_iso8601(globalState.megsb.tai_time_seconds);
    mtx.unlock();

    ImGui::Text("%s",iso8601.c_str());

    mtx.lock();
    int32_t yPosHi = globalState.MBypos;
    mtx.unlock();

    int32_t yPosLo = 1024-yPosHi;
    ImGui::BeginGroup();
    {
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.0f, 1.0f, 0.0f, 0.5f)); // Green
        ImGui::VSliderInt("##MBY1", ImVec2(30,MEGS_IMAGE_HEIGHT*mbzoom*0.5), &yPosHi, 512, 1023);
        ImGui::PopStyleColor();
        // remove spacing between sliders
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetStyle().ItemSpacing.y);
        // Set the color for the second slider
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.0f, 0.0f, 1.0f, 1.0f)); // Blue
        ImGui::VSliderInt("##MBY2", ImVec2(30,MEGS_IMAGE_HEIGHT*(mbzoom)*0.5), &yPosLo, 0, 511);
        ImGui::PopStyleColor();
    }
    ImGui::EndGroup();
    ImGui::SameLine();

    float value = 1.0f; // changing this will crop the image
    ImGui::Image((void*)(intptr_t)megsBTextureID, ImVec2(MEGS_IMAGE_WIDTH*mbzoom,MEGS_IMAGE_HEIGHT*mbzoom), ImVec2(0.0f,0.0f), ImVec2(value,value));

    ImGui::End();

    // dislpay the value of one pixel from each half
    uint16_t hiRowValues[MEGS_IMAGE_WIDTH];
    uint16_t lowRowValues[MEGS_IMAGE_WIDTH];
    uint16_t maxValue = 0;
    uint16_t minValue = 0xFFFF;
    mtx.lock();
    for (uint32_t x = 0; x < MEGS_IMAGE_WIDTH; ++x) {
        hiRowValues[x] = globalState.megsb.image[yPosHi+1][x];
        lowRowValues[x] = globalState.megsb.image[yPosLo-1][x];
        if (hiRowValues[x] > maxValue) maxValue = hiRowValues[x];
        if (hiRowValues[x] < minValue) minValue = hiRowValues[x];
        if (lowRowValues[x] > maxValue) maxValue =lowRowValues[x];
        if (lowRowValues[x] < minValue) minValue = lowRowValues[x];   
    }
    mtx.unlock();

    //std::cout<< "Minvalue: " << minValue << " Maxvalue: " << maxValue << std::endl;

    {
        ImGui::Begin("MB Row Plots");
        ImPlot::SetNextAxesToFit();
        ImPlot::BeginPlot("Pixel Values", ImVec2(450, 180));
        std::string label = "Row "+std::to_string(yPosHi+1);
        ImPlot::PlotLine(label.c_str(), hiRowValues, MEGS_IMAGE_WIDTH);
        label = "Row "+std::to_string(yPosLo-1);
        ImPlot::PlotLine(label.c_str(), lowRowValues, MEGS_IMAGE_WIDTH);
        ImPlot::EndPlot();
        ImGui::End();
    }

}


void displaySimpleMB(GLuint textureID) {
    ImGui::Image((ImTextureID)(intptr_t)textureID, ImVec2(2048.0f, 1024.0f), ImVec2(0, 0), ImVec2(1, 1));
}

// 
void renderSimpleTextureMB(GLuint textureID, const uint16_t (*image)[MEGS_IMAGE_HEIGHT]) {
    // Prepare 8-bit texture data (resize only if necessary)
    int width=MEGS_IMAGE_WIDTH;
    int height=MEGS_IMAGE_HEIGHT;
    std::vector<uint8_t> textureData(width * height);

    // Populate textureData directly from the 2D array
    for (int x = 0; x < width; ++x) {
        for (int y = 0; y < height; ++y) {
            int index = y * width + x; // 1D index in textureData
            uint16_t value = image[x][y]; // Fetch pixel value

            // Example scaling to fit into 8-bit texture (adjust logic as needed)
            textureData[index] = static_cast<uint8_t>(value & 0xFF);
        }
    }

    // Bind the texture
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Update the texture with the new data
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, textureData.data());

    // Unbind the texture
    glBindTexture(GL_TEXTURE_2D, 0);
}

void updateStatusWindow()
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("Refresh rate: %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
    renderInputTextWithColor("601 a59 MEGS-A Pkts", globalState.packetsReceived.MA, 12, false, 0.0, 0.9);
    renderInputTextWithColor("602 a5a MEGS-B Pkts", globalState.packetsReceived.MB, 12, false, 0.0, 0.9);
    renderInputTextWithColor("604 a5c ESP Pkts", globalState.packetsReceived.ESP, 12, false, 0.0, 0.9);
    renderInputTextWithColor("605 a5d MEGS-P Pkts", globalState.packetsReceived.MP, 12, false, 0.0, 0.9);
    renderInputTextWithColor("606 a5e SHK Pkts",globalState.packetsReceived.SHK, 12, false, 0.0, 0.9);
    renderInputTextWithColor("Unknown Packets", globalState.packetsReceived.Unknown, 12, true, 0.0, 0.9);

    renderInputTextWithColor("MEGS-A Gap Count", globalState.dataGapsMA, 12, !globalState.isFirstMAImage, 0.0, 0.9);
    renderInputTextWithColor("MEGS-B Gap Count", globalState.dataGapsMB, 12, !globalState.isFirstMBImage, 0.0, 0.9);
    renderInputTextWithColor("MEGS-P Gap Count", globalState.dataGapsMP, 12, true, 0.0, 0.9);
    renderInputTextWithColor("ESP Gap Count", globalState.dataGapsESP, 12, true, 0.0, 0.9);
    renderInputTextWithColor("SHK Gap Count", globalState.dataGapsSHK, 12, true, 0.0, 0.9);

    renderInputTextWithColor("MEGS-A Parity Errors", globalState.parityErrorsMA, 12, true, 0.0, 0.9);
    renderInputTextWithColor("MEGS-B Parity Errors", globalState.parityErrorsMB, 12, true, 0.0, 0.9);

    renderInputTextWithColor("MEGS-A Top Saturated", globalState.saturatedPixelsMATop, 12, true, 0.0, 0.9);
    renderInputTextWithColor("MEGS-A Bottom Saturated", globalState.saturatedPixelsMABottom, 12, true, 0.0, 0.9);
    renderInputTextWithColor("MEGS-B Top Saturated", globalState.saturatedPixelsMBTop, 12, true, 0.0, 0.9);
    renderInputTextWithColor("MEGS-B Bottom Saturated", globalState.saturatedPixelsMBBottom, 12, true, 0.0, 0.9);
}

void plotESPTarget(int lastIdx) {
    constexpr float twoPi = 2.0f * 3.1415926535f;

    // Calculate the angles from the quad diodes, this is almost a direct reuse of the code from the flight L0B code
    float qsum = globalState.esp.ESP_q0[lastIdx] + globalState.esp.ESP_q1[lastIdx] + globalState.esp.ESP_q2[lastIdx] + globalState.esp.ESP_q3[lastIdx];
	float inv_qsum = 1.f / (qsum + (1.e-10));
	float qX = ((globalState.esp.ESP_q1[lastIdx] + globalState.esp.ESP_q3[lastIdx]) - (globalState.esp.ESP_q0[lastIdx] + globalState.esp.ESP_q2[lastIdx])) * inv_qsum ;
	float qY = ((globalState.esp.ESP_q0[lastIdx] + globalState.esp.ESP_q1[lastIdx]) - (globalState.esp.ESP_q2[lastIdx] + globalState.esp.ESP_q3[lastIdx])) * inv_qsum;
    float maxAbsNorm = (abs(qX) > abs(qY)) ? abs(qX) : abs(qY);

    int arcsecX = ((int) ((qX - 2.0) * 1000)) + 1000;
	int arcsecY = ((int) ((qY - 2.0) * 1000)) + 1000;
    float xanglearcsec = (esp_x_angle_table[arcsecX] / 1000.0);
    float yanglearcsec = (esp_y_angle_table[arcsecY] / 1000.0);
    //float xangleDeg = xanglearcsec / 3600.0;
    //float yangleDeg = yanglearcsec / 3600.0;

    // Create a plot with ImPlot
    //if (ImPlot::BeginPlot("##ESP Target Plot", ImVec2(-1, 0), ImPlotFlags_Equal)) {
    if (ImPlot::BeginPlot("##ESP Target Plot", ImVec2(200, 200), ImPlotFlags_Equal)) {
        // Set the axis ranges to be from -1 to +1
        float maxScale = (maxAbsNorm > 1.0) ? maxAbsNorm * 1.5 : 1.0;
        ImPlot::SetupAxisLimits(ImAxis_X1, -maxScale, maxScale);
        ImPlot::SetupAxisLimits(ImAxis_Y1, -maxScale, maxScale);

        // Radii for the open circles
        const float radii[] = {0.01f, 0.05f, 0.1f, 0.5f, 1.0f};

        // Draw the circles centered at (0, 0)
        for (float radius : radii) {
            // Generate points for the circle
            const int points = 30;
            // Use std::vector for the circle points
            std::vector<float> xData(points);
            std::vector<float> yData(points);


            for (int i = 0; i < points; ++i) {
                float angle = twoPi * (float)i / (float)(points - 1);
                xData[i] = radius * cos(angle);
                yData[i] = radius * sin(angle);
            }
            // Plot the circle as a line
            ImPlot::PlotLine("##Circle", xData.data(), yData.data(), points);
        }

        // Plot the data point (as an asterisk)
        ImPlot::SetNextMarkerStyle(ImPlotMarker_Asterisk, 8.0f, ImVec4(1, 0, 0, 1), 1.5f);
        
        ImPlot::PlotScatter("Measurement", &qX, &qY, 1);
 
        // Display the angles below the plot
        ImGui::Text("X Angle: %.2f arcsec", xanglearcsec);
        ImGui::Text("Y Angle: %.2f arcsec", yanglearcsec);
        renderInputTextWithColor("Qsum:", qsum, 12, true, 100000.f, 200000.f, 100.f, 50.f);
 
        // End the plot
        ImPlot::EndPlot();
    }
}

void updateESPWindow()
{
    // need to find the last populated index
    int index = ESP_INTEGRATIONS_PER_FILE - 1;
    while ((index > 1) && (globalState.esp.ESP_xfer_cnt[index] == 0)) 
    {
        index--;
    }

    // Column 1
    ImGui::Columns(3,"ESP Data");
    //ImGui::SetColumnWidth(0, 210.0f);
    //ImGui::Text("ESP Status Column");
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10);

    std::string iso8601 = tai_to_iso8601(globalState.esp.tai_time_seconds);
    //const char* tmpiISO8601 = iso8601.c_str();
    //ImGui::Text("pkt:%s", tmpiISO8601);
    std::string newiso8601 = tai_to_iso8601sss( iso8601, globalState.esp.tai_time_subseconds);
    const char* tmpiISO8601sss = newiso8601.c_str(); 
    ImGui::Text("pkt:%s", tmpiISO8601sss);

    renderInputTextWithColor("ESP xfer cnt", globalState.esp.ESP_xfer_cnt[index], 12, false, 0.0, 0.9);
    renderInputTextWithColor("ESP q0", globalState.esp.ESP_q0[index], 12, false, 0.0, 0.9);
    renderInputTextWithColor("ESP q1", globalState.esp.ESP_q1[index], 12, false, 0.0, 0.9);
    renderInputTextWithColor("ESP q2", globalState.esp.ESP_q2[index], 12, false, 0.0, 0.9);
    renderInputTextWithColor("ESP q3", globalState.esp.ESP_q3[index], 12, false, 0.0, 0.9);
    renderInputTextWithColor("ESP 171", globalState.esp.ESP_171[index], 12, false, 0.0, 0.9);
    renderInputTextWithColor("ESP 257", globalState.esp.ESP_257[index], 12, false, 0.0, 0.9);
    renderInputTextWithColor("ESP 304", globalState.esp.ESP_304[index], 12, false, 0.0, 0.9);
    renderInputTextWithColor("ESP 366", globalState.esp.ESP_366[index], 12, false, 0.0, 0.9);
    renderInputTextWithColor("ESP dark", globalState.esp.ESP_dark[index], 12, false, 0.0, 0.9);

    renderInputTextWithColor("MP Ly-a", globalState.megsp.MP_lya[index], 12, false, 0.0, 0.9);
    renderInputTextWithColor("MP dark", globalState.megsp.MP_dark[index], 12, false, 0.0, 0.9);

    // Column 2
    ImGui::NextColumn();
    //ImGui::Text("ESP Plots Column");

    // Plot the ESP data
    plotESPTarget(index);

    // Column 3
    ImGui::NextColumn();
    ImPlot::SetNextAxesToFit();
    ImPlot::BeginPlot("##Quads", ImVec2(300, 180));
    ImPlot::PlotLine("ESP q0", globalState.esp.ESP_q0, ESP_INTEGRATIONS_PER_FILE);
    ImPlot::PlotLine("ESP q1", globalState.esp.ESP_q1, ESP_INTEGRATIONS_PER_FILE);
    ImPlot::PlotLine("ESP q2", globalState.esp.ESP_q2, ESP_INTEGRATIONS_PER_FILE);
    ImPlot::PlotLine("ESP q3", globalState.esp.ESP_q3, ESP_INTEGRATIONS_PER_FILE);
    ImPlot::EndPlot();
    ImPlot::SetNextAxesToFit();
    ImPlot::BeginPlot("##Others", ImVec2(300, 180));
    ImPlot::PlotLine("ESP 17", globalState.esp.ESP_171, ESP_INTEGRATIONS_PER_FILE);
    ImPlot::PlotLine("ESP 25", globalState.esp.ESP_257, ESP_INTEGRATIONS_PER_FILE);
    ImPlot::PlotLine("ESP 30", globalState.esp.ESP_304, ESP_INTEGRATIONS_PER_FILE);
    ImPlot::PlotLine("ESP 36", globalState.esp.ESP_366, ESP_INTEGRATIONS_PER_FILE);
    ImPlot::PlotLine("ESP dark", globalState.esp.ESP_dark, ESP_INTEGRATIONS_PER_FILE);
    ImPlot::EndPlot();

    // reset to single column layout
    ImGui::Columns(1);

}

void displayFPGAStatus() {

    // Column 1
    ImGui::Columns(2,"FPGA ");
    ImGui::SetColumnWidth(0, 130.0f);
    ImGui::Text("FPGA Registers");

    mtx.lock();
    uint16_t reg0 = globalState.FPGA_reg0;
    uint16_t reg1 = globalState.FPGA_reg1;
    uint16_t reg2 = globalState.FPGA_reg2;
    uint16_t reg3 = globalState.FPGA_reg3;
    uint32_t totalReadCounter = globalState.totalReadCounter;
    uint32_t readsPerSecond = globalState.readsPerSecond;
    uint32_t packetsPerSecond = globalState.packetsPerSecond;
    uint32_t shortPacketCounter = globalState.shortPacketCounter;
    mtx.unlock();

    renderInputTextWithColor("Reg 0", reg0, 6, false, 0.0, 0.9);
    renderInputTextWithColor("Reg 1", reg1, 6, false, 0.0, 0.9);
    renderInputTextWithColor("Reg 2", reg2, 6, false, 0.0, 0.9);
    renderInputTextWithColor("Reg 3", reg3, 6, false, 0.0, 0.9);
    renderInputTextWithColor("Read Cnt", totalReadCounter, 6, false, 0.0, 0.9);
    renderInputTextWithColor("Reads/s", readsPerSecond, 6, false, 0.0, 0.9);
    renderInputTextWithColor("USB MB/s", readsPerSecond*65536.0f, 6, true, 7.0, 10.0f, 0.1, 0.01);
    renderInputTextWithColor("pkt/s", packetsPerSecond, 6, true, 481.0, 500.0f, 2.9f, 1.9f);
    renderInputTextWithColor("short pkts", shortPacketCounter, 6, true, 1.0, 2.0f);

    ImGui::NextColumn();
    ImGui::Text("FPGA Interpreted Values");
    //reg0 is multiple status bits
    renderInputTextWithColor("FIFO TxEmpty", (reg0) & 0x01, 12, false, 0.0, 0.9);
    renderInputTextWithColor("FIFO RxEmpty", (reg0 >> 1) & 0x01, 12, true, 0.9, 1.9);
    renderInputTextWithColor("FIFO RxErr", (reg0 >> 2) & 0x01, 12, true, 0.0, 0.9);
    //reg1 is version
    renderInputTextWithColor("Firmware Ver", reg1, 12, false, 0.0, 0.9);
    int gseType=reg1>>12;
    switch (gseType) {
        case 1:
            ImGui::Text("GSEType: SyncSerial");
            break;
        case 2:
            ImGui::Text("GSEType: SpaceWire");
            break;
        case 3:
            ImGui::Text("GSEType: Parallel");
            break;
        default: 
            ImGui::Text("GSEType: Unknown");
    }
    //reg2 is overflow
    renderInputTextWithColor("FIFO Overflow", reg2, 12, true, 0, 0.9f);
    //reg3 is temperature
    float temperature = (reg3 >> 4) * 503.975f / 4096.0f - 273.15f;
    renderInputTextWithColor("FPGA Temp (C)", temperature, 12, true, 40.0f, 45.0f, 20.0f, 10.0f);


}



int imgui_thread() {

    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char* glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    // Create window with graphics context
    GLFWwindow* window = glfwCreateWindow(1200, 800, "Dear ImGui GLFW+OpenGL3 example", nullptr, nullptr);
    if (window == nullptr)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
    //io.ConfigViewportsNoAutoMerge = true;
    //io.ConfigViewportsNoTaskBarIcon = true;


    // Preload fonts into ImGui's font atlas (this should be done outside the rendering loop)
    ImFont* pFont = io.Fonts->AddFontFromFileTTF("./fonts/DejaVuSans.ttf", 16.0f);
    bool isFontLoaded = false;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    ImGui::CreateContext();
    // Setup ImPlot
    ImPlot::CreateContext();




    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
#ifdef __EMSCRIPTEN__
    ImGui_ImplGlfw_InstallEmscriptenCallbacks(window, "#canvas");
#endif
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Demo loads fonts here

    // Our state
    bool show_demo_window = false; //true;
    //bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    
    //std::lock_guard<std::mutex> lock(mtx); // lock the mutex
    mtx.lock();
    GLuint megsATextureID = createProperTextureFromMEGSImage(&globalState.megsa.image, Image_Display_Scale_MA);
    GLuint megsBTextureID = createProperTextureFromMEGSImage(&globalState.megsb.image, Image_Display_Scale_MB);
    mtx.unlock(); // unlock the mutex

    // Test image for verify orientation
    // uint16_t testimg[MEGS_IMAGE_WIDTH][MEGS_IMAGE_HEIGHT];
    // populatePattern(testimg);
    // for (int x = 0; x < 10; ++x) { // verify there is nonzero data in the testimg
    //     for (int y = 0; y < 10; ++y) {
    //         std::cout << testimg[x][y] << " "; // Print first 10x10 pixels
    //     }
    //     std::cout << std::endl;
    // }
    // GLuint mbSimpleTextureID = createProperTextureFromMEGSImage(testimg, MEGS_IMAGE_WIDTH, MEGS_IMAGE_HEIGHT, true, true);

    // Main loop
#ifdef __EMSCRIPTEN__
    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.IniFilename = nullptr;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while ((!glfwWindowShouldClose(window)) && (globalState.running))
#endif
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)
        {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        if (!isFontLoaded) {
            ImGui::PushFont(pFont);
            isFontLoaded = true;
        }

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {
            //static int counter = 0;

            ImGui::Begin("SDO-EVE Rocket FPGA Status");                          // Create a window called "Hello, world!" and append into it.
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            displayFPGAStatus();
            ImGui::End();
        }

        {
            //mtx.lock();
            displayMAImageWithControls(megsATextureID);
            displayMBImageWithControls(megsBTextureID);
            //mtx.unlock();

            // 3. Show another simple window.
            {
                ImGui::Begin("Channel Status");
                mtx.lock();
                updateStatusWindow();
                mtx.unlock();
                ImGui::End();
            }

            {
                ImGui::Begin("ESP Window");
                mtx.lock();
                updateESPWindow();
                mtx.unlock();
                ImGui::End();
            }

            // debugging image display
            // {
            //     ImGui::Begin("MEGS-B Image Simple Viewer");
            //     ImGui::Text("MEGS-B Image Simple Text");
            //     mtx.lock();
            //     displaySimpleMB(mbSimpleTextureID);
            //     mtx.unlock();
            //     ImGui::End();
            // }
        }
        if (isFontLoaded) {
            ImGui::PopFont();
            isFontLoaded = false;
        }


        // Rendering
        ImGui::Render();
        int display_w, display_h;

        // this is the part that draws stuff to the screen
        {
            mtx.lock();
            if (globalState.megsAUpdated) {
                renderUpdatedTextureFromMEGSAImage(megsATextureID);
                globalState.megsAUpdated = false;  // Reset flag after updating texture
            }
            mtx.unlock();

            mtx.lock();
            if (globalState.megsBUpdated) {
                renderUpdatedTextureFromMEGSBImage(megsBTextureID);
                // debugging image display
                // renderSimpleTextureMB(mbSimpleTextureID, testimg);
                globalState.megsBUpdated = false;  // Reset flag after updating texture
            }
            mtx.unlock();

        }

        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows
        // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
        //  For this specific demo app we could also call glfwMakeContextCurrent(window) directly)
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        glfwSwapBuffers(window); // swap the buffer with the screen
    }

#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif

    // Cleanup
    ImPlot::DestroyContext();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteTextures(1, &megsATextureID);
    glDeleteTextures(1, &megsBTextureID);
    // debugging image display
    // glDeleteTextures(1, &mbSimpleTextureID);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
