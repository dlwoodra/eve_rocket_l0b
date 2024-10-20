//This deals with running the demo. Main runs this in a separate thread.

#include "eve_l0b.hpp"
#include "commonFunctions.hpp"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include "implot.h"
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

enum LimitState {
    NoCheck,
    Green,
    Yellow,
    Red
};

// Function to determine the color based on state
ImVec4 getColorForState(LimitState state) {
    switch (state) {
        case Green: return ImVec4(0.0f, 0.5f, 0.0f, 1.0f);  // Green
        case Yellow: return ImVec4(0.4f, 0.4f, 0.0f, 1.0f); // Yellow
        case Red: return ImVec4(0.5f, 0.0f, 0.0f, 1.0f);    // Red
        default: return ImGui::GetStyleColorVec4(ImGuiCol_FrameBg); // Default color
    }
}

float mazoom = 0.25f;         // Zoom level (1.0 = full resolution)
float mbzoom = 0.25f;         // Zoom level (1.0 = full resolution)
bool mamodulo256 = true;    // Modulo 256 display
bool mbmodulo256 = true;    // Modulo 256 display


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

// This function is only thread-safe if image and transposeData are not accessed elsewhere during execution.
// Callers are expected to use mtx to lock access during modification.
// switch x and y 
void transposeImage2D(const uint16_t (*image)[MEGS_IMAGE_HEIGHT], uint16_t (*transposeData)[MEGS_IMAGE_WIDTH]) {
    const uint32_t width = MEGS_IMAGE_WIDTH;
    const uint32_t height = MEGS_IMAGE_HEIGHT;

    // approx mean time 17-19 ms - Winner!
    // single thread
    for (uint32_t x = 0; x < width; ++x) {
        for (uint32_t y = 0; y < height; ++y) {
            transposeData[y][x] = image[x][y];
        }
    }
}

// Replace image with histogram equalized version, 8-bits per pixel
void histogramEqualization(uint8_t* image, int width, int height) {
    // Step 1: Calculate the histogram
    int histogram[256] = {0};
    for (int i = 0; i < width * height; ++i) {
        histogram[image[i]]++;
    }

    // Step 2: Compute the cumulative distribution function (CDF)
    int cdf[256] = {0};
    cdf[0] = histogram[0];
    for (int i = 1; i < 256; ++i) {
        cdf[i] = cdf[i - 1] + histogram[i];
    }

    // Step 3: Normalize the CDF
    float cdf_min = *std::min_element(cdf, cdf + 256);
    float cdf_range = width * height - cdf_min;
    uint8_t new_image[width * height];
    
    for (int i = 0; i < width * height; ++i) {
        new_image[i] = static_cast<uint8_t>(255.0f * (cdf[image[i]] - cdf_min) / cdf_range);
    }

    // Copy the new image back
    std::memcpy(image, new_image, width * height);
}

void renderInputTextWithColor(const char* label, long value, size_t bufferSize, bool limitCheck, float lowerLimit, float upperLimit) {
    LimitState state = NoCheck;
    
    float_t itemWidthValue = ImGui::GetFontSize() * 6;
    ImGui::PushItemWidth(itemWidthValue);

    char strval[bufferSize];
    snprintf(strval, bufferSize, "%ld", value);

    if (limitCheck) {
        const float fvalue = static_cast<float>(value); // fvallue is only used for limit checking
        if (fvalue > upperLimit) {
            state = Red;
        } else if (fvalue > upperLimit * 0.9f) { // Near violation (adjust the threshold as needed)
            state = Yellow;
        } else {
            state = Green;
        }
    }

    // Set color before rendering
    ImGui::PushStyleColor(ImGuiCol_FrameBg, getColorForState(state));

    // Render the InputText
    ImGui::InputText(label, strval, bufferSize);

    // free(strval); // No need to free stack-allocated memory

    // Restore default color
    ImGui::PopStyleColor();
}

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// initialize a texture for a MEGS image
GLuint createProperTextureFromMEGSImage(uint16_t (*data)[MEGS_IMAGE_HEIGHT], int width, int height, bool modulo256 = false, bool scale = true) {
    std::vector<uint8_t> textureData(width * height); // 8-bit data for display

    // Transpose the image data (90-degree clockwise rotation)
    for (int x = 0; x < width; ++x) {
        for (int y = 0; y < height; ++y) {
            int index = y*width + x; //1d translation, no transpose
            uint16_t value = data[x][y]; // Get the original value

            // Process the value into the textureData based on modulo256 or scale options
            if (modulo256) {
                textureData[index] = static_cast<uint8_t>(value & 0xFF); // Modulo 256
            } else if (scale) {
                textureData[index] = static_cast<uint8_t>((value & 0x3FFF) >> 6); // Scale 14 bits to 8 bits
            }
        }
    }

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
    //glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, textureData.data());
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, textureData.data());

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

    // Populate textureData directly from the 2D array
    for (int x = 0; x < width; ++x) {
        for (int y = 0; y < height; ++y) {
            int index = y * width + x; // 1D index in textureData
            if (mamodulo256) {
                textureData[index] = static_cast<uint8_t>(globalState.megsa.image[x][y] & 0xFF); // Modulo 256
            } else {
                textureData[index] = static_cast<uint8_t>((globalState.megsa.image[x][y] & 0x3FFF) >> 6); // Scale 14 bits to 8 bits
            }
        }
    }
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, MEGS_IMAGE_WIDTH, MEGS_IMAGE_HEIGHT, GL_RED, GL_UNSIGNED_BYTE, textureData.data());
    glBindTexture(GL_TEXTURE_2D, textureID);
}

void renderUpdatedTextureFromMEGSBImage(GLuint megsBTextureID)
{
    int width=MEGS_IMAGE_WIDTH;
    int height=MEGS_IMAGE_HEIGHT;
    std::vector<uint8_t> textureData(width * height);

    // Populate textureData directly from the 2D array
    for (int x = 0; x < width; ++x) {
        for (int y = 0; y < height; ++y) {
            int index = y * width + x; // 1D index in textureData
            if (mbmodulo256) {
                textureData[index] = static_cast<uint8_t>(globalState.megsb.image[x][y] & 0xFF); // Modulo 256
            } else {
                textureData[index] = static_cast<uint8_t>((globalState.megsb.image[x][y] & 0x3FFF) >> 6); // Scale 14 bits to 8 bits
            }
        }
    }
    glBindTexture(GL_TEXTURE_2D, megsBTextureID);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, MEGS_IMAGE_WIDTH, MEGS_IMAGE_HEIGHT, GL_RED, GL_UNSIGNED_BYTE, textureData.data());
    glBindTexture(GL_TEXTURE_2D, megsBTextureID);
}

void renderMAImageWithZoom(GLuint megsATextureID, uint16_t* data, int fullWidth, int fullHeight, float zoom, ImVec2 viewportSize, bool modulo256, bool scale)
{
    // Calculate the zoom level
    float value = 1.0 / zoom;
    
    // default
    ImVec2 uv0(0.0f, 0.0f); 
    ImVec2 uv1(value, value); 

    // Render the image using the texture ID
    ImGui::Image((void*)(intptr_t)megsATextureID, viewportSize, uv0, uv1);
    //ImGui::Image((void*)(intptr_t)megsATextureID, ImVec2(2048.0f*0.25f, 1024.0f*0.25f), ImVec2(0.0f,0.0f), ImVec2(1.0f,1.0f));
}

// void renderMBImageWithZoom(GLuint megsBTextureID, uint16_t* data, int fullWidth, int fullHeight, float zoom, ImVec2 viewportSize, bool modulo256, bool scale)
// {
//     // Calculate the zoom level
//     float value = 1.0 / zoom;
    
//     // default
//     ImVec2 uv0(0.0f, 0.0f); 
//     ImVec2 uv1(value, value); 

//     // Render the image using the texture ID
//     //ImGui::Image((void*)(intptr_t)megsBTextureID, viewportSize, uv0, uv1);
//     ImGui::Image((void*)(intptr_t)megsBTextureID, ImVec2(MEGS_IMAGE_WIDTH*0.25f,MEGS_IMAGE_HEIGHT*0.25), uv0, uv1);

//     // re-use textures, delete them in cleanup section
// }

void displayMAImageWithControls(GLuint megsATextureID)
{
    //static float zooma = 0.5f;         // Zoom level (1.0 = full resolution)
    //static bool modulo256a = true;    // Modulo 256 display
    //static bool scalea = false;         // Scaled view

    ImGui::Begin("MEGS-A Image Viewer");

    // Viewport size for display (1024x512)
    ImVec2 viewportSizea = ImVec2(1024.0f, 512.0f);
    //ImVec2 viewportSizea = ImVec2(2048.0f, 1024.0f); // works, but we need to shrink it
    
    // Zoom slider
    ImGui::SliderFloat("MA Zoom", &mazoom, 0.25f, 4.0f, "MA Zoom %.1fx");
    
    // Toggle for scaled or modulo 256 view
    ImGui::Checkbox("MA Modulo 256", &mamodulo256);
    //scalea = !modulo256a;

    std::string iso8601 = tai_to_iso8601(globalState.megsa.tai_time_seconds);
    char* tmpiISO8601 = const_cast<char*>(iso8601.c_str());
    ImGui::Text("MA 1st pkt: %s",tmpiISO8601);

    for (uint32_t x = 0; x < MEGS_IMAGE_WIDTH; x++) {
        for (uint32_t y = 0; y < MEGS_IMAGE_HEIGHT; y++) {
            if (mamodulo256) {
                globalState.sclMegsA[x][y] = globalState.megsa.image[x][y] & 0xFF; // modulo 256
            } else {
                globalState.sclMegsA[x][y] = globalState.megsa.image[x][y] >> 6; //scale 14 bits to 8 bits
            }
        }
    }

    float value = 1.0f; // changing this will crop the image
    //ImGui::Image((void*)(intptr_t)megsBTextureID, ImVec2(MEGS_IMAGE_WIDTH*0.25f,MEGS_IMAGE_HEIGHT*0.25), ImVec2(0.0f,0.0f), ImVec2(value,value));
    ImGui::Image((void*)(intptr_t)megsATextureID, ImVec2(MEGS_IMAGE_WIDTH*mazoom,MEGS_IMAGE_HEIGHT*mazoom), ImVec2(0.0f,0.0f), ImVec2(value,value));


    ImGui::End();
}

void displayMBImageWithControls(GLuint megsBTextureID)
{
    ImGui::Begin("MEGS-B Image Viewer");

    // Viewport size for display (1024x512)
    //ImVec2 viewportSize = ImVec2(1024.0f, 512.0f);
    
    // Zoom slider
    ImGui::SliderFloat("MB Zoom", &mbzoom, 0.25f, 1.0f, "MB Zoom %.1fx");
    
    // Toggle for scaled or modulo 256 view
    ImGui::Checkbox("MB Modulo 256", &mbmodulo256);

    std::string iso8601 = tai_to_iso8601(globalState.megsb.tai_time_seconds);
    char* tmpiISO8601 = const_cast<char*>(iso8601.c_str());
    ImGui::Text("MB 1st pkt: %s",tmpiISO8601);

    // Render the image with the current zoom level
    //renderMBImageWithZoom(megsBTextureID, reinterpret_cast<uint16_t*>(globalState.transMegsB), MEGS_IMAGE_T_WIDTH, MEGS_IMAGE_T_HEIGHT, zoom, viewportSize, modulo256, scale);

    for (uint32_t x = 0; x < MEGS_IMAGE_WIDTH; x++) {
        for (uint32_t y = 0; y < MEGS_IMAGE_HEIGHT; y++) {
            if (mbmodulo256) {
                globalState.sclMegsB[x][y] = globalState.megsb.image[x][y] & 0xFF; // modulo 256
            } else {
                globalState.sclMegsB[x][y] = globalState.megsb.image[x][y] >> 6; //scale 14 bits to 8 bits
            }
        }
    }

    float value = 1.0f; // changing this will crop the image
    //ImGui::Image((void*)(intptr_t)megsBTextureID, ImVec2(MEGS_IMAGE_WIDTH*0.25f,MEGS_IMAGE_HEIGHT*0.25), ImVec2(0.0f,0.0f), ImVec2(value,value));
    ImGui::Image((void*)(intptr_t)megsBTextureID, ImVec2(MEGS_IMAGE_WIDTH*mbzoom,MEGS_IMAGE_HEIGHT*mbzoom), ImVec2(0.0f,0.0f), ImVec2(value,value));

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

    renderInputTextWithColor("MEGS-A Gap Count", globalState.dataGapsMA, 12, true, 0.0, 0.9);
    renderInputTextWithColor("MEGS-B Gap Count", globalState.dataGapsMB, 12, true, 0.0, 0.9);
    renderInputTextWithColor("MEGS-P Gap Count", globalState.dataGapsMP, 12, true, 0.0, 0.9);
    renderInputTextWithColor("ESP Gap Count", globalState.dataGapsESP, 12, true, 0.0, 0.9);
    renderInputTextWithColor("SHK Gap Count", globalState.dataGapsSHK, 12, true, 0.0, 0.9);

    renderInputTextWithColor("MEGS-A Parity Errors", globalState.parityErrorsMA, 12, true, 0.0, 0.9);
    renderInputTextWithColor("MEGS-B Parity Errors", globalState.parityErrorsMB, 12, true, 0.0, 0.9);
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
    ImGui::Columns(2,"ESP Columns");
    ImGui::Text("ESP Status Column");
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10);

    std::string iso8601 = tai_to_iso8601(globalState.esp.tai_time_seconds);
    const char* tmpiISO8601 = iso8601.c_str();
    ImGui::Text("pkt:%s", tmpiISO8601);

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

    // Column 2
    ImGui::NextColumn();
    ImGui::Text("ESP Plots Column");

    // reset to single column layout
    ImGui::Columns(1);

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
    //GLuint megsATextureID = createTextureFromMEGSImage( &globalState.transMegsA[0][0], MEGS_IMAGE_WIDTH, MEGS_IMAGE_HEIGHT, true, true);
    //GLuint megsBTextureID = createTextureFromMEGSImage( &globalState.transMegsB[0][0], MEGS_IMAGE_T_WIDTH, MEGS_IMAGE_T_HEIGHT, true, true);
    GLuint megsATextureID = createProperTextureFromMEGSImage( globalState.megsa.image, MEGS_IMAGE_WIDTH, MEGS_IMAGE_HEIGHT, true, true);
    GLuint megsBTextureID = createProperTextureFromMEGSImage( globalState.megsb.image, MEGS_IMAGE_WIDTH, MEGS_IMAGE_HEIGHT, true, true);
    mtx.unlock(); // unlock the mutex

    uint16_t testimg[MEGS_IMAGE_WIDTH][MEGS_IMAGE_HEIGHT];
    populatePattern(testimg);
    for (int x = 0; x < 10; ++x) { // verify there is nonzero data in the testimg
        for (int y = 0; y < 10; ++y) {
            std::cout << testimg[x][y] << " "; // Print first 10x10 pixels
        }
        std::cout << std::endl;
    }
    GLuint mbSimpleTextureID = createProperTextureFromMEGSImage(testimg, MEGS_IMAGE_WIDTH, MEGS_IMAGE_HEIGHT, true, true);

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

            ImGui::Begin("SDO-EVE Rocket Data Display");                          // Create a window called "Hello, world!" and append into it.
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);

            ImGui::End();
        }

        {
            mtx.lock();
            displayMAImageWithControls(megsATextureID);
            displayMBImageWithControls(megsBTextureID);
            mtx.unlock();

            // 3. Show another simple window.
            {
                ImGui::Begin("Status Window");
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
            // {
            //     ImGui::Begin("MEGS-A Image Viewer");
            //     ImGui::Text("MEGS-A Image Viewer");
            //     ImGui::End();
            // }
            {
                ImGui::Begin("MEGS-B Image Simple Viewer");
                ImGui::Text("MEGS-B Image Simple Text");
                mtx.lock();
                displaySimpleMB(mbSimpleTextureID);
                mtx.unlock();
                ImGui::End();
            }
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
                renderSimpleTextureMB(mbSimpleTextureID, testimg);
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
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteTextures(1, &megsATextureID);
    glDeleteTextures(1, &megsBTextureID);
    glDeleteTextures(1, &mbSimpleTextureID);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
