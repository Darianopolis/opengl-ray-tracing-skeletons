#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/gl.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/epsilon.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/compatibility.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/norm.hpp>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <vector>
#include <random>
#include <limits>
#include <variant>
#include <chrono>

constexpr float Inf = std::numeric_limits<float>::infinity();
constexpr float Eps = 0.000001f;

struct RNG {
    std::mt19937_64 rng;
    std::uniform_real_distribution<float> normalizedFloats{-1.f, 1.f};
    std::uniform_real_distribution<float> dist01{0.f, 1.f};
    std::vector<float> sampleKernel;
    uint32_t sampleIndex = 0;
    uint32_t sampleKernelSize = 0;
    bool regenerate = false;

    void UpdateRandomKernel()
    {
        sampleKernelSize = uint32_t(normalizedFloats(rng) * 250.f + 1000.f);
        sampleKernel.resize(sampleKernelSize);
        for (uint32_t i = 0; i < sampleKernelSize; i++) {
            sampleKernel[i] = normalizedFloats(rng);
        }
        sampleIndex = 0;
    }

    float Rand11()
    {
        if (sampleIndex > sampleKernelSize) {
            sampleIndex = 0;
            if (regenerate)
                UpdateRandomKernel();
        }
        float v = sampleKernel[sampleIndex++];
        return v;
    }

    float Rand01()
    {
        return Rand11() * 0.5f + 0.5f;
    }

    float Rand11Slow()
    {
        return normalizedFloats(rng);
    }

    int RandIntSlow(int min, int max)
    {
        std::uniform_int_distribution dist{min, max};
        return dist(rng);
    }

    glm::vec3 RandVec01()
    {
        return { Rand01(), Rand01(), Rand01() };
    }
};

struct Color {
    glm::vec3 value;
};

struct Ray {
    glm::vec3 origin, dir;
    float t = Inf;
};

struct Hit {
    glm::vec3 point;
    glm::vec3 normal;
};

struct Sphere {
    glm::vec3 center;
    float radius;

    inline bool Hit(Ray& ray, Hit& hit)
    {
        auto oc = ray.origin - center;
        auto halfB = glm::dot(oc, ray.dir);
        auto c = glm::length2(oc) - radius * radius;

        auto disc2 = halfB * halfB - c;
        if (disc2 < 0) return false;

        auto disc = glm::sqrt(disc2);

        auto root = -halfB - disc;
        if ((root < Eps) | (ray.t < root)) {
            root = -halfB + disc;
            if ((root < Eps) | (ray.t < root))
                return false;
        }

        ray.t = root;
        hit.point = ray.origin + ray.dir * root;
        hit.normal = glm::normalize(hit.point - center);

        return true;
    }
};

using Primitive = std::variant<Sphere>;

struct App {
    GLFWwindow *window;
    glm::ivec2 windowSize;

    glm::ivec2 textureSize;
    std::vector<glm::vec4> pixels;

    GLuint texture;
    GLuint framebuffer;

    RNG rng;

    //// Custom Variables ////

    float texSizeMultiplier = 0.1;

    float fovDegrees = 90.f;
    int sample = 0;
    uint64_t rays = 0;
    std::chrono::high_resolution_clock::time_point sampleStart;
    std::chrono::high_resolution_clock::time_point sampleEnd;

    std::vector<Color> colours;
    std::vector<Primitive> primitives;

    //// End of Custom Variables ////

    App()
    {
        // Setup GLFW
        glfwInit();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        window = glfwCreateWindow(800, 600, "Ray Tracing Skeleton 03 - Ray Gen", nullptr, nullptr);

        // Setup OpenGL
        glfwMakeContextCurrent(window);
        gladLoadGL(glfwGetProcAddress);
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

        // Scene
        primitives.push_back(Sphere{.center = { -0.25f, 0.f, 0.f }, .radius = 0.5f });
        primitives.push_back(Sphere{.center = { 0.25f, 0.f, 0.f }, .radius = 0.5f });

        colours.push_back(Color{{1.f, 0.f, 0.f}});
        colours.push_back(Color{{0.f, 1.f, 0.f}});
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

        ResetSamples();
    }

    glm::vec4 CastRay(glm::vec2 ndc)
    {
        float aspect = float(textureSize.x) / float(textureSize.y);
        float fov = glm::radians(fovDegrees);

        // Hardcoded camera axis for now
        glm::vec3 pos { 0.f, 0.f, 1.f };
        glm::vec3 Z { 0.f, 0.f, -1.f / glm::atan(fov * 0.5f) };
        glm::vec3 X { aspect, 0.f, 0.f };
        glm::vec3 Y { 0.f, 1.f, 0.f };

        // Compute normalized directional vector from camera axis
        auto dir = glm::normalize(X * ndc.x + Y * ndc.y + Z);

        // Initialize ray and hit
        Ray ray{pos, dir, Inf};
        Hit hit{};
        Color color {};

        // Search primitives to find a hit
        for (int i = 0; i < primitives.size(); ++i) {
            rays++;
            if (std::visit([&](auto&& prim) { return prim.Hit(ray, hit); }, primitives[i])) {
                color = colours[i];
            }
        }

        if (ray.t == Inf)
            return glm::vec4(0.f, 0.f, 0.f, 1.f);

        auto lightDir = glm::normalize(glm::vec3(-2.f, 1.f, 1.f));
        float light = glm::dot(hit.normal, lightDir);

        ray.origin = hit.point;
        ray = { hit.point, lightDir, Inf };
        hit = {};
        for (auto& primitive : primitives) {
            rays++;
            std::visit([&](auto&& prim) { return prim.Hit(ray, hit); }, primitive);
        }

        if (ray.t < Inf) // Occluded from light
            return glm::vec4(0.f, 0.f, 0.f, 1.f);

        return glm::vec4(glm::vec3(color.value) * glm::vec3(light), 1.f);
    }

    void Sample(float weight, float jitter = 0.f)
    {
        rng.UpdateRandomKernel();

        for (int y = 0; y < textureSize.y; ++y) {
            for (int x = 0; x < textureSize.x; ++x) {
                // Compute normalized pixel position in [-1, 1] with some jitter
                glm::vec2 ndc = {
                    (x + 0.5f + jitter * 0.5f * rng.Rand11()) * 2.f / textureSize.x - 1.f,
                    (y + 0.5f + jitter * 0.5f * rng.Rand11()) * 2.f / textureSize.y - 1.f
                };

                // Compute update pixel value based on weight
                Pixel(x, y) = Pixel(x, y) * (1.f - weight) + weight * CastRay(ndc);
            }
        }
    }

    void ResetSamples()
    {
        sample = 0;
        rays = 0;
    }

    std::string formatLargeNumber(uint64_t value)
    {
        std::stringstream ss;
        ss.imbue(std::locale("en_US"));
        ss << value;

        return ss.str();
    }

    void Run()
    {
        using namespace std::chrono;
        using namespace std::chrono_literals;

        int frames = 0;
        int fps = 0;
        auto last = steady_clock::now();
        while (!glfwWindowShouldClose(window)) {

            if (steady_clock::now() - last > 1s)
            {
                fps = frames;
                frames = 0;
                last = steady_clock::now();
            }

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            ImGui::Begin("Settings");

            // Show Statistics
            ImGui::Text("Sample: %i", sample);
            ImGui::Text("Rays/s: %s", formatLargeNumber(rays / duration_cast<duration<float>>(sampleEnd - sampleStart).count()).c_str());
            ImGui::Text("Total Rays: %s", formatLargeNumber(rays).c_str());
            ImGui::Text("Time: %.1fs", duration_cast<duration<float>>(sampleEnd - sampleStart).count());
            ImGui::Text("Texture Size: (%i, %i)", textureSize.x, textureSize.y);
            ImGui::Text("FPS: %i", fps);

            // Change scale of texture relative to window  size
            if (ImGui::SliderFloat("Texture scale", &texSizeMultiplier, 0.02f, 1.f)) {
                ResizeTexture(int(windowSize.x * texSizeMultiplier), int(windowSize.y * texSizeMultiplier));
            }

            if (ImGui::SliderFloat("FOV", &fovDegrees, 10.f, 170.f)) {
                ResetSamples();
            }

            ImGui::End();

            // Sample using exponential moving average and update times
            if (sample == 0)
                sampleStart = high_resolution_clock::now();
            if (sample < 100) {
                Sample(1.f / ++sample, 1.f);
                sampleEnd = high_resolution_clock::now();
            }

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

            frames++;
        }
    }
};

int main()
{
    App{}.Run();
}