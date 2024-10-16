//This deals with running the demo. Main runs this in a separate thread.

#include "eve_l0b.hpp"
#include "commonFunctions.hpp"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include <stdio.h>
#include <vector>
#include <iostream>

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

// This function is only thread-safe if image and transposeData are not accessed elsewhere during execution.
// Callers are expected to use megsAUpdated and megsBUpdated flags to prevent access during modification.
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

void renderInputTextWithColor(const char* label, char* buffer, size_t bufferSize, bool limitCheck, float value, float lowerLimit, float upperLimit) {
    LimitState state = NoCheck;
    
    float_t itemWidthValue = ImGui::GetFontSize() * 6;
    ImGui::PushItemWidth(itemWidthValue);

    if (limitCheck) {
        if (value > upperLimit) {
            state = Red;
        } else if (value > upperLimit * 0.9f) { // Near violation (adjust the threshold as needed)
            state = Yellow;
        } else {
            state = Green;
        }
    }

    // Set color before rendering
    ImGui::PushStyleColor(ImGuiCol_FrameBg, getColorForState(state));

    // Render the InputText
    ImGui::InputText(label, buffer, bufferSize);

    // Restore default color
    ImGui::PopStyleColor();
}

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// initialize a texture for a MEGS image
GLuint createTextureFromMEGSImage(uint16_t* data, int width, int height, bool modulo256 = false, bool scale = true)
{
    std::vector<uint8_t> textureData(width * height); // 8-bit data for display

    // Fill textureData with processed pixel values
    for (int i = 0; i < width * height; ++i) {
        uint16_t value = data[i];

        if (modulo256) {
            textureData[i] = static_cast<uint8_t>(value & 0xFF); // Modulo 256
        } else if (scale) {
            textureData[i] = static_cast<uint8_t>((value & 0x3FFF) >> 6); // Scale 14 bits to 8 bits
        }
    }

    // Generate and bind a new texture
    GLuint textureID;
    glGenTextures(1, &textureID);        // Generate the texture ID
    glBindTexture(GL_TEXTURE_2D, textureID);  // Bind the texture

    // Set texture filtering parameters
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Upload the texture data to OpenGL
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, textureData.data());


    // Unbind the texture
    glBindTexture(GL_TEXTURE_2D, 0);

    return textureID;  // Return the generated texture ID
}

//update the texture whenever MEGS-A changes
void updateTextureFromMEGSAImage(GLuint textureID)
{
    glBindTexture(GL_TEXTURE_2D, textureID);
    transposeImage2D(globalState.megsa.image, globalState.transMegsA);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, MEGS_IMAGE_WIDTH, MEGS_IMAGE_HEIGHT, GL_RED, GL_UNSIGNED_BYTE, globalState.transMegsA);
    //glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, MEGS_IMAGE_WIDTH, MEGS_IMAGE_HEIGHT, GL_RED, GL_UNSIGNED_BYTE, globalState.megsa.image);
    glBindTexture(GL_TEXTURE_2D, textureID);
}

void updateTextureFromMEGSBImage(GLuint megsBTextureID)
{
    glBindTexture(GL_TEXTURE_2D, megsBTextureID);
    transposeImage2D(globalState.megsb.image, globalState.transMegsB);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, MEGS_IMAGE_T_WIDTH, MEGS_IMAGE_T_HEIGHT, GL_RED, GL_UNSIGNED_BYTE, globalState.transMegsB);
    glBindTexture(GL_TEXTURE_2D, megsBTextureID);

    //glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, MEGS_IMAGE_WIDTH, MEGS_IMAGE_HEIGHT, GL_RED, GL_UNSIGNED_BYTE, globalState.megsb.image);

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
}

void renderMBImageWithZoom(GLuint megsBTextureID, uint16_t* data, int fullWidth, int fullHeight, float zoom, ImVec2 viewportSize, bool modulo256, bool scale)
{
    // Calculate the zoom level
    float value = 1.0 / zoom;
    
    // default
    ImVec2 uv0(0.0f, 0.0f); 
    ImVec2 uv1(value, value); 

    // Render the image using the texture ID
    ImGui::Image((void*)(intptr_t)megsBTextureID, viewportSize, uv0, uv1);

    // Optionally, delete the texture after rendering if it’s not needed anymore
    //glDeleteTextures(1, &textureID);
}

void displayMAImageWithControls(GLuint megsATextureID)
{
    static float zooma = 0.5f;         // Zoom level (1.0 = full resolution)
    static bool modulo256a = true;    // Modulo 256 display
    static bool scalea = false;         // Scaled view

    ImGui::Begin("MEGS-A Image Viewer");

    // Viewport size for display (1024x512)
    ImVec2 viewportSizea = ImVec2(1024.0f, 512.0f);
    
    // Zoom slider
    ImGui::SliderFloat("MA Zoom", &zooma, 0.25f, 4.0f, "MA Zoom %.1fx");
    
    // Toggle for scaled or modulo 256 view
    ImGui::Checkbox("MA Modulo 256", &modulo256a);
    scalea = !modulo256a;

    //ImGui::Text("MA Time: %s",globalState.megsa.iso8601.c_str());

    //ImGui::Text("MA ParityErrorCount %ld",globalState.parityErrorsMA);

    // Render the image with the current zoom level
    renderMAImageWithZoom(megsATextureID, reinterpret_cast<uint16_t*>(globalState.transMegsA), MEGS_IMAGE_WIDTH, MEGS_IMAGE_HEIGHT, zooma, viewportSizea, modulo256a, scalea);

    ImGui::End();
}
void displayMBImageWithControls(GLuint megsBTextureID)
{
    static float zoom = 0.5f;         // Zoom level (1.0 = full resolution)
    static bool modulo256 = true;    // Modulo 256 display
    static bool scale = false;         // Scaled view

    ImGui::Begin("MEGS-B Image Viewer");

    // Viewport size for display (1024x512)
    ImVec2 viewportSize = ImVec2(1024.0f, 512.0f);
    
    // Zoom slider
    ImGui::SliderFloat("MB Zoom", &zoom, 0.25f, 4.0f, "MB Zoom %.1fx");
    
    // Toggle for scaled or modulo 256 view
    ImGui::Checkbox("MB Modulo 256", &modulo256);
    scale = !modulo256;

    ImGui::Text("MB Time: %s",globalState.megsb.iso8601.c_str());


    // Render the image with the current zoom level
    renderMBImageWithZoom(megsBTextureID, reinterpret_cast<uint16_t*>(globalState.transMegsB), MEGS_IMAGE_T_WIDTH, MEGS_IMAGE_T_HEIGHT, zoom, viewportSize, modulo256, scale);
    //renderMBImageWithZoom(megsBTextureID, reinterpret_cast<uint16_t*>(globalState.megsb.image), MEGS_IMAGE_WIDTH, MEGS_IMAGE_HEIGHT, zoom, viewportSize, modulo256, scale);
    //renderMBImageWithZoom(megsBTextureID, transposeImage(globalState.megsb.image).data(), MEGS_IMAGE_WIDTH, MEGS_IMAGE_HEIGHT, zoom, viewportSize, modulo256, scale);
    
    ImGui::End();
}

void updateStatusWindow()
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("Refresh rate: %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
    renderInputTextWithColor("601 a59 MEGS-A Pkts", strdup((std::to_string(globalState.packetsReceived.MA)).c_str()), 12, false, static_cast<float>(globalState.packetsReceived.MA), 0.0, 0.9);
    renderInputTextWithColor("602 a5a MEGS-B Pkts", strdup((std::to_string(globalState.packetsReceived.MB)).c_str()), 12, false, static_cast<float>(globalState.packetsReceived.MB), 0.0, 0.9);
    renderInputTextWithColor("604 a5c ESP Pkts", strdup((std::to_string(globalState.packetsReceived.ESP)).c_str()), 12, false, static_cast<float>(globalState.packetsReceived.ESP), 0.0, 0.9);
    renderInputTextWithColor("605 a5d MEGS-P Pkts", strdup((std::to_string(globalState.packetsReceived.MP)).c_str()), 12, false, static_cast<float>(globalState.packetsReceived.MP), 0.0, 0.9);
    renderInputTextWithColor("606 a5e SHK Pkts", strdup((std::to_string(globalState.packetsReceived.SHK)).c_str()), 12, false, static_cast<float>(globalState.packetsReceived.SHK), 0.0, 0.9);
    renderInputTextWithColor("Unknown Packets", strdup((std::to_string(globalState.packetsReceived.Unknown)).c_str()), 12, true, static_cast<float>(globalState.packetsReceived.Unknown), 0.0, 0.9);

    renderInputTextWithColor("MEGS-A Gap Count", strdup((std::to_string(globalState.dataGapsMA)).c_str()), 12, true, static_cast<float>(globalState.dataGapsMA), 0.0, 0.9);
    renderInputTextWithColor("MEGS-B Gap Count", strdup((std::to_string(globalState.dataGapsMB)).c_str()), 12, true, static_cast<float>(globalState.dataGapsMB), 0.0, 0.9);
    renderInputTextWithColor("MEGS-P Gap Count", strdup((std::to_string(globalState.dataGapsMP)).c_str()), 12, true, static_cast<float>(globalState.dataGapsMP), 0.0, 0.9);
    renderInputTextWithColor("ESP Gap Count", strdup((std::to_string(globalState.dataGapsESP)).c_str()), 12, true, static_cast<float>(globalState.dataGapsESP), 0.0, 0.9);
    renderInputTextWithColor("SHK Gap Count", strdup((std::to_string(globalState.dataGapsSHK)).c_str()), 12, true, static_cast<float>(globalState.dataGapsSHK), 0.0, 0.9);


    renderInputTextWithColor("MEGS-A Parity Errors", strdup((std::to_string(globalState.parityErrorsMA)).c_str()), 12, true, static_cast<float>(globalState.parityErrorsMA), 0.0, 0.9);
    renderInputTextWithColor("MEGS-B Parity Errors", strdup((std::to_string(globalState.parityErrorsMB)).c_str()), 12, true, static_cast<float>(globalState.parityErrorsMB), 0.0, 0.9);
}

void updateESPWindow()
{
    // need to find the last populated index
    int index = ESP_INTEGRATIONS_PER_FILE - 1;
    while ((index > 1) && (globalState.esp.ESP_xfer_cnt[index] == 0)) 
    {
        index--;
    }
    // std::cout<< "index: " << index << std::endl;
    //int index=0;

    // Column 1
    ImGui::Columns(2,"ESP Columns");
    ImGui::Text("ESP Status Column");
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10);
    ImGui::Text("ESP packet time: %s",globalState.esp.iso8601.c_str());

    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6);
    ImGui::InputText("ESP xfer cnt", strdup((std::to_string(globalState.esp.ESP_xfer_cnt[index])).c_str()), 12, ImGuiInputTextFlags_ReadOnly);
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6);
    ImGui::InputText("ESP q0", strdup((std::to_string(globalState.esp.ESP_q0[index])).c_str()), 12, ImGuiInputTextFlags_ReadOnly);
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6);
    ImGui::InputText("ESP q1", strdup((std::to_string(globalState.esp.ESP_q1[index])).c_str()), 12, ImGuiInputTextFlags_ReadOnly);
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6);
    ImGui::InputText("ESP q2", strdup((std::to_string(globalState.esp.ESP_q2[index])).c_str()), 12, ImGuiInputTextFlags_ReadOnly);
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6);
    ImGui::InputText("ESP q3", strdup((std::to_string(globalState.esp.ESP_q3[index])).c_str()), 12, ImGuiInputTextFlags_ReadOnly);
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6);
    ImGui::InputText("ESP 171", strdup((std::to_string(globalState.esp.ESP_171[index])).c_str()), 12, ImGuiInputTextFlags_ReadOnly);
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6);
    ImGui::InputText("ESP 257", strdup((std::to_string(globalState.esp.ESP_257[index])).c_str()), 12, ImGuiInputTextFlags_ReadOnly);
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6);
    ImGui::InputText("ESP 304", strdup((std::to_string(globalState.esp.ESP_304[index])).c_str()), 12, ImGuiInputTextFlags_ReadOnly);
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6);
    ImGui::InputText("ESP 366", strdup((std::to_string(globalState.esp.ESP_366[index])).c_str()), 12, ImGuiInputTextFlags_ReadOnly);
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6);
    ImGui::InputText("ESP dark", strdup((std::to_string(globalState.esp.ESP_dark[index])).c_str()), 12, ImGuiInputTextFlags_ReadOnly);

    // Column 2
    ImGui::NextColumn();
    ImGui::Text("ESP Plots Column");

    // reset to single column layout
    ImGui::Columns(1);
    globalState.espUpdated = false;

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
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Dear ImGui GLFW+OpenGL3 example", nullptr, nullptr);
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
    //GLuint megsATextureID = createTextureFromMEGSImage( &globalState.megsa.image[0][0], MEGS_IMAGE_WIDTH, MEGS_IMAGE_HEIGHT, true, true);
    //GLuint megsBTextureID = createTextureFromMEGSImage( &globalState.megsb.image[0][0], MEGS_IMAGE_T_WIDTH, MEGS_IMAGE_T_HEIGHT, true, true);
    GLuint megsATextureID = createTextureFromMEGSImage( &globalState.transMegsA[0][0], MEGS_IMAGE_WIDTH, MEGS_IMAGE_HEIGHT, true, true);
    GLuint megsBTextureID = createTextureFromMEGSImage( &globalState.transMegsB[0][0], MEGS_IMAGE_T_WIDTH, MEGS_IMAGE_T_HEIGHT, true, true);
    mtx.unlock(); // unlock the mutex

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

            // 3. Show another simple window.
            {
                ImGui::Begin("Status Window");
                updateStatusWindow();
                ImGui::End();
            }

            if (globalState.espUpdated)
            {
                ImGui::Begin("ESP Window");
                updateESPWindow();
                globalState.espUpdated = false;
                ImGui::End();
            }
            mtx.unlock();
        }

        // Rendering
        ImGui::Render();
        int display_w, display_h;

        // this is the part that draws stuff to the screen
        {
            mtx.lock();
            if (globalState.megsAUpdated) {
                updateTextureFromMEGSAImage(megsATextureID);
                globalState.megsAUpdated = false;  // Reset flag after updating texture
            }
            if (globalState.megsBUpdated) {
                updateTextureFromMEGSBImage(megsBTextureID);
                globalState.megsBUpdated = false;  // Reset flag after updating texture
            }
            if (globalState.espUpdated) {
                updateESPWindow();
                globalState.espUpdated = false;
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

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
