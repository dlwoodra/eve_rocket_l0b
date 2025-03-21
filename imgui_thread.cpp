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
#include <deque>

#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif

#include <GLFW/glfw3.h> 

extern ProgramState globalState;
extern std::mutex mtx;

const char* Image_Display_Scale_Items[] = { "Mod 256", "Full Scale", "HistEqual" };

struct GlobalGUI
{
    int Image_Display_Scale_MA = 0;
    int Image_Display_Scale_MB = 0;
    float mazoom = 0.2f;
    float mbzoom = 0.2f;         // Zoom level (1.0 = full resolution)
    uint16_t MADark[MEGS_IMAGE_HEIGHT][MEGS_IMAGE_WIDTH] = {0};
    uint16_t MBDark[MEGS_IMAGE_HEIGHT][MEGS_IMAGE_WIDTH] = {0};
    uint16_t displayableMAImage[MEGS_IMAGE_HEIGHT][MEGS_IMAGE_WIDTH] = {0};
    uint16_t displayableMBImage[MEGS_IMAGE_HEIGHT][MEGS_IMAGE_WIDTH] = {0};
    bool removeMADark = false;
    bool removeMBDark = false;
    bool flipMAVertical = true; //03/21/25 set default to flip to be consistent with previous displays
    // display is desired to be flipped vertically
    // bottom left is slit 2, bottom right is SAM
    // top is slit 1
    bool flipMBVertical = true;
    bool flipMAHorizontal = false;
    bool flipMBHorizontal = false;
};

static GlobalGUI globalGUI;

ImPlotColormap selectedMAColormap = ImPlotColormap_Jet;
ImPlotColormap selectedMBColormap = ImPlotColormap_Jet;

static int customColorMapIdMA = -1;
static int customColorMapIdMB = -1;

constexpr uint16_t numColormaps = 18; // 16 default colormaps + 2 custom colormaps

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

void differenceAndClip(const uint16_t measurement[MEGS_IMAGE_HEIGHT][MEGS_IMAGE_WIDTH],
                       const uint16_t background[MEGS_IMAGE_HEIGHT][MEGS_IMAGE_WIDTH],
                       uint16_t result[MEGS_IMAGE_HEIGHT][MEGS_IMAGE_WIDTH]) {
    //#pragma omp parallel for
    for (size_t i = 0; i < MEGS_IMAGE_HEIGHT; ++i) {
        //#pragma omp simd
        for (size_t j = 0; j < MEGS_IMAGE_WIDTH; ++j) {
            int diff = measurement[i][j] - background[i][j];
            result[i][j] = diff > 0 ? diff : 0;
        }
    }
}

void addFilledCircleToTreeNode(LimitState state) {
    // Position elements on the same line
    ImGui::SameLine();

    // Get the position to place the circle after the text
    ImVec2 circlePos = ImGui::GetCursorScreenPos();

    // Adjust position for a more visually appealing alignment
    circlePos.x += 10;  // Slightly to the right of the TreeNode label
    circlePos.y += 8;   // Centered vertically with the text (adjust as needed)

    // Draw a filled circle inline with the text
    ImGui::GetWindowDrawList()->AddCircleFilled(circlePos, 8, ImGui::ColorConvertFloat4ToU32(getColorForState(state))); // colored circle
    ImGui::Dummy(ImVec2(0, 15));  // Move cursor down after circle, if adding more content below

}

void flipVertical(uint16_t image[1024][2048]) {
    for (int row = 0; row < 1024 / 2; ++row) {
        std::swap_ranges(image[row], image[row] + 2048, image[1023 - row]);
    }
}

void flipHorizontal(uint16_t image[1024][2048]) {
    for (int row = 0; row < 1024; ++row) {
        std::reverse(image[row], image[row] + 2048);
    }
}

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

std::vector<uint16_t> reorderCircularBuffer(const uint16_t* buffer, size_t size, size_t headIndex) {
    std::vector<uint16_t> ordered(size);

    // Copy elements from headIndex to the end of the buffer
    size_t count = 0;
    for (size_t i = headIndex; i < size; ++i) {
        ordered[count++] = buffer[i];
    }

    // Copy elements from the beginning of the buffer to headIndex
    for (size_t i = 0; i < headIndex; ++i) {
        ordered[count++] = buffer[i];
    }

    return ordered;
}

std::vector<ImVec4> InterpolateColormap(ImPlotColormap colormap) {
    int newSize = 256;
    const int colormapSize = ImPlot::GetColormapSize(colormap);
    std::vector<ImVec4> interpolatedColormap(newSize);

    for (int i = 0; i < newSize; ++i) {
        float t = static_cast<float>(i) / (newSize - 1); // Normalize to [0, 1]
        float colormapIndex = t * (colormapSize - 1);    // Scale to colormap range
        interpolatedColormap[i] = ImPlot::SampleColormap(colormapIndex / (colormapSize - 1), colormap);
    }

    return interpolatedColormap;
}

const char* GetColormapShortNameByIndex(uint16_t index) {
    const char* colormapNames[numColormaps] = {
        "Jet",
        "Cool",
        "Hot",
        "Spectral",
        "Plasma",
        "Viridis",
        "Paired",
        "Greys",
        "RdBu",
        "BrBG",
        "PiYG",
        "Deep",
        "Dark",
        "Pastel",
        "Pink",
        "Twilight",
        "RainbowCustomMA", "RainbowCustomMB"
    };
    return colormapNames[index];
}

const char* GetColormapNameByIndex(uint16_t index) {
    const std::vector<const char*> colormapNames = {
        "ImPlotColormap_Jet", // index 0
        "ImPlotColormap_Cool",    // index 1
        "ImPlotColormap_Hot",    // index 2
        "ImPlotColormap_Spectral",   // index 3
        "ImPlotColormap_Plasma",
        "ImPlotColormap_Viridis",
        "ImPlotColormap_Paired",
        "ImPlotColormap_Greys",
        "ImPlotColormap_RdBu",
        "ImPlotColormap_BrBG",
        "ImPlotColormap_PiYG",
        "ImPlotColormap_Deep",
        "ImPlotColormap_Dark",
        "ImPlotColormap_Pastel",
        "ImPlotColormap_Pink",
        "ImPlotColormap_Twilight",
        "RainbowCustomMA", "RainbowCustomMB"
    };
    if (index >= 0 && index < colormapNames.size()) {
        return colormapNames[index];
    }
    return "Unknown"; // Return a default value if index is out of range
}

ImPlotColormap GetColormapObjectByIndex(uint16_t index) {
    const std::vector<ImPlotColormap> colormaps = {
        ImPlotColormap_Jet,
        ImPlotColormap_Cool,
        ImPlotColormap_Hot,
        ImPlotColormap_Spectral,
        ImPlotColormap_Plasma,
        ImPlotColormap_Viridis,
        ImPlotColormap_Paired,
        ImPlotColormap_Greys,
        ImPlotColormap_RdBu,
        ImPlotColormap_BrBG,
        ImPlotColormap_PiYG,
        ImPlotColormap_Deep,
        ImPlotColormap_Dark,
        ImPlotColormap_Pastel,
        ImPlotColormap_Pink,
        ImPlotColormap_Twilight,
        customColorMapIdMA, customColorMapIdMB
    };
    if (index >= 0 && index < colormaps.size()) {
        return colormaps[index];
    }
    return ImPlotColormap_Jet; // Return a default value if index is out of range
}

// Modify colormap to replace the highest value with white, lowest value with black
void SetRainbowCustomColormap(bool isMA) {
    // init
    if ((isMA && (customColorMapIdMA == -1)) || (!isMA && (customColorMapIdMB == -1))) {

        std::vector<ImVec4> custom256 = InterpolateColormap(ImPlotColormap_Jet); // just pick Jet

        // Modify the first and last colors to be black and white
        custom256[0] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f); // Black
        custom256[255] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // White

        // Register the modified colormap
        if (isMA) {
            customColorMapIdMA = ImPlot::AddColormap("RainbowCustomMA", custom256.data(), custom256.size(), true);
        } else {    
            customColorMapIdMB = ImPlot::AddColormap("RainbowCustomMB", custom256.data(), custom256.size(), true);
        }
    }
    if (isMA) {
        ImPlot::PushColormap(customColorMapIdMA);
    } else {
        ImPlot::PushColormap(customColorMapIdMB);
    }

    // after using it,need to use ImPlot::PopColormap();
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
            #pragma omp simd
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
            #pragma omp simd
            for (uint32_t x = 0; x < MEGS_IMAGE_WIDTH; ++x) {
                textureData[rowIndex + x] = static_cast<uint8_t>((*megsImage)[y][x] & 0xFF);
            }
        }
    }
}

template<typename T>
void renderInputTextWithColor(const char* label, T value, size_t bufferSize, bool limitCheck,
                              float yHiLimit, float rHiLimit, float yLoLimit = -100.0f, float rLoLimit = -200.0f,
                              const char* format = nullptr, const int itemWidthMultiplier = 4) {
    static_assert(std::is_arithmetic<T>::value, "Value must be a numeric type (integer or floating point)");

    LimitState state = NoCheck;

    float_t itemWidthValue = ImGui::GetFontSize() * itemWidthMultiplier;
    ImGui::PushItemWidth(itemWidthValue);

    char strval[bufferSize];
    // Use provided format or default to integer/float format
    if (format) {
        snprintf(strval, bufferSize, format, static_cast<double>(value)); // `double` for compatibility
    } else if constexpr (std::is_integral<T>::value) {
        snprintf(strval, bufferSize, "%ld", static_cast<long>(value));
    } else if constexpr (std::is_floating_point<T>::value) {
        snprintf(strval, bufferSize, "%.2f", static_cast<float>(value)); // Default to 2 decimal places for floats
    }

    if (limitCheck) {
        float fvalue = static_cast<float>(value); // Ensure a common type for comparison
        if (fvalue > rHiLimit) {
            state = Red;
        } else if (fvalue > yHiLimit) {
            state = Yellow;
        } else if (fvalue < rLoLimit) {
            state = Red;
        } else if (fvalue < yLoLimit) {
            state = Yellow;
        } else {
            state = Green;
        }
    }

    // Set color before rendering
    ImGui::PushStyleColor(ImGuiCol_FrameBg, getColorForState(state));

    // Render the InputText with formatted value
    ImGui::InputText(label, strval, bufferSize);

    // Restore default color
    ImGui::PopStyleColor();
}

void ShowColormapComboBox(const char* label, int& selectedColormapIndex, ImVec2 previewSize) {
    // refer to the enum ImPlotColormap_ for the list of available colormaps
    // https://github.com/epezent/implot/discussions/168
    //ImPlot::ColormapButton("##ColormapButton", previewSize, GetColormapObjectByIndex(selectedColormapIndex));
    //ImGui::SameLine();

    // Create the Combo box to select the colormap
    if (ImGui::BeginCombo(label, GetColormapShortNameByIndex(selectedColormapIndex))) {
        for (int i = 0; i < numColormaps; i++) {
            bool isSelected = (selectedColormapIndex == i);
            ImPlot::ColormapButton("##ColormapButton", ImVec2(30,10), GetColormapObjectByIndex(i));
            ImGui::SameLine();
            if (ImGui::Selectable(GetColormapShortNameByIndex(i), isSelected)) {
                selectedColormapIndex = i;
            }

            // Highlight the selected item
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
}

void ShowColormapSelector(bool isMA) {
    static int selectedColormapIndex = 0;  // Keep track of the selected colormap

    // Call the ShowColormapComboBox function to display the combo and preview
    ShowColormapComboBox("Colormap", selectedColormapIndex, ImVec2(50, 20));

    // Example plot with the selected colormap
    ImPlotColormap selectedColormap = ImPlotColormap_Jet;
    switch (selectedColormapIndex) {
        case 0: selectedColormap = ImPlotColormap_Jet; break;
        case 1: selectedColormap = ImPlotColormap_Cool; break;
        case 2: selectedColormap = ImPlotColormap_Hot; break;
        case 3: selectedColormap = ImPlotColormap_Spectral; break;
        case 4: selectedColormap = ImPlotColormap_Plasma; break;
        case 5: selectedColormap = ImPlotColormap_Viridis; break;
        case 6: selectedColormap = ImPlotColormap_Paired; break;
        case 7: selectedColormap = ImPlotColormap_Greys; break;
        case 8: selectedColormap = ImPlotColormap_RdBu; break;
        case 9: selectedColormap = ImPlotColormap_BrBG; break;
        case 10: selectedColormap = ImPlotColormap_PiYG; break;
        case 11: selectedColormap = ImPlotColormap_Deep; break;
        case 12: selectedColormap = ImPlotColormap_Dark; break;
        case 13: selectedColormap = ImPlotColormap_Pastel; break;
        case 14: selectedColormap = ImPlotColormap_Pink; break;
        case 15: selectedColormap = ImPlotColormap_Twilight; break;
        case 16: selectedColormap = customColorMapIdMA; break;
        case 17: selectedColormap = customColorMapIdMB; break;
    }

    ImPlot::PushColormap(selectedColormap);

    if ( isMA ) {
        selectedMAColormap = selectedColormap;
    } else {
        selectedMBColormap = selectedColormap;
    }
}

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

void GenerateColorizedTexture(const std::vector<unsigned char>& intensityData, 
                              int width, 
                              int height, 
                              std::vector<unsigned char>& colorizedData,
                              ImPlotColormap colormap) {
    // Ensure the output vector is sized correctly (RGB = 3 channels)
    colorizedData.resize(width * height * 3);

    // Map intensity values (0-255) to colormap
    for (int i = 0; i < width * height; ++i) {
        float normalizedValue = intensityData[i] / 255.0f; // Normalize to [0, 1]
        ImVec4 color = ImPlot::SampleColormap(normalizedValue, colormap);

        // Convert ImVec4 to RGB (assumes no alpha channel for the texture)
        colorizedData[i * 3 + 0] = static_cast<unsigned char>(color.x * 255.0f); // Red
        colorizedData[i * 3 + 1] = static_cast<unsigned char>(color.y * 255.0f); // Green
        colorizedData[i * 3 + 2] = static_cast<unsigned char>(color.z * 255.0f); // Blue
    }
}

// initialize a texture for a MEGS image
GLuint createProperTextureFromMEGSImage(uint16_t (*data)[MEGS_IMAGE_HEIGHT][MEGS_IMAGE_WIDTH], int Image_Display_Scale, bool isMA) {
    std::vector<uint8_t> textureData(MEGS_TOTAL_PIXELS); // 8-bit data for display

    // uint16_t tmpdata[MEGS_IMAGE_HEIGHT][MEGS_IMAGE_WIDTH];

    // // remove background if box is checked
    // if ((isMA) && (globalGUI.removeMADark)) {
    //     std::memcpy(tmpdata, data, MEGS_IMAGE_HEIGHT * MEGS_IMAGE_WIDTH * sizeof(uint16_t));
    //     differenceAndClip(tmpdata, globalGUI.MADark, *data);
    //     std::cout<<"MA dark removed "<<tmpdata[0][0]<<" "<<globalGUI.MADark[0][0]<<" "<< *data[0][0] << std::endl;
    // } else if ((!isMA) && (globalGUI.removeMBDark)) {
    //     std::memcpy(tmpdata, data, MEGS_IMAGE_HEIGHT * MEGS_IMAGE_WIDTH * sizeof(uint16_t));
    //     differenceAndClip(tmpdata, globalGUI.MBDark, *data);
    //     std::cout<<"MB dark removed "<<tmpdata[0][0]<<" "<<globalGUI.MBDark[0][0]<<" "<< *data[0][0] << std::endl;
    // }

    scaleImageToTexture(data, textureData, Image_Display_Scale);

    GenerateColorizedTexture(textureData, MEGS_IMAGE_WIDTH, MEGS_IMAGE_HEIGHT, textureData, isMA ? selectedMAColormap : selectedMBColormap);
    ImPlot::PopColormap();

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

    // remove background if box is checked
    std::memcpy(globalGUI.displayableMAImage, globalState.megsa.image, MEGS_IMAGE_HEIGHT * MEGS_IMAGE_WIDTH * sizeof(uint16_t));
    if (globalGUI.removeMADark) {
        differenceAndClip(globalState.megsa.image, globalGUI.MADark, globalGUI.displayableMAImage); // switch order
        //std::cout<<"MA dark removed "<<globalGUI.displayableMAImage[0][0]<<" "<<globalGUI.MADark[0][0]<<" "<< globalState.megsa.image[0][0] << std::endl;
    }

    if (globalGUI.flipMAVertical) {
        flipVertical(globalGUI.displayableMAImage);
    }
    if (globalGUI.flipMAHorizontal) {
        flipHorizontal(globalGUI.displayableMAImage);
    }

    scaleImageToTexture(&globalGUI.displayableMAImage, textureData, globalGUI.Image_Display_Scale_MA);
    //scaleImageToTexture(&globalState.megsa.image, textureData, globalGUI.Image_Display_Scale_MA);

    SetRainbowCustomColormap(true);
    std::vector<uint8_t> colorTextureData( width * height * 3); //r,g,b
    GenerateColorizedTexture(textureData, MEGS_IMAGE_WIDTH, MEGS_IMAGE_HEIGHT, colorTextureData, selectedMAColormap); //ColormapSelectedCustom);

    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, MEGS_IMAGE_WIDTH, MEGS_IMAGE_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, colorTextureData.data());

    ImPlot::PopColormap();
}

void renderUpdatedTextureFromMEGSBImage(GLuint megsBTextureID)
{
    int width=MEGS_IMAGE_WIDTH;
    int height=MEGS_IMAGE_HEIGHT;
    std::vector<uint8_t> textureData(width * height);

    // remove background if box is checked
    std::memcpy(globalGUI.displayableMBImage, globalState.megsb.image, MEGS_IMAGE_HEIGHT * MEGS_IMAGE_WIDTH * sizeof(uint16_t));
    if (globalGUI.removeMBDark) {
        differenceAndClip(globalState.megsb.image, globalGUI.MBDark, globalGUI.displayableMBImage); // switch order
        //std::cout<<"MB dark removed "<<globalGUI.displayableMBImage[0][0]<<" "<<globalGUI.MBDark[0][0]<<" "<< globalState.megsb.image[0][0] << std::endl;
    }

    if (globalGUI.flipMBVertical) {
        flipVertical(globalGUI.displayableMBImage);
    }
    if (globalGUI.flipMBHorizontal) {
        flipHorizontal(globalGUI.displayableMBImage);
    }

    scaleImageToTexture(&globalGUI.displayableMBImage, textureData, globalGUI.Image_Display_Scale_MB);
    //scaleImageToTexture(&globalState.megsb.image, textureData, globalGUI.Image_Display_Scale_MB);
    SetRainbowCustomColormap(false);

    std::vector<uint8_t> colorTextureData( width * height * 3); //r,g,b
    GenerateColorizedTexture(textureData, MEGS_IMAGE_WIDTH, MEGS_IMAGE_HEIGHT, colorTextureData, selectedMBColormap);

    glBindTexture(GL_TEXTURE_2D, megsBTextureID); // need to bin before glTexImage2D
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, MEGS_IMAGE_WIDTH, MEGS_IMAGE_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, colorTextureData.data());

    ImPlot::PopColormap();
}


void copyROIToSmallImage(const uint16_t* largeImage, size_t largeWidth,
                   uint16_t* smallImage, size_t smallWidth,
                   size_t startRow, size_t startCol,
                   size_t height, size_t width) {
    // Iterate through the rectangle row by row
    for (size_t row = 0; row < height; ++row) {
        const uint16_t* sourceRow = largeImage + (startRow + row) * largeWidth + startCol;
        uint16_t* targetRow = smallImage + row * smallWidth;

        // Use std::copy for efficient memory copying
        std::copy(sourceRow, sourceRow + width, targetRow);
    }
}

uint16_t findMinImageValue(const uint16_t* image, size_t width, size_t height) {
    return *std::min_element(image, image + (width * height));
}

void calculateCentroid(uint16_t* image, size_t width, size_t height, double& xcentroid, double& ycentroid) {
    double xsum = 0.0;
    double ysum = 0.0;
    double total = 0.0;
    uint16_t minValue = *std::min_element(image, image + (width*height));

    // Iterate through the image pixels
    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x) {
            uint16_t pixelValue = (image[y * width + x] - minValue); //remove "background", at least one pixel will be zero
            double weight = static_cast<double>(pixelValue);

            // Accumulate the weighted sum
            xsum += weight * x;
            ysum += weight * y;
            total += weight;
        }
    }
    // If the total weight is zero, set the centroid to the origin
    if (total <= 0.0) {
        xcentroid = 0.0;
        ycentroid = 0.0;
        return;
    }

    // Calculate the centroid coordinates
    xcentroid = xsum / total;
    ycentroid = ysum / total;
}

void displayMAImageWithControls(GLuint megsATextureID)
{
    ImGui::Begin("MEGS-A Image Viewer");
    
    // Zoom slider
    ImGui::SetNextItemWidth(120);
    ImGui::InputFloat("MA Zoom", &globalGUI.mazoom, 0.2f, 0.2f, "%.2f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::Combo("Scaletype", &globalGUI.Image_Display_Scale_MA, Image_Display_Scale_Items, IM_ARRAYSIZE(Image_Display_Scale_Items));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(70);
    ShowColormapSelector(true); // false for MEGS-B

    if (ImGui::Button("Collect MA Dark")) {
        mtx.lock();
        std::memcpy(globalGUI.MADark, globalState.megsa.image, sizeof(globalGUI.MADark));
        mtx.unlock();
        std::cout<<"MA Dark collected and stored"<<std::endl;
    }
    ImGui::SameLine();
    ImGui::Checkbox("Remove MA Dark", &globalGUI.removeMADark);
    ImGui::SameLine();
    ImGui::Checkbox("X-Flip MA", &globalGUI.flipMAHorizontal);
    ImGui::SameLine();
    ImGui::Checkbox("Y-Flip MA", &globalGUI.flipMAVertical);

    ImGui::NewLine();

    ImPlot::PopColormap();
    
    mtx.lock();
    std::string tmpiISO8601sss = tai_to_iso8601_with_milliseconds(globalState.megsa.tai_time_seconds, globalState.megsa.tai_time_subseconds); 
    mtx.unlock();

    ImGui::Text("%s",tmpiISO8601sss.c_str());

    int32_t yPosHi = globalState.MAypos.load(std::memory_order_relaxed);

    int32_t yPosLo = 1024-yPosHi;
    ImGui::BeginGroup();
    {
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.0f, 1.0f, 0.0f, 0.5f)); // Green
        ImGui::VSliderInt("##MAY1", ImVec2(30,MEGS_IMAGE_HEIGHT*globalGUI.mazoom*0.5), &yPosHi, 512, 1023);
        ImGui::PopStyleColor();
        // remove spacing between sliders
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetStyle().ItemSpacing.y);
        // Set the color for the second slider
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.0f, 0.0f, 1.0f, 1.0f)); // Blue
        ImGui::VSliderInt("##MAY2", ImVec2(30,MEGS_IMAGE_HEIGHT*(globalGUI.mazoom)*0.5), &yPosLo, 0, 511);
        ImGui::PopStyleColor();
    }
    ImGui::EndGroup();
    ImGui::SameLine();

    float value = 1.0f; // changing this will crop the image
    ImGui::Image((void*)(intptr_t)megsATextureID, ImVec2(MEGS_IMAGE_WIDTH*globalGUI.mazoom,MEGS_IMAGE_HEIGHT*globalGUI.mazoom), ImVec2(0.0f,0.0f), ImVec2(value,value));

    ImGui::SameLine();
    // Add a color bar

    SetRainbowCustomColormap(true);
    ImPlot::ColormapScale("MA Colorbar", 0.0f, 255.0f, ImVec2(100, MEGS_IMAGE_HEIGHT*globalGUI.mazoom), "%g", 0, selectedMAColormap); // Adjust size as needed
    ImPlot::PopColormap();

    // dislpay the value of one pixel from each half
    uint16_t hiRowValues[MEGS_IMAGE_WIDTH];
    uint16_t lowRowValues[MEGS_IMAGE_WIDTH];
    uint16_t maxValue = 0;
    uint16_t minValue = 0xFFFF;

    uint16_t firstRow[MEGS_IMAGE_WIDTH]={0};
    uint16_t secondRow[MEGS_IMAGE_WIDTH]={0};
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

    ImGui::Begin("MA Row Plots");
    {
        std::string label; 
        static int firstRowIdx = 255;
        static int secondRowIdx = 767;

        bool isTreeNodeRowSelectOpen = ImGui::TreeNode("Select MA Rows");
        if (isTreeNodeRowSelectOpen) {
            ImGui::SetNextItemWidth(100);
            ImGui::InputInt("MA FirstRow", &firstRowIdx);
            ImGui::SetNextItemWidth(100);
            ImGui::InputInt("MA SecondRow", &secondRowIdx);
            ImGui::TreePop();
        }

        ImPlot::SetNextAxesToFit();
        //if (ImPlot::BeginPlot("MA Pixel Values", ImVec2(450, 180))) {
        if (ImPlot::BeginPlot("MA Pixel Values", ImVec2(-1, -1))) {

            mtx.lock();
            std::memcpy(firstRow, globalState.megsa.image[firstRowIdx], sizeof(firstRow));
            std::memcpy(secondRow, globalState.megsa.image[secondRowIdx], sizeof(secondRow));
            mtx.unlock();

            label = "MA Row "+std::to_string(firstRowIdx);
            ImPlot::SetNextLineStyle(ImVec4(235.0f / 255.0f, 137.0f / 255.0f, 52.0f / 255.0f, 1.0f));
            ImPlot::PlotLine(label.c_str(), firstRow, MEGS_IMAGE_WIDTH);

            label = "MA Row "+std::to_string(secondRowIdx);
            ImPlot::PlotLine(label.c_str(), secondRow, MEGS_IMAGE_WIDTH);
            ImPlot::EndPlot();
        }

    }
    ImGui::End();

    ImGui::Begin("MEGS-A Rate Meter");

    if ( ImGui::TreeNode("MA Rate Meter") )
    {
        static std::deque<uint16_t> megsARateMeterDN;
        size_t maxRateMeterSize = 10;

        static int xyTarget[2] = {1610, 285}; // SAM center?
        static int xyhalfWidth[2] = {5, 5};
        float itemWidthValue = ImGui::GetFontSize() * 6;
        ImGui::PushItemWidth(itemWidthValue);

        ImGui::InputInt2("X,Y Target", xyTarget);

        uint16_t imageSmall[xyhalfWidth[0] * 2][xyhalfWidth[1] * 2];

        mtx.lock();
        // create a pointer to the selected image
        auto& selectedIMage = globalState.megsa.image;
        int value = selectedIMage[xyTarget[1]][xyTarget[0]];
        copyROIToSmallImage(&selectedIMage[0][0], MEGS_IMAGE_WIDTH, 
            &imageSmall[0][0], xyhalfWidth[0]*2, 
            xyTarget[1] - xyhalfWidth[1], 
            xyTarget[0] - xyhalfWidth[0], 
            2 * xyhalfWidth[1], 2 * xyhalfWidth[0]);
        mtx.unlock();
        ImGui::Text("DN at Target: %d", value);

        // only update when megsAUpdated is set to true
        static uint32_t megsAImageCountPrev = 0;
        if (globalState.megsAImageCount.load(std::memory_order_relaxed) != megsAImageCountPrev) {
            megsAImageCountPrev = globalState.megsAImageCount.load(std::memory_order_relaxed);
            // add the value to the rate meter deque
            if ( megsARateMeterDN.size() >= maxRateMeterSize ) {
                megsARateMeterDN.pop_front();
            }
            megsARateMeterDN.push_back(value);
        }

        ImGui::InputInt2("X,Y halfWidth", xyhalfWidth);
        double xcentroid, ycentroid;
        calculateCentroid(&imageSmall[0][0], xyhalfWidth[0] * 2, xyhalfWidth[1] * 2, xcentroid, ycentroid);
        xcentroid += xyTarget[0] - xyhalfWidth[0];
        ycentroid += xyTarget[1] - xyhalfWidth[1];

        ImGui::Text("Centroid location: %.2f %.2f", xcentroid, ycentroid);

        // Plot the rate meter
        ImPlot::SetNextAxesToFit();
        if (ImPlot::BeginPlot("MEGS-A Rate Meter", ImVec2(-1, 200))) {
            std::vector<uint16_t> megsARateMeterDNVec(megsARateMeterDN.begin(), megsARateMeterDN.end());
            ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle);
            ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(0.0f, 1.0f, 0.0f, 1.0f)); // Green
            ImPlot::PlotLine("MEGS-A Rate Meter", megsARateMeterDNVec.data(), megsARateMeterDNVec.size());
            ImPlot::PopStyleColor();
            ImPlot::EndPlot();
        }

        ImGui::TreePop();
    }

    ImGui::End();

}

void displayMBImageWithControls(GLuint megsBTextureID)
{
    ImGui::Begin("MEGS-B Image Viewer");

    ImGui::SetNextItemWidth(120);
    ImGui::InputFloat("MB Zoom", &globalGUI.mbzoom, 0.2f, 0.2f, "%.2f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::Combo("Scaletype", &globalGUI.Image_Display_Scale_MB, Image_Display_Scale_Items, IM_ARRAYSIZE(Image_Display_Scale_Items));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(70);
    ShowColormapSelector(false); // false for MEGS-B

    if (ImGui::Button("Collect MB Dark")) {
        mtx.lock();
        std::memcpy(globalGUI.MBDark, globalState.megsb.image, sizeof(globalGUI.MBDark));
        mtx.unlock();
        std::cout<<"MB Dark collected and stored"<<std::endl;
    }
    ImGui::SameLine();
    ImGui::Checkbox("Remove MB Dark", &globalGUI.removeMBDark);
    ImGui::SameLine();
    ImGui::Checkbox("X-Flip MB", &globalGUI.flipMBHorizontal);
    ImGui::SameLine();
    ImGui::Checkbox("Y-Flip MB", &globalGUI.flipMBVertical);

    ImGui::NewLine();

    ImPlot::PopColormap();

    mtx.lock();
    std::string tmpiISO8601sss = tai_to_iso8601_with_milliseconds(globalState.megsb.tai_time_seconds, globalState.megsb.tai_time_subseconds); 
    mtx.unlock();

    ImGui::Text("%s",tmpiISO8601sss.c_str());

    int32_t yPosHi = globalState.MBypos.load(std::memory_order_relaxed);

    int32_t yPosLo = 1024-yPosHi;
    ImGui::BeginGroup();
    {
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.0f, 1.0f, 0.0f, 0.5f)); // Green
        ImGui::VSliderInt("##MBY1", ImVec2(30,MEGS_IMAGE_HEIGHT*globalGUI.mbzoom*0.5), &yPosHi, 512, 1023);
        ImGui::PopStyleColor();
        // remove spacing between sliders
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetStyle().ItemSpacing.y);
        // Set the color for the second slider
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.0f, 0.0f, 1.0f, 1.0f)); // Blue
        ImGui::VSliderInt("##MBY2", ImVec2(30,MEGS_IMAGE_HEIGHT*(globalGUI.mbzoom)*0.5), &yPosLo, 0, 511);
        ImGui::PopStyleColor();
    }
    ImGui::EndGroup();
    ImGui::SameLine();

    float value = 1.0f; // changing this will crop the image
    ImGui::Image((void*)(intptr_t)megsBTextureID, ImVec2(MEGS_IMAGE_WIDTH*globalGUI.mbzoom,MEGS_IMAGE_HEIGHT*globalGUI.mbzoom), ImVec2(0.0f,0.0f), ImVec2(value,value));

    ImGui::SameLine();
    // Add a color bar

    SetRainbowCustomColormap(false);
    ImPlot::ColormapScale("MB Colorbar", 0.0f, 255.0f, ImVec2(100, MEGS_IMAGE_HEIGHT*globalGUI.mbzoom), "%g", 0, selectedMBColormap); // Adjust size as needed
    ImPlot::PopColormap();

    // dislpay the value of one pixel from each half
    uint16_t hiRowValues[MEGS_IMAGE_WIDTH];
    uint16_t lowRowValues[MEGS_IMAGE_WIDTH];
    uint16_t maxValue = 0;
    uint16_t minValue = 0xFFFF;

    uint16_t firstRow[MEGS_IMAGE_WIDTH]={0};
    uint16_t secondRow[MEGS_IMAGE_WIDTH]={0};
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

    ImGui::End();

    ImGui::Begin("MB Row Plots");
    {
        std::string label; 
        static int firstRowIdx = 255;
        static int secondRowIdx = 767;

        bool isTreeNodeRowSelectOpen = ImGui::TreeNode("Select MB Rows");
        if (isTreeNodeRowSelectOpen) {
            ImGui::SetNextItemWidth(100);
            ImGui::InputInt("MB FirstRow", &firstRowIdx);
            ImGui::SetNextItemWidth(100);
            ImGui::InputInt("MB SecondRow", &secondRowIdx);
            ImGui::TreePop();
        }

        ImPlot::SetNextAxesToFit();
        if (ImPlot::BeginPlot("MB Raw Pixel Values", ImVec2(-1, -1))) 
        {

            mtx.lock();
            std::memcpy(firstRow, globalState.megsb.image[firstRowIdx], sizeof(firstRow));
            std::memcpy(secondRow, globalState.megsb.image[secondRowIdx], sizeof(secondRow));
            mtx.unlock();

            label = "MB Row "+std::to_string(firstRowIdx);
            ImPlot::SetNextLineStyle(ImVec4(235.0f / 255.0f, 137.0f / 255.0f, 52.0f / 255.0f, 1.0f));
            ImPlot::PlotLine(label.c_str(), firstRow, MEGS_IMAGE_WIDTH);

            label = "MB Row "+std::to_string(secondRowIdx);
            ImPlot::PlotLine(label.c_str(), secondRow, MEGS_IMAGE_WIDTH);
            ImPlot::EndPlot();
        }
    }
    ImGui::End();

    ImGui::Begin("MEGS-B Rate Meter");

    if ( ImGui::TreeNode("MB Rate Meter") )
    {
        static std::deque<uint16_t> megsBRateMeterDN;
        size_t maxRateMeterSize = 10;

        static int xyTarget[2] = {1720, 252};
        static int xyhalfWidth[2] = {5, 5};
        float itemWidthValue = ImGui::GetFontSize() * 6;
        ImGui::PushItemWidth(itemWidthValue);

        ImGui::InputInt2("X,Y Target", xyTarget);

        uint16_t imageSmall[xyhalfWidth[0] * 2][xyhalfWidth[1] * 2];

        mtx.lock();
        // create a pointer to the selected image
        auto& selectedIMage = globalState.megsb.image;
        int value = selectedIMage[xyTarget[1]][xyTarget[0]];
        copyROIToSmallImage(&selectedIMage[0][0], MEGS_IMAGE_WIDTH, 
            &imageSmall[0][0], xyhalfWidth[0]*2, 
            xyTarget[1] - xyhalfWidth[1], 
            xyTarget[0] - xyhalfWidth[0], 
            2 * xyhalfWidth[1], 2 * xyhalfWidth[0]);
        mtx.unlock();
        ImGui::Text("DN at Target: %d", value);

        // only update when megsAUpdated is set to true
        static uint32_t megsBImageCountPrev = 0;
        if (globalState.megsBImageCount.load(std::memory_order_relaxed) != megsBImageCountPrev) {
            megsBImageCountPrev = globalState.megsBImageCount.load(std::memory_order_relaxed);
            // add the value to the rate meter deque
            if ( megsBRateMeterDN.size() >= maxRateMeterSize ) {
                megsBRateMeterDN.pop_front();
            }
            megsBRateMeterDN.push_back(value);
        }

        ImGui::InputInt2("X,Y halfWidth", xyhalfWidth);
        double xcentroid, ycentroid;
        calculateCentroid(&imageSmall[0][0], xyhalfWidth[0] * 2, xyhalfWidth[1] * 2, xcentroid, ycentroid);
        xcentroid += xyTarget[0] - xyhalfWidth[0];
        ycentroid += xyTarget[1] - xyhalfWidth[1];

        ImGui::Text("Centroid location: %.2f %.2f", xcentroid, ycentroid);

        // Plot the rate meter
        ImPlot::SetNextAxesToFit();
        if (ImPlot::BeginPlot("MEGS-B Rate Meter", ImVec2(-1, 200))) {
            std::vector<uint16_t> megsBRateMeterDNVec(megsBRateMeterDN.begin(), megsBRateMeterDN.end());
            ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle);
            ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(0.0f, 1.0f, 0.0f, 1.0f)); // Green
            ImPlot::PlotLine("MEGS-B Rate Meter", megsBRateMeterDNVec.data(), megsBRateMeterDNVec.size());
            ImPlot::PopStyleColor();
            ImPlot::EndPlot();
        }

        ImGui::TreePop();
    }
    ImGui::End();
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
    // TODO: need to switch indices
    #pragma omp parallel for
    for (int y = 0; y < height; ++y) {
        int rowStartIndex = y * width;
        for (int x = 0; x < width; ++x) {
            int index = rowStartIndex + x;
            uint16_t value = image[x][y];
            textureData[index] = static_cast<uint8_t>(value & 0xFF);
        }
    }
    // for (int x = 0; x < width; ++x) {
    //     for (int y = 0; y < height; ++y) {
    //         int index = y * width + x; // 1D index in textureData
    //         uint16_t value = image[x][y]; // Fetch pixel value

    //         // Example scaling to fit into 8-bit texture (adjust logic as needed)
    //         textureData[index] = static_cast<uint8_t>(value & 0xFF);
    //     }
    // }

    // Bind the texture
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Update the texture with the new data
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, textureData.data());

    // Unbind the texture
    glBindTexture(GL_TEXTURE_2D, 0);
}

std::string ByteArrayToHexString(const uint8_t* payloadBytes, size_t length)
{
    std::string result = "Timestamp: ";
    result.reserve(50 + length * 5);  // Preallocate estimated size to reduce reallocations

    // lamda function to append hex value to the result string
    // this maps the 4-bit nibbles to their hex representation 0-f
    auto appendHexValue = [&result](uint16_t value) {
        constexpr char hex_chars[] = "0123456789abcdef";
        result.push_back(hex_chars[(value >> 12) & 0xF]);
        result.push_back(hex_chars[(value >> 8) & 0xF]);
        result.push_back(hex_chars[(value >> 4) & 0xF]);
        result.push_back(hex_chars[value & 0xF]);
        result.push_back(' ');
    };

    for (size_t i = 0; i + 1 < length; i += 2) {
        if (i == 8) {
            result += " \nMode: ";
        } else if (i == 10) {
            result += "\nData Samples: ";
        }

        uint16_t value = (static_cast<uint16_t>(payloadBytes[i]) << 8) | payloadBytes[i + 1];
        appendHexValue(value);
    }

    return result;
}

// Wrapper function for using ByteArrayToHexString with a std::vector<uint8_t>
std::string ByteArrayToHexString(const std::vector<uint8_t>& payloadBytes)
{
    return ByteArrayToHexString(payloadBytes.data(), payloadBytes.size());
}

void updateRawPacketWindow()
{
    ImGui::Begin("Raw Packets");

    mtx.lock();
    if ( ImGui::TreeNodeEx("Raw Packet Payloads", ImGuiTreeNodeFlags_DefaultOpen) )
    //if ( ImGui::TreeNodeEx("Raw Packet Payloads") )
    {

        if (ImGui::TreeNode("ESP Raw Packet"))
        {
            ImGui::TextWrapped("%s", ByteArrayToHexString(globalState.espPayloadBytes, sizeof(globalState.espPayloadBytes)).c_str());
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("MEGS-P Raw Packet"))
        {
            ImGui::TextWrapped("%s", ByteArrayToHexString(globalState.megsPPayloadBytes, sizeof(globalState.megsPPayloadBytes)).c_str());
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("SHK Raw Packet"))
        {
            ImGui::TextWrapped("%s", ByteArrayToHexString(globalState.shkPayloadBytes, sizeof(globalState.shkPayloadBytes)).c_str());
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("MEGS-A Raw Packet"))
        {
            ImGui::TextWrapped("%s", ByteArrayToHexString(globalState.megsAPayloadBytes, sizeof(globalState.megsAPayloadBytes)).c_str());
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("MEGS-B Raw Packet"))
        {
            ImGui::TextWrapped("%s", ByteArrayToHexString(globalState.megsBPayloadBytes, sizeof(globalState.megsBPayloadBytes)).c_str());
            ImGui::TreePop();
        }

        ImGui::TreePop();
    }
    mtx.unlock();

    ImGui::End();

}


void updateControlWindow()
{
    ImGui::Begin("Control Panel");

    if ( ImGui::TreeNode("Developer Control") )
    {
        ImGui::Text("CCSDSReader packet delay (ms)");
        bool enableSlowReplay = globalState.args.slowReplay.load();
        ImGui::Checkbox("Enable Slow Replay", &enableSlowReplay);
        globalState.args.slowReplay.store(enableSlowReplay);

        if (enableSlowReplay) {
            int delay = globalState.slowReplayWaitTime.load();
            ImGui::SliderInt("Packet Delay", &delay, 1, 10);
            globalState.slowReplayWaitTime.store(delay);
        }
        ImGui::TreePop();
    }

    if ( ImGui::TreeNode("Reset Packet Gap Counters") )
    {
        if (ImGui::Button("Reset All")) {
            globalState.dataGapsMA.store(0);
            globalState.dataGapsMB.store(0);
            globalState.dataGapsESP.store(0);
            globalState.dataGapsMP.store(0);
            globalState.dataGapsSHK.store(0);
        }
        if (ImGui::Button("Reset MEGS-A Gap Count")) {
            globalState.dataGapsMA.store(0);
        }
        if (ImGui::Button("Reset MEGS-B Gap Count")) {
            globalState.dataGapsMB.store(0);
        }
        if (ImGui::Button("Reset ESP Gap Count")) {
            globalState.dataGapsESP.store(0);
        }
        if (ImGui::Button("Reset MEGS-P Gap Count")) {
            globalState.dataGapsMP.store(0);
        }
        if (ImGui::Button("Reset SHK Gap Count")) {
            globalState.dataGapsSHK.store(0);
        }
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Reset Parity Error Counters")) {
        if (ImGui::Button("Reset All")) {
            globalState.parityErrorsMA.store(0);
            globalState.parityErrorsMB.store(0);
        }
        if (ImGui::Button("Reset MEGS-A Parity Errors")) {
            globalState.parityErrorsMA.store(0);
        }
        if (ImGui::Button("Reset MEGS-B Parity Errors")) {
            globalState.parityErrorsMB.store(0);
        }
        ImGui::TreePop();
    }

    ImGui::End();
}

void updateStatusWindow()
{
    ImGui::Begin("Channel Status");

    LimitState state = Green;

    ImGuiIO& io = ImGui::GetIO();
    if ( ImGui::TreeNode("Display Status") )
    {
        ImGui::Text("Refresh rate: %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
        ImGui::TreePop();
    }

    mtx.lock();
    double eTAI    = globalState.esp.tai_time_seconds;
    double eTAIsub = globalState.esp.tai_time_subseconds;
    double rTAI    = globalState.esp.rec_tai_seconds;
    double rTAIsub = globalState.esp.rec_tai_subseconds;
    mtx.unlock();
    
    double espTAI = tai_ss(eTAI, eTAIsub);
    double espRecTAI = tai_ss(rTAI, rTAIsub);

    double deltaTimeMilliSec = (espTAI - espRecTAI)*1000.0f;
    double redLim = 100.0f;
    double yellowLim = 99.9f; // essentially disable yellow state
    //double yellowLim = 70.0f;
    if ( (deltaTimeMilliSec > redLim) | (deltaTimeMilliSec < -redLim) ) {
        state = Red;
    } else if ( (deltaTimeMilliSec > yellowLim) | (deltaTimeMilliSec < -yellowLim) ) {
        state = Yellow;
    }

    bool isTreeNodeClockStatusOpen = ImGui::TreeNode("Clock Status");
    addFilledCircleToTreeNode(state);
    if ( isTreeNodeClockStatusOpen )
    {
        ImGui::Text("Pkt TAI: %.3f ", espTAI);
        ImGui::Text("Rec TAI: %.3f ", espRecTAI);
        renderInputTextWithColor("Rec-Pkt (millisec)", (deltaTimeMilliSec), 12, true, yellowLim, redLim, -yellowLim, -redLim, "%.3f", 7);
        ImGui::TreePop();
    }


    if ( ImGui::TreeNodeEx("Packet Counters", ImGuiTreeNodeFlags_DefaultOpen)) {
        renderInputTextWithColor("601 a59 MEGS-A Pkts", globalState.packetsReceived.MA.load(std::memory_order_relaxed), 12, false, 0.0, 0.9,-100,-200,nullptr,5);
        renderInputTextWithColor("602 a5a MEGS-B Pkts", globalState.packetsReceived.MB.load(std::memory_order_relaxed), 12, false, 0.0, 0.9,-100,-200,nullptr,5);
        renderInputTextWithColor("604 a5c ESP Pkts", globalState.packetsReceived.ESP.load(std::memory_order_relaxed), 12, false, 0.0, 0.9);
        renderInputTextWithColor("605 a5d MEGS-P Pkts", globalState.packetsReceived.MP.load(std::memory_order_relaxed), 12, false, 0.0, 0.9);
        renderInputTextWithColor("606 a5e SHK Pkts",globalState.packetsReceived.SHK.load(std::memory_order_relaxed), 12, false, 0.0, 0.9);
        renderInputTextWithColor("Unknown Packets", globalState.packetsReceived.Unknown.load(std::memory_order_relaxed), 12, true, 0.0, 0.9);
        ImGui::TreePop();
    }


    state = Green; // initialize to green
    if ((globalState.dataGapsMA.load() != 0) || (globalState.dataGapsMB.load() != 0) ||
        (globalState.dataGapsESP.load() != 0) || (globalState.dataGapsMP.load() != 0) ||
        (globalState.dataGapsSHK.load() != 0)) {
        state = Red;
    }

    bool isTreeNodePacketGapCounterOpen = ImGui::TreeNodeEx("Packet Gap Counters");
    addFilledCircleToTreeNode(state);
    if (isTreeNodePacketGapCounterOpen ) {
        renderInputTextWithColor("MEGS-A Gap Count", globalState.dataGapsMA.load(std::memory_order_relaxed), 12, !globalState.isFirstMAImage.load(std::memory_order_relaxed), 0.0, 0.9);
        renderInputTextWithColor("MEGS-B Gap Count", globalState.dataGapsMB.load(std::memory_order_relaxed), 12, !globalState.isFirstMBImage.load(std::memory_order_relaxed), 0.0, 0.9);
        renderInputTextWithColor("MEGS-P Gap Count", globalState.dataGapsMP.load(std::memory_order_relaxed), 12, true, 0.0, 0.9);
        renderInputTextWithColor("ESP Gap Count", globalState.dataGapsESP.load(std::memory_order_relaxed), 12, true, 0.0, 0.9);
        renderInputTextWithColor("SHK Gap Count", globalState.dataGapsSHK.load(std::memory_order_relaxed), 12, true, 0.0, 0.9);
        ImGui::TreePop();
    }

    state = Green;
    if ((globalState.parityErrorsMA.load() != 0) || (globalState.parityErrorsMB.load() !=0)) {
        state = Red;
    }
    bool isTreeNodeParityErrorCountersOpen = ImGui::TreeNodeEx("Parity Error Counters");
    addFilledCircleToTreeNode(state);
    if (isTreeNodeParityErrorCountersOpen) {
        renderInputTextWithColor("MEGS-A Parity Errors", globalState.parityErrorsMA.load(std::memory_order_relaxed), 12, true, 0.0, 0.9);
        renderInputTextWithColor("MEGS-B Parity Errors", globalState.parityErrorsMB.load(std::memory_order_relaxed), 12, true, 0.0, 0.9);
        ImGui::TreePop();
    }

    // determine saturated pixels
    {
        uint32_t saturatedPixelsTop, saturatedPixelsBottom;
        bool testPattern;

        testPattern = globalState.isMATestPattern.load();
        mtx.lock();
        countSaturatedPixels(globalState.megsa.image,
            saturatedPixelsTop,
            saturatedPixelsBottom,
            testPattern);
        mtx.unlock();
        globalState.saturatedPixelsMABottom.store(saturatedPixelsBottom, std::memory_order_relaxed);
        globalState.saturatedPixelsMATop.store(saturatedPixelsTop, std::memory_order_relaxed);

        testPattern = globalState.isMBTestPattern.load();
        mtx.lock();
        countSaturatedPixels(globalState.megsb.image,
            saturatedPixelsTop,
            saturatedPixelsBottom,
            testPattern);
        mtx.unlock();
        globalState.saturatedPixelsMBBottom.store(saturatedPixelsBottom, std::memory_order_relaxed);
        globalState.saturatedPixelsMBTop.store(saturatedPixelsTop, std::memory_order_relaxed);
    }

    state = Green;
    if ((globalState.saturatedPixelsMABottom.load()) || (globalState.saturatedPixelsMATop.load() !=0) ||
        (globalState.saturatedPixelsMBBottom.load()) || (globalState.saturatedPixelsMBTop.load() !=0)) {
        state = Red;
    }
    // display
    bool isTreeNodeSaturatedPixelsOpen = ImGui::TreeNode("Saturated Pixels");
    addFilledCircleToTreeNode(state);
    if (isTreeNodeSaturatedPixelsOpen) { // calculate and display regardless of tree node open or not
        renderInputTextWithColor("MEGS-A Top Saturated", globalState.saturatedPixelsMATop.load(std::memory_order_relaxed), 12, true, 0.0, 0.9);
        renderInputTextWithColor("MEGS-A Bottom Saturated", globalState.saturatedPixelsMABottom.load(std::memory_order_relaxed), 12, true, 0.0, 0.9);

         renderInputTextWithColor("MEGS-B Top Saturated", globalState.saturatedPixelsMBTop.load(std::memory_order_relaxed), 12, true, 0.0, 0.9);
        renderInputTextWithColor("MEGS-B Bottom Saturated", globalState.saturatedPixelsMBBottom.load(std::memory_order_relaxed), 12, true, 0.0, 0.9);
        ImGui::TreePop();
    }
    ImGui::End(); // end of Channel Status
}

 void plotESPTarget(int lastIdx) {
    constexpr float twoPi = 2.0f * 3.1415926535f;
    constexpr int maxPoints = 40; // Maximum number of points to store

    static std::deque<std::pair<float, float>> pointXYBuffer; // Circular buffer for rel XY points

    float d0 = 32.0f;
    float d1 = 47.0f;
    float d2 = 41.0f;
    float d3 = 39.0f; // dark offsets for the quad diodes, may need to tune these for temperature

    // reset dark if needed automatically
    d0 = std::min(d0, static_cast<float>(globalState.esp.ESP_q0[lastIdx]));
    d1 = std::min(d1, static_cast<float>(globalState.esp.ESP_q1[lastIdx]));
    d2 = std::min(d2, static_cast<float>(globalState.esp.ESP_q2[lastIdx]));
    d3 = std::min(d3, static_cast<float>(globalState.esp.ESP_q3[lastIdx]));

    // Calculate the angles from the quad diodes, this is almost a direct reuse of the code from the flight L0B code
    float qsum = globalState.esp.ESP_q0[lastIdx] + globalState.esp.ESP_q1[lastIdx] + 
                 globalState.esp.ESP_q2[lastIdx] + globalState.esp.ESP_q3[lastIdx] - (d0 + d1 + d2 + d3);
    qsum = std::max(qsum, 1.e-6f); // avoid divide by zero
    float inv_qsum = 1.f / qsum;
    float dq0 = std::max(globalState.esp.ESP_q0[lastIdx] - d0, 0.0f);
    float dq1 = std::max(globalState.esp.ESP_q1[lastIdx] - d1, 0.0f);
    float dq2 = std::max(globalState.esp.ESP_q2[lastIdx] - d2, 0.0f);
    float dq3 = std::max(globalState.esp.ESP_q3[lastIdx] - d3, 0.0f);

    float qX = ((dq1 + dq3) - (dq0 + dq2)) * inv_qsum;
 	float qY = ((dq0 + dq1) - (dq2 + dq3)) * inv_qsum;
  
    float maxAbsNorm = (abs(qX) > abs(qY)) ? abs(qX) : abs(qY);

    int arcsecX = ((int) ((qX - 2.0) * 1000)) + 1000;
 	int arcsecY = ((int) ((qY - 2.0) * 1000)) + 1000;
    float xanglearcsec = (esp_x_angle_table[arcsecX] / 1000.0);
    float yanglearcsec = (esp_y_angle_table[arcsecY] / 1000.0);

    float xangleDeg = xanglearcsec / 3600.0;
    float yangleDeg = yanglearcsec / 3600.0;
    float xangleArcmin = xangleDeg * 60.0;
    float yangleArcmin = yangleDeg * 60.0;

    float maxAngleArcSecNorm = (abs(xanglearcsec) > abs(yanglearcsec)) ? abs(xanglearcsec) : abs(yanglearcsec);

    static int selectedValue = 0;
    if (ImGui::TreeNode("Select Units"))
    {
        ImGui::RadioButton("Rel XY", &selectedValue, 0);
        ImGui::SameLine();
        ImGui::RadioButton("ArcSec", &selectedValue, 1);
        ImGui::SameLine();
        ImGui::RadioButton("ArcMin", &selectedValue, 2);
        ImGui::SameLine();
        ImGui::RadioButton("Degrees", &selectedValue, 3);
        ImGui::Text("Double-click in plot to auto scale to fit, scroll to zoom in/out");
        ImGui::TreePop();
    }
    float maxNorm = maxAbsNorm;

    // Add the new point to the circular buffer
    if (selectedValue == 0) {
        pointXYBuffer.emplace_back(qX, qY);
    } else if (selectedValue == 1) {
        pointXYBuffer.emplace_back(xanglearcsec, yanglearcsec);
        maxNorm = maxAngleArcSecNorm; // arcsec
    } else if (selectedValue == 2) {
        pointXYBuffer.emplace_back(xangleArcmin, yangleArcmin);
        maxNorm = maxAngleArcSecNorm/60.; // convert to arcmin
    } else {
        pointXYBuffer.emplace_back(xangleDeg, yangleDeg);
        maxNorm = maxAngleArcSecNorm/3600.; // convert to degrees
    }
    // Limit the number of points in the buffer
    if (pointXYBuffer.size() > maxPoints) {
        pointXYBuffer.pop_front(); // Remove the oldest point
    }

    // Get the available space
    ImVec2 availableSize = ImGui::GetContentRegionAvail();
    // Calculate the side length based on the smaller dimension (for a square plot)
    float sideLength = availableSize.x;
    // Create a square plot size based on the smallest dimension
    ImVec2 plotSize(sideLength, sideLength);

    // Create a plot with ImPlot
    if (ImPlot::BeginPlot("##ESP Target Plot", plotSize, ImPlotFlags_Equal)) {
        // Set the axis ranges to be from -1 to +1
        float maxScale = (maxNorm > 1.0) ? maxNorm * 1.5 : 1.0;
        ImPlot::SetupAxisLimits(ImAxis_X1, -maxScale, maxScale);
        ImPlot::SetupAxisLimits(ImAxis_Y1, -maxScale, maxScale);

        ImPlot::SetAxes(ImAxis_X1, ImAxis_Y1); // default

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

        // Plot all points in the buffer
        std::vector<float> xBuffer, yBuffer;
        for (const auto& point : pointXYBuffer) {
            xBuffer.push_back(point.first);
            yBuffer.push_back(point.second);
        }

        // Plot the data point (as an asterisk)
        ImPlot::SetNextMarkerStyle(ImPlotMarker_Asterisk, 8.0f, ImVec4(1, 0, 0, 1), 1.5f);
        
        ImPlot::PlotScatter("##Symbols", xBuffer.data(), yBuffer.data(), xBuffer.size());
        // add lines
        ImPlot::PlotLine("##Lines", xBuffer.data(), yBuffer.data(), xBuffer.size());

        // Display the angles below the plot
        if (selectedValue == 0) {
            ImGui::Text("Rel X: %.2f", qX);
            ImGui::Text("Rel Y: %.2f", qY);
        } else if (selectedValue == 1) {
            ImGui::Text("X Angle: %.2f arcsec", xanglearcsec);
            ImGui::Text("Y Angle: %.2f arcsec", yanglearcsec);
        } else if (selectedValue == 2) {
            ImGui::Text("X Angle: %.2f arcmin", xangleArcmin);
            ImGui::Text("Y Angle: %.2f arcmin", yangleArcmin);
        } else {
            ImGui::Text("X Angle: %.2f degrees", xangleDeg);
            ImGui::Text("Y Angle: %.2f degrees", yangleDeg);
        }
        renderInputTextWithColor("Qsum-dark:", qsum, 12, true, 100000.f, 200000.f, 500.f, 180.f);
 
        // End the plot
        ImPlot::EndPlot();
    }
}

void updateSHKWindow()
{
    ImGui::Begin("SHK Data");

    mtx.lock();
    bool isFPGATreeNodeOpen = ImGui::TreeNodeEx("SHK FPGA Status"); //, ImGuiTreeNodeFlags_DefaultOpen);
    if (isFPGATreeNodeOpen)
    {
        renderInputTextWithColor("Mode", globalState.shk.mode[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("MEGSA_Analog_Mux", globalState.shk.MEGSA_Analog_Mux_Register[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("MEGSB_Analog_Mux", globalState.shk.MEGSB_Analog_Mux_Register[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("MEGSA_Digital_Status", globalState.shk.MEGSA_Digital_Status_Register[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("MEGSB_Digital_Status", globalState.shk.MEGSB_Digital_Status_Register[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("MEGSA Integration Time", globalState.shk.MEGSA_Integration_Timer_Register[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("MEGSB Integration Time", globalState.shk.MEGSB_Integration_Timer_Register[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("MEGSA Command Error Count", globalState.shk.MEGSA_Command_Error_Count_Register[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("MEGSB Command Error Count", globalState.shk.MEGSB_Command_Error_Count_Register[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("MEGSA CEB FGA Version", globalState.shk.MEGSA_CEB_FPGA_Version_Register[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("MEGSB CEB FGA Version", globalState.shk.MEGSB_CEB_FPGA_Version_Register[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("FPGA Board Temp", globalState.shkConv.FPGA_Board_Temperature[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("FPGA Board +5V", globalState.shkConv.FPGA_Board_p5_0_Voltage[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("FPGA Board +3.3V", globalState.shkConv.FPGA_Board_p3_3_Voltage[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("FPGA Board +2.5V", globalState.shkConv.FPGA_Board_p2_5_Voltage[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("FPGA Board +1.2V", globalState.shkConv.FPGA_Board_p1_2_Voltage[0], 12, false, 0.0, 0.9);
        ImGui::TreePop();
    }
    // MEGS-A
    if (ImGui::TreeNode("SHK MEGS-A Status"))
    {
        renderInputTextWithColor("MEGSA CEB Temp", globalState.shkConv.MEGSA_CEB_Temperature[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("MEGSA CPR Temp", globalState.shkConv.MEGSA_CPR_Temperature[0], 12, false, 0.0, 0.9);    
        renderInputTextWithColor("MEGSA +24V", globalState.shkConv.MEGSA_p24_Voltage[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("MEGSA +15V", globalState.shkConv.MEGSA_p15_Voltage[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("MEGSA -15V", globalState.shkConv.MEGSA_m15_Voltage[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("MEGSA +5V analog", globalState.shkConv.MEGSA_p5_0_Analog_Voltage[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("MEGSA -5V", globalState.shkConv.MEGSA_m5_0_Voltage[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("MEGSA +5V digital", globalState.shkConv.MEGSA_p5_0_Digital_Voltage[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("MEGSA +2.5V", globalState.shkConv.MEGSA_p2_5_Voltage[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("MEGSA +24V current", globalState.shkConv.MEGSA_p24_Current[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("MEGSA +15V current", globalState.shkConv.MEGSA_p15_Current[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("MEGSA -15V current", globalState.shkConv.MEGSA_m15_Current[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("MEGSA +5V analog current", globalState.shkConv.MEGSA_p5_0_Analog_Current[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("MEGSA -5V current", globalState.shkConv.MEGSA_m5_0_Current[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("MEGSA +5V digital current", globalState.shkConv.MEGSA_p5_0_Digital_Current[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("MEGSA +2.5V current", globalState.shkConv.MEGSA_p2_5_Current[0], 12, false, 0.0, 0.9);
        ImGui::TreePop();
    }
    // MEGSB
        if (ImGui::TreeNode("SHK MEGS-B Status"))
    {
        renderInputTextWithColor("MEGSB CEB Temp", globalState.shkConv.MEGSB_CEB_Temperature[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("MEGSB CPR Temp", globalState.shkConv.MEGSB_CPR_Temperature[0], 12, false, 0.0, 0.9);    
        renderInputTextWithColor("MEGSB +24V", globalState.shkConv.MEGSB_p24_Voltage[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("MEGSB +15V", globalState.shkConv.MEGSB_p15_Voltage[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("MEGSB -15V", globalState.shkConv.MEGSB_m15_Voltage[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("MEGSB +5V analog", globalState.shkConv.MEGSB_p5_0_Analog_Voltage[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("MEGSB -5V", globalState.shkConv.MEGSB_m5_0_Voltage[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("MEGSB +5V digital", globalState.shkConv.MEGSB_p5_0_Digital_Voltage[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("MEGSB +2.5V", globalState.shkConv.MEGSB_p2_5_Voltage[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("MEGSB +24V current", globalState.shkConv.MEGSB_p24_Current[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("MEGSB +15V current", globalState.shkConv.MEGSB_p15_Current[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("MEGSB -15V current", globalState.shkConv.MEGSB_m15_Current[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("MEGSB +5V analog current", globalState.shkConv.MEGSB_p5_0_Analog_Current[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("MEGSB -5V current", globalState.shkConv.MEGSB_m5_0_Current[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("MEGSB +5V digital current", globalState.shkConv.MEGSB_p5_0_Digital_Current[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("MEGSB +2.5V current", globalState.shkConv.MEGSB_p2_5_Current[0], 12, false, 0.0, 0.9);
        ImGui::TreePop();
    }

    // Component Temperatures
    if (ImGui::TreeNode("SHK Component Temperatures"))
    {
        // Thermistor_Diodes are disconnected for SURF
        //renderInputTextWithColor("MEGSA Thermistor Diode", globalState.shkConv.MEGSA_Thermistor_Diode[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("MEGSA PRT Temp", globalState.shkConv.MEGSA_PRT[0], 12, false, 0.0, 0.9);
        //renderInputTextWithColor("MEGSB Thermistor Diode", globalState.shkConv.MEGSB_Thermistor_Diode[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("MEGSB PRT Temp", globalState.shkConv.MEGSB_PRT[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("ESP Electrometer Temp", globalState.shkConv.ESP_Electrometer_Temperature[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("ESP_Detector Temp", globalState.shkConv.ESP_Detector_Temperature[0], 12, false, 0.0, 0.9);
        renderInputTextWithColor("MEGSP_Temperature", globalState.shkConv.MEGSP_Temperature[0], 12, false, 0.0, 0.9);
        ImGui::TreePop();
    }

    mtx.unlock();
    ImGui::End();
}

void updateESPWindow()
{
 
    uint16_t espIndex = globalState.espIndex.load();

    ImGui::Begin("ESP MEGS-P Diodes");
    {
        mtx.lock();

        std::string tmpiISO8601sss = tai_to_iso8601_with_milliseconds(globalState.esp.tai_time_seconds, globalState.esp.tai_time_subseconds); 
        ImGui::Text("pkt:%s", tmpiISO8601sss.c_str());

        renderInputTextWithColor("ESP xfer cnt", globalState.esp.ESP_xfer_cnt[espIndex], 12, false, 0.0, 0.9);
        renderInputTextWithColor("ESP q0", globalState.esp.ESP_q0[espIndex], 12, false, 0.0, 0.9);
        renderInputTextWithColor("ESP q1", globalState.esp.ESP_q1[espIndex], 12, false, 0.0, 0.9);
        renderInputTextWithColor("ESP q2", globalState.esp.ESP_q2[espIndex], 12, false, 0.0, 0.9);
        renderInputTextWithColor("ESP q3", globalState.esp.ESP_q3[espIndex], 12, false, 0.0, 0.9);
        renderInputTextWithColor("ESP 171", globalState.esp.ESP_171[espIndex], 12, false, 0.0, 0.9);
        renderInputTextWithColor("ESP 257", globalState.esp.ESP_257[espIndex], 12, false, 0.0, 0.9);
        renderInputTextWithColor("ESP 304", globalState.esp.ESP_304[espIndex], 12, false, 0.0, 0.9);
        renderInputTextWithColor("ESP 366", globalState.esp.ESP_366[espIndex], 12, false, 0.0, 0.9);
        renderInputTextWithColor("ESP dark", globalState.esp.ESP_dark[espIndex], 12, false, 0.0, 0.9);

        renderInputTextWithColor("MP Ly-a", globalState.megsp.MP_lya[espIndex], 12, false, 0.0, 0.9);
        renderInputTextWithColor("MP dark", globalState.megsp.MP_dark[espIndex], 12, false, 0.0, 0.9);
        mtx.unlock();
    }
    ImGui::End(); // end of ESP MEGS-P Diodes

    // Plot the ESP data
    ImGui::Begin("ESP Target");
    mtx.lock();
    plotESPTarget(espIndex);
    mtx.unlock();
    ImGui::End(); // end of ESP Target


    ImGui::Begin("ESP Timeseries");

    ImPlot::SetNextAxesToFit();
    ImVec2 availableSize = ImGui::GetContentRegionAvail(); // Get the available space for the plot
    float halfHeight = availableSize.y / 2;
    ImVec2 plotSize(availableSize.x, halfHeight);

    int headIndex = (espIndex + 1) % ESP_INTEGRATIONS_PER_FILE;

    if (ImPlot::BeginPlot("##Quads", plotSize)) { 
        static ImPlotLegendFlags quadLegendFlags = ImPlotLegendFlags_Horizontal | ImPlotLegendFlags_Outside;
        ImPlot::SetupLegend(ImPlotLocation_North, quadLegendFlags);

        mtx.lock();
        std::vector<uint16_t> q0 = reorderCircularBuffer(globalState.esp.ESP_q0, ESP_INTEGRATIONS_PER_FILE, headIndex);
        std::vector<uint16_t> q1 = reorderCircularBuffer(globalState.esp.ESP_q1, ESP_INTEGRATIONS_PER_FILE, headIndex);
        std::vector<uint16_t> q2 = reorderCircularBuffer(globalState.esp.ESP_q2, ESP_INTEGRATIONS_PER_FILE, headIndex);
        std::vector<uint16_t> q3 = reorderCircularBuffer(globalState.esp.ESP_q3, ESP_INTEGRATIONS_PER_FILE, headIndex);
        mtx.unlock();

        ImPlot::PlotLine("ESP q0", q0.data(), ESP_INTEGRATIONS_PER_FILE);
        ImPlot::PlotLine("ESP q1", q1.data(), ESP_INTEGRATIONS_PER_FILE);
        ImPlot::PlotLine("ESP q2", q2.data(), ESP_INTEGRATIONS_PER_FILE);
        ImPlot::PlotLine("ESP q3", q3.data(), ESP_INTEGRATIONS_PER_FILE);
        ImPlot::EndPlot();
    }

    ImGui::Spacing();

    ImPlot::SetNextAxesToFit();
    if (ImPlot::BeginPlot("##Others", plotSize)) {
        static ImPlotLegendFlags espOtherLegendFlags = ImPlotLegendFlags_Horizontal | ImPlotLegendFlags_Outside;
        ImPlot::SetupLegend(ImPlotLocation_North, espOtherLegendFlags);

        mtx.lock();
        std::vector<uint16_t> ESP_171 = reorderCircularBuffer(globalState.esp.ESP_171, ESP_INTEGRATIONS_PER_FILE, headIndex);
        std::vector<uint16_t> ESP_257 = reorderCircularBuffer(globalState.esp.ESP_257, ESP_INTEGRATIONS_PER_FILE, headIndex);
        std::vector<uint16_t> ESP_304 = reorderCircularBuffer(globalState.esp.ESP_304, ESP_INTEGRATIONS_PER_FILE, headIndex);
        std::vector<uint16_t> ESP_366 = reorderCircularBuffer(globalState.esp.ESP_366, ESP_INTEGRATIONS_PER_FILE, headIndex);
        std::vector<uint16_t> ESP_dark = reorderCircularBuffer(globalState.esp.ESP_dark, ESP_INTEGRATIONS_PER_FILE, headIndex);
        std::vector<uint16_t> MP_Lya = reorderCircularBuffer(globalState.megsp.MP_lya, MEGSP_INTEGRATIONS_PER_FILE, headIndex);
        std::vector<uint16_t> MP_dark = reorderCircularBuffer(globalState.megsp.MP_dark, MEGSP_INTEGRATIONS_PER_FILE, headIndex);
        mtx.unlock();

        ImPlot::PlotLine("ESP 17", ESP_171.data(), ESP_INTEGRATIONS_PER_FILE);
        ImPlot::PlotLine("ESP 25", ESP_257.data(), ESP_INTEGRATIONS_PER_FILE);
        ImPlot::PlotLine("ESP 30", ESP_304.data(), ESP_INTEGRATIONS_PER_FILE);
        ImPlot::PlotLine("ESP 36", ESP_366.data(), ESP_INTEGRATIONS_PER_FILE);
        ImPlot::PlotLine("ESP dark", ESP_dark.data(), ESP_INTEGRATIONS_PER_FILE);
        ImPlot::PlotLine("MEGSP Lya", MP_Lya.data(), MEGSP_INTEGRATIONS_PER_FILE);
        ImPlot::PlotLine("MEGSP dark", MP_dark.data(), MEGSP_INTEGRATIONS_PER_FILE);
        ImPlot::EndPlot();
    }


    ImGui::End(); // end of ESP Timeseries
}

void displayFPGAStatus() {

    ImGui::Begin("SDO-EVE Rocket FPGA Status");                          // Create a window called "Hello, world!" and append into it.
            
    uint16_t reg0 = globalState.FPGA_reg0.load();
    uint16_t reg1 = globalState.FPGA_reg1.load();
    uint16_t reg2 = globalState.FPGA_reg2.load();
    uint16_t reg3 = globalState.FPGA_reg3.load();

    uint32_t readsPerSecond = globalState.readsPerSecond.load();
    uint32_t packetsPerSecond = globalState.packetsPerSecond.load();
    uint32_t shortPacketCounter = globalState.shortPacketCounter.load();

    LimitState state = Green;

    float yellowHighReadsPerSecond = 30.0f; // WAG
    float redHighReadsPerSecond = 31.0f; // WAG
    float yellowLowReadsPerSecond = 12.9f; // WAG
    float redLowReadsPerSecond = 11.9f; // WAG

    float yellowHighPacketsPerSecond = 850.0f; // WAG
    float redHighPacketsPerSecond = 900.0f; // WAG
    float yellowLowPacketsPerSecond = 1.9f; // WAG
    float redLowPacketsPerSecond = 1.8f; // WAG

    if ( (shortPacketCounter > 0) || (packetsPerSecond > yellowHighPacketsPerSecond) || (readsPerSecond > yellowHighReadsPerSecond) ||
        (packetsPerSecond < yellowLowPacketsPerSecond) || (readsPerSecond < yellowLowReadsPerSecond) )
    {
        //set yellow color
        state = Yellow;
    }
    if ( (shortPacketCounter > 0) || (packetsPerSecond > redHighPacketsPerSecond) || (readsPerSecond > redHighReadsPerSecond) ||
        (packetsPerSecond < redLowPacketsPerSecond) || (readsPerSecond < redLowReadsPerSecond) )
    {
        //set red hi color
        state = Red;
    }

    bool isTreeNodeFPGARegistersOpen = ImGui::TreeNodeEx("FPGA Registers", ImGuiTreeNodeFlags_DefaultOpen);
    addFilledCircleToTreeNode(state);

    if (isTreeNodeFPGARegistersOpen) {
        renderInputTextWithColor("Reg 0", reg0, 6, false, 0.0, 0.9);
        renderInputTextWithColor("Reg 1", reg1, 6, false, 0.0, 0.9);
        renderInputTextWithColor("Reg 2", reg2, 6, false, 0.0, 0.9);
        renderInputTextWithColor("Reg 3", reg3, 6, false, 0.0, 0.9);

        renderInputTextWithColor("64k Reads/s", readsPerSecond, 6, true, yellowHighReadsPerSecond, redHighReadsPerSecond, yellowLowReadsPerSecond, redLowReadsPerSecond);
        // to convert reads per second to Mb/s div by 2 (read/s * 65536Bytes/read * 8bits/Byte * 1Mb/(1024*1024bits)=Mb/s )
        float mBps = (readsPerSecond >> 1); // * 65536.0f * 8.0f/ 1024.0f / 1024.0f is same as 2^16 * 2^3 / 2^10 / 2^10 = 2^-1

        float yellowHighmBps = 18.0f; // WAG
        float redHighmBps = 21.0f; // WAG
        float yellowLowmBps = 6.9f; // WAG
        float redLowmBps = 0.51f; // WAG

        renderInputTextWithColor("USB Mb/s", mBps, 6, true, yellowHighmBps, redHighmBps, yellowLowmBps, redLowmBps, "%.1f");

        renderInputTextWithColor("pkt/s", packetsPerSecond, 6, true, yellowHighPacketsPerSecond, redHighPacketsPerSecond, yellowLowPacketsPerSecond, redLowPacketsPerSecond);

        float yellowHighShortPacketCounter = 0.5f;
        float redHighShortPacketCounter = 0.9f;
        renderInputTextWithColor("short pkts", shortPacketCounter, 6, true, yellowHighShortPacketCounter, redHighShortPacketCounter);

        ImGui::TreePop();
    }

    int FIFOTxEmpty = (reg0 & 0x01);
    int FIFORxEmpty = (reg0 >> 1) & 0x01;
    int FIFORxError = (reg0 >> 2) & 0x01;
    //reg3 is temperature
    float temperature = (reg3 >> 4) * 503.975f / 4096.0f - 273.15f; // usually in the high 30s

    float yellowHighTemp = 50.0f;
    float redHighTemp = 60.0f; // XEM7310 spec says +70 is max operating temperature
    float yellowLowTemp = 15.0f;
    float redLowTemp = 10.0f;

    state = Green;

    if (( temperature > yellowHighTemp) || (temperature < yellowLowTemp) || (FIFORxEmpty) || (FIFORxError)){
        state = Yellow;
    }
    if ((FIFORxError) || (temperature > redHighTemp) || (temperature < redLowTemp)){
        state = Red;
    }

    bool isTreeNodeFPGAConvertedOpen = ImGui::TreeNodeEx("FPGA Converted", ImGuiTreeNodeFlags_DefaultOpen);
    addFilledCircleToTreeNode(state);

    if (isTreeNodeFPGAConvertedOpen) {
        //reg0 is multiple status bits
        renderInputTextWithColor("FIFO TxEmpty", FIFOTxEmpty, 12, false, 0.0, 0.9);
        renderInputTextWithColor("FIFO RxEmpty", FIFORxEmpty, 12, true, 0.9, 1.9);
        renderInputTextWithColor("FIFO RxError", FIFORxError, 12, true, 0.0, 0.9);
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

        renderInputTextWithColor("FPGA Temp (C)", temperature, 12, true, yellowHighTemp, redHighTemp, yellowLowTemp, redLowTemp, "%.1f");

        ImGui::TreePop();
    }
    ImGui::End(); // end of FPGA Status
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

    int screenWidth = 1200;
    int screenHeight = 800;

    if (globalState.args.fullScreen.load()) {
        // determine screen resolution
        const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
        screenWidth = mode->width;
        screenHeight = mode->height;
    }

    // Create window with graphics context
    GLFWwindow* window = glfwCreateWindow(screenWidth, screenHeight, "SDO-EVE Rocket L0b - Dear ImGui GLFW+OpenGL3", nullptr, nullptr);
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
    bool show_demo_window = false;
    //bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    SetRainbowCustomColormap(true);
    SetRainbowCustomColormap(false);
    
    //std::lock_guard<std::mutex> lock(mtx); // lock the mutex
    mtx.lock();
    GLuint megsATextureID = createProperTextureFromMEGSImage(&globalState.megsa.image, globalGUI.Image_Display_Scale_MA, true);
    GLuint megsBTextureID = createProperTextureFromMEGSImage(&globalState.megsb.image, globalGUI.Image_Display_Scale_MB, false);
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
    // mtx.lock();
    // GLuint mbSimpleTextureID = createProperTextureFromMEGSImage(testimg, MEGS_IMAGE_WIDTH, MEGS_IMAGE_HEIGHT, true, true);
    // mtx.unlock();

    // Main loop
#ifdef __EMSCRIPTEN__
    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.IniFilename = nullptr;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while ((!glfwWindowShouldClose(window)) && (globalState.running.load()))
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
            displayFPGAStatus();
        }

        {
            //mtx.lock();
            displayMAImageWithControls(megsATextureID);
            displayMBImageWithControls(megsBTextureID);
            //mtx.unlock();

            // 3. Show another simple window.
            {
                updateStatusWindow();

            }
            {
                // show raw packets in separate window
                updateRawPacketWindow();
            }

            {
                updateESPWindow();
            }

            {
                updateSHKWindow();
            }

            {
                updateControlWindow();
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

        //ImGui::ShowMetricsWindow(); // used for debugging

        // Rendering
        ImGui::Render();
        int display_w, display_h;

        // this is the part that draws stuff to the screen
        {
            if (globalState.megsAUpdated.load(std::memory_order_relaxed)) {
                mtx.lock();
                renderUpdatedTextureFromMEGSAImage(megsATextureID);
                mtx.unlock();
                globalState.megsAUpdated.store(false, std::memory_order_relaxed);  // Reset flag after updating texture
            }

            if (globalState.megsBUpdated.load(std::memory_order_relaxed)) {
                mtx.lock();
                renderUpdatedTextureFromMEGSBImage(megsBTextureID);
                mtx.unlock();
                // debugging image display
                // renderSimpleTextureMB(mbSimpleTextureID, testimg);
                globalState.megsBUpdated.store(false, std::memory_order_relaxed);  // Reset flag after updating texture
            }

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
