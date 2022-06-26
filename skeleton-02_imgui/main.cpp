#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <vector>
#include <random>

struct App {
    GLFWwindow *window;
    glm::ivec2 windowSize;

    glm::ivec2 textureSize;
    std::vector<glm::vec4> pixels;

    GLuint texture;
    GLuint framebuffer;

    std::default_random_engine rng;
    std::uniform_real_distribution<float> norm01{0.f, 1.f};
    std::uniform_real_distribution<float> norm11{-1.f, 1.f};

    //// Custom Variables ////

    float texSizeMultiplier = 0.1;

    glm::vec4 u0 = { 0.f, 0.f, 0.f, 1.f };
    glm::vec4 u1 = { 1.f, 0.f, 0.f, 1.f };
    glm::vec4 v0 = { 0.f, 0.f, 0.f, 1.f };
    glm::vec4 v1 = { 0.f, 1.f, 0.f, 1.f };

    int sample = 0;

    //// End of Custom Variables ////

    App()
    {
        // Setup GLFW
        glfwInit();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        window = glfwCreateWindow(800, 600, "Ray Tracing Skeleton 01 - ImGui", nullptr, nullptr);

        // Setup OpenGL
        glfwMakeContextCurrent(window);
        gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
        glfwSwapInterval(1);

        // Register for resize callback
        glfwSetWindowUserPointer(window, this);
        glfwSetWindowSizeCallback(window, [](auto wnd, int w, int h) {
            ((App*)glfwGetWindowUserPointer(wnd))->OnResize(w, h);
        });

        ImGui::CreateContext();
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 330 core");

        // Create OpenGL resources
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glGenFramebuffers(1, &framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

        // Initial window size
        glfwGetWindowSize(window, &windowSize.x, &windowSize.y);
        OnResize(windowSize.x, windowSize.y);
    }

    ~App()
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        glfwDestroyWindow(window);
        glfwTerminate();
    }

    glm::vec4& Pixel(int x, int y) 
    {
        return pixels[y * textureSize.x + x];
    }

    void WritePixelsToTexture()
    {
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, textureSize.x, textureSize.y, 0, GL_RGBA, GL_FLOAT, pixels.data());
    }

    void OnResize(int w, int h)
    {
        windowSize = { w, h };
        
        ResizeTexture(int(w * texSizeMultiplier), int(h * texSizeMultiplier));
    }

    void ResizeTexture(int w, int h)
    {
        // Resize CPU-side pixel storage
        // (OpenGL Resources are automatically resized on WritePixelsToTexture)
        textureSize = { w, h };
        pixels.resize(textureSize.x * textureSize.y);

        sample = 0;
    }

    glm::vec4 CastRay(glm::vec2 ndc)
    {
        // Basic parameterized UV test pattern
        float u = (ndc.x * 0.5f + 0.5f);
        float v = (ndc.y * 0.5f + 0.5f);
        return u0 * (1.f - u) + u1 * u + v0 * (1.f - v) + v1 * v;
    }

    void Sample(float weight, float jitter = 0.f)
    {
        for (int y = 0; y < textureSize.y; ++y) {
            for (int x = 0; x < textureSize.x; ++x) {
                // Compute normalized pixel position in [-1, 1] with some jitter
                glm::vec2 ndc = {
                    (x + 0.5f + jitter * 0.5f * norm11(rng)) * 2.f / textureSize.x - 1.f,
                    (y + 0.5f + jitter * 0.5f * norm11(rng)) * 2.f / textureSize.y - 1.f
                };

                // Compute update pixel value based on weight
                Pixel(x, y) = Pixel(x, y) * (1.f - weight) + weight * CastRay(ndc);
            }
        }
    }

    void Run()
    {
        while (!glfwWindowShouldClose(window)) {
            
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            ImGui::Begin("Settings");

            // Show Details
            ImGui::Text("Sample: %i", sample);
            ImGui::Text("Texture Size: (%i, %i)", textureSize.x, textureSize.y);

            // Change scale of texture relative to window  size
            if (ImGui::SliderFloat("Texture scale", &texSizeMultiplier, 0.02f, 1.f)) {
                ResizeTexture(int(windowSize.x * texSizeMultiplier), int(windowSize.y * texSizeMultiplier));
                sample = 0;
            }

            // Colour change options for UV demo
            bool colorChanged = false;
            colorChanged |= ImGui::ColorEdit4("U0", &u0[0]);
            colorChanged |= ImGui::ColorEdit4("U1", &u1[0]);
            colorChanged |= ImGui::ColorEdit4("V0", &v0[0]);
            colorChanged |= ImGui::ColorEdit4("V1", &v1[0]);
            if (colorChanged) sample = 0;

            ImGui::End();
            
            // Sample using exponential moving average
            if (sample < 100) 
                Sample(1.f / ++sample, 1.f);

            // Just blit the texture directly out to the screen for now!
            WritePixelsToTexture();
            glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            glBlitFramebuffer(
                0, 0, textureSize.x, textureSize.y, 
                0, 0, windowSize.x, windowSize.y, 
                GL_COLOR_BUFFER_BIT, GL_NEAREST);

            // Present
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(window);

            glfwPollEvents();
        }
    }
};

int main()
{
    App{}.Run();
}