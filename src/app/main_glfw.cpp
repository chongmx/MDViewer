// Entry point for Windows, Linux, macOS (GLFW + OpenGL 3) and the web (Emscripten GLFW + WebGL 2).
#include "application.h"

#include "mdv/os/os.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#if defined(__EMSCRIPTEN__)
#   include <GLES3/gl3.h>
#   include <emscripten.h>
#else
#   include <glad/glad.h>
#endif
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <memory>
#include <string>

namespace {

struct AppState {
    GLFWwindow* window = nullptr;
    std::unique_ptr<mdv::app::Application> app;
};

AppState* g_state = nullptr;

#if !defined(__EMSCRIPTEN__)
void onDrop(GLFWwindow*, int count, const char** paths)
{
    if (g_state && count > 0)
        g_state->app->openPath(paths[0]);
}
#endif

void mainLoopStep(void* arg)
{
    auto* state = static_cast<AppState*>(arg);
    glfwPollEvents();
    if (glfwGetWindowAttrib(state->window, GLFW_ICONIFIED) != 0) {
        ImGui_ImplGlfw_Sleep(10);
        return;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    state->app->frame();
    ImGui::Render();

    int width = 0, height = 0;
    glfwGetFramebufferSize(state->window, &width, &height);
    glViewport(0, 0, width, height);
    const ImVec4 clear = state->app->clearColor();
    glClearColor(clear.x, clear.y, clear.z, clear.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(state->window);

    if (state->app->quitRequested())
        glfwSetWindowShouldClose(state->window, GLFW_TRUE);
}

} // namespace

int main(int argc, char** argv)
{
    mdv::os::InitParams params;
    params.argc = argc;
    params.argv = argv;
    if (!mdv::os::init(params))
        return 1;

    if (!glfwInit())
        return 1;

#if defined(__EMSCRIPTEN__)
    const char* glslVersion = "#version 300 es";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    const char* glslVersion = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#else
    const char* glslVersion = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    const float scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
    GLFWwindow* window = glfwCreateWindow(int(1280 * scale), int(860 * scale), "MDViewer", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

#if !defined(__EMSCRIPTEN__)
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
        return 1;
#endif

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
#if defined(__EMSCRIPTEN__)
    ImGui_ImplGlfw_InstallEmscriptenCallbacks(window, "#canvas");
#endif
    ImGui_ImplOpenGL3_Init(glslVersion);

    // Heap-allocated so it survives main() returning under emscripten_set_main_loop.
    auto* state = new AppState();
    g_state = state;
    state->window = window;
    state->app = std::make_unique<mdv::app::Application>();
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    state->app->attachUi(scale, std::string(renderer ? renderer : "?") + " / " + (version ? version : "?"));
    state->app->openInitialDocument();
#if !defined(__EMSCRIPTEN__) // the browser GLFW ports do not all implement file drop
    glfwSetDropCallback(window, onDrop);
#endif

#if defined(__EMSCRIPTEN__)
    emscripten_set_main_loop_arg(mainLoopStep, state, 0, true);
#else
    while (!glfwWindowShouldClose(window))
        mainLoopStep(state);

    state->app.reset();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    g_state = nullptr;
    delete state;
    mdv::os::shutdown();
#endif
    return 0;
}
