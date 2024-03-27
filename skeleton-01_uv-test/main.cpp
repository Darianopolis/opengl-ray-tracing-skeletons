#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/gl.h>
#include <glm/glm.hpp>

#include <vector>

struct App {
    GLFWwindow *window;
    glm::ivec2 windowSize;

    glm::ivec2 textureSize;
    std::vector<glm::vec4> pixels;

    GLuint texture;
    GLuint framebuffer;

    App()
    {
        // Setup GLFW
        glfwInit();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        window = glfwCreateWindow(800, 600, "OpenGL UV Test", nullptr, nullptr);

        // Setup OpenGL
        glfwMakeContextCurrent(window);
        gladLoadGL(glfwGetProcAddress);
        glfwSwapInterval(1);

        // Register for resize callback
        glfwSetWindowUserPointer(window, this);
        glfwSetWindowSizeCallback(window, [](auto wnd, int w, int h) {
            ((App*)glfwGetWindowUserPointer(wnd))->OnResize(w, h);
        });

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
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    /*
     * Gets the pixel at (x, y) to be read/modified
     */
    glm::vec4& Pixel(int x, int y)
    {
        return pixels[y * textureSize.x + x];
    }

    /*
     * Write out pixels to the OpenGL texture to be displayed
     */
    void WritePixelsToTexture()
    {
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, textureSize.x, textureSize.y, 0, GL_RGBA, GL_FLOAT, pixels.data());
    }

    void OnResize(int w, int h)
    {
        windowSize = { w, h };

        // Change this if you want a different sized texture to your screen!
        ResizeTexture(w, h);
    }

    void ResizeTexture(int w, int h)
    {
        // Resize CPU-side pixel storage
        // (OpenGL Resources are automatically resized on WritePixelsToTexture)
        textureSize = { w, h };
        pixels.resize(textureSize.x * textureSize.y);

        // Demo UV pattern
        for (int y = 0; y < textureSize.y; ++y) {
            for (int x = 0; x < textureSize.x; ++x) {
                Pixel(x, y) = { x / (textureSize.x - 1.f), y / (textureSize.y - 1.f), 0.f, 1.f };
            }
        }
    }

    void Run()
    {
        while (!glfwWindowShouldClose(window)) {

            // Just blit the texture directly out to the screen for now!
            WritePixelsToTexture();
            glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            glBlitFramebuffer(
                0, 0, textureSize.x, textureSize.y,
                0, 0, windowSize.x, windowSize.y,
                GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);

            // Present and check events
            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    }
};

int main()
{
    App{}.Run();
}