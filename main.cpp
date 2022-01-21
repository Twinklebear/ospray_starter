#include <algorithm>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <limits>
#include <thread>
#include <SDL.h>
#include <ospray/ospray.h>
#include <ospray/ospray_cpp.h>
#include <ospray/ospray_cpp/ext/glm.h>
#include "arcball_camera.h"
#include "glad/glad.h"
#include "imgui/imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl.h"
#include "stb_image.h"
#include "stb_image_write.h"
#include "util/arcball_camera.h"
#include "util/json.hpp"
#include "util/shader.h"
#include "util/transfer_function_widget.h"
#include "util/util.h"

using namespace ospray;
using json = nlohmann::json;

const std::string fullscreen_quad_vs = R"(
#version 330 core

const vec4 pos[4] = vec4[4](
	vec4(-1, 1, 0.5, 1),
	vec4(-1, -1, 0.5, 1),
	vec4(1, 1, 0.5, 1),
	vec4(1, -1, 0.5, 1)
);

void main(void){
	gl_Position = pos[gl_VertexID];
}
)";

const std::string display_texture_fs = R"(
#version 330 core

uniform sampler2D img;

out vec4 color;

void main(void){ 
	ivec2 uv = ivec2(gl_FragCoord.xy);
	color = texelFetch(img, uv, 0);
})";

int win_width = 1280;
int win_height = 720;

glm::vec2 transform_mouse(glm::vec2 in)
{
    return glm::vec2(in.x * 2.f / win_width - 1.f, 1.f - 2.f * in.y / win_height);
}

void run_app(const std::vector<std::string> &args, SDL_Window *window);

int main(int argc, const char **argv)
{
    OSPError init_err = ospInit(&argc, argv);
    if (init_err != OSP_NO_ERROR) {
        throw std::runtime_error("Failed to initialize OSPRay");
    }

    OSPDevice device = ospGetCurrentDevice();
    if (!device) {
        throw std::runtime_error("OSPRay device could not be fetched!");
    }
    ospDeviceSetErrorCallback(
        device,
        [](void *, OSPError, const char *errorDetails) {
            std::cerr << "OSPRay error: " << errorDetails << std::endl;
            throw std::runtime_error(errorDetails);
        },
        nullptr);
    ospDeviceSetStatusCallback(
        device, [](void *, const char *msg) { std::cout << msg; }, nullptr);

    bool warnAsErrors = true;
    auto logLevel = OSP_LOG_WARNING;

    ospDeviceSetParam(device, "warnAsError", OSP_BOOL, &warnAsErrors);
    ospDeviceSetParam(device, "logLevel", OSP_INT, &logLevel);

    ospDeviceCommit(device);
    ospDeviceRelease(device);

    // Load our module
    ospLoadModule("example");

    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        std::cerr << "Failed to init SDL: " << SDL_GetError() << "\n";
        return -1;
    }

    const char *glsl_version = "#version 330 core";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    SDL_Window *window = SDL_CreateWindow("OSPRay Starter",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          win_width,
                                          win_height,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_SetSwapInterval(1);
    SDL_GL_MakeCurrent(window, gl_context);

    if (!gladLoadGL()) {
        std::cerr << "Failed to initialize OpenGL\n";
        return 1;
    }

    // Setup Dear ImGui context
    ImGui::CreateContext();

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer bindings
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    run_app(std::vector<std::string>(argv, argv + argc), window);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    ospShutdown();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

void run_app(const std::vector<std::string> &args, SDL_Window *window)
{
    bool cmdline_camera = false;
    glm::vec3 cam_eye;
    glm::vec3 cam_at;
    glm::vec3 cam_up;

    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "-camera") {
            cmdline_camera = true;
            cam_eye.x = std::stof(args[++i]);
            cam_eye.y = std::stof(args[++i]);
            cam_eye.z = std::stof(args[++i]);

            cam_at.x = std::stof(args[++i]);
            cam_at.y = std::stof(args[++i]);
            cam_at.z = std::stof(args[++i]);

            cam_up.x = std::stof(args[++i]);
            cam_up.y = std::stof(args[++i]);
            cam_up.z = std::stof(args[++i]);
        }
    }

    const glm::vec3 world_center(0.f);
    if (!cmdline_camera) {
        cam_eye = world_center - glm::vec3(0.f, 0.f, 5.f);
        cam_at = world_center;
        cam_up = glm::vec3(0.f, 1.f, 0.f);
    }
    ArcballCamera arcball(cam_eye, cam_at, cam_up);

    cpp::Renderer renderer("scivis");
    renderer.setParam("backgroundColor", glm::vec4(0.f, 0.f, 0.f, 1.f));
    renderer.commit();

    // Sphere positions
    std::vector<glm::vec3> positions = {glm::vec3(-1.0f, -1.0f, 0.0f),
                                        glm::vec3(0.0f, 1.0f, 0.0f),
                                        glm::vec3(1.0f, -1.0f, 0.0f)};

    // create and setup our geometry
    cpp::Geometry mesh("example_spheres");
    mesh.setParam("sphere.position", cpp::CopiedData(positions));
    mesh.setParam("radius", 0.5f);
    mesh.commit();

    // put the mesh into a model
    cpp::GeometricModel model(mesh);
    model.commit();

    cpp::Group group;
    group.setParam("geometry", cpp::CopiedData(model));
    group.commit();

    cpp::Instance instance(group);
    instance.commit();

    cpp::Light light("ambient");
    light.commit();

    cpp::World world;
    world.setParam("instance", cpp::CopiedData(instance));
    world.setParam("light", cpp::CopiedData(light));
    world.commit();

    cam_eye = arcball.eye();
    glm::vec3 cam_dir = arcball.dir();
    cam_up = arcball.up();

    cpp::Camera camera("perspective");
    camera.setParam("aspect", static_cast<float>(win_width) / win_height);
    camera.setParam("position", cam_eye);
    camera.setParam("direction", cam_dir);
    camera.setParam("up", cam_up);
    camera.setParam("fovy", 40.f);
    camera.commit();

    cpp::FrameBuffer fb(win_width, win_height, OSP_FB_SRGBA, OSP_FB_COLOR | OSP_FB_ACCUM);
    fb.clear();

    Shader display_render(fullscreen_quad_vs, display_texture_fs);
    display_render.uniform("img", 0);

    GLuint render_texture;
    glGenTextures(1, &render_texture);
    glBindTexture(GL_TEXTURE_2D, render_texture);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA8,
                 win_width,
                 win_height,
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 nullptr);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glClearColor(0.0, 0.0, 0.0, 0.0);
    glDisable(GL_DEPTH_TEST);

    // Start rendering asynchronously
    cpp::Future future = fb.renderFrame(renderer, camera, world);
    std::vector<OSPObject> pending_commits;

    ImGuiIO &io = ImGui::GetIO();
    glm::vec2 prev_mouse(-2.f);
    bool done = false;
    bool camera_changed = true;
    bool window_changed = false;
    bool take_screenshot = false;
    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                done = true;
            }
            if (!io.WantCaptureKeyboard && event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    done = true;
                } else if (event.key.keysym.sym == SDLK_p) {
                    auto eye = arcball.eye();
                    auto dir = arcball.dir();
                    auto up = arcball.up();
                    std::cout << "-camera " << eye.x << " " << eye.y << " " << eye.z << " "
                              << eye.x + dir.x << " " << eye.y + dir.y << " " << eye.z + dir.z
                              << " " << up.x << " " << up.y << " " << up.z << "\n";
                } else if (event.key.keysym.sym == SDLK_c) {
                    take_screenshot = true;
                }
            }
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE &&
                event.window.windowID == SDL_GetWindowID(window)) {
                done = true;
            }
            if (!io.WantCaptureMouse) {
                if (event.type == SDL_MOUSEMOTION) {
                    const glm::vec2 cur_mouse =
                        transform_mouse(glm::vec2(event.motion.x, event.motion.y));
                    if (prev_mouse != glm::vec2(-2.f)) {
                        if (event.motion.state & SDL_BUTTON_LMASK) {
                            arcball.rotate(prev_mouse, cur_mouse);
                            camera_changed = true;
                        } else if (event.motion.state & SDL_BUTTON_RMASK) {
                            arcball.pan(cur_mouse - prev_mouse);
                            camera_changed = true;
                        }
                    }
                    prev_mouse = cur_mouse;
                } else if (event.type == SDL_MOUSEWHEEL) {
                    arcball.zoom(event.wheel.y / 100.f);
                    camera_changed = true;
                }
            }
            if (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_RESIZED) {
                window_changed = true;
                win_width = event.window.data1;
                win_height = event.window.data2;
                io.DisplaySize.x = win_width;
                io.DisplaySize.y = win_height;

                camera.setParam("aspect", static_cast<float>(win_width) / win_height);
                pending_commits.push_back(camera.handle());

                // make new framebuffer
                fb = cpp::FrameBuffer(
                    win_width, win_height, OSP_FB_SRGBA, OSP_FB_COLOR | OSP_FB_ACCUM);
                fb.clear();

                glDeleteTextures(1, &render_texture);
                glGenTextures(1, &render_texture);
                // Setup the render textures for color and normals
                glBindTexture(GL_TEXTURE_2D, render_texture);
                glTexImage2D(GL_TEXTURE_2D,
                             0,
                             GL_RGBA8,
                             win_width,
                             win_height,
                             0,
                             GL_RGBA,
                             GL_UNSIGNED_BYTE,
                             nullptr);

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            }
        }

        if (camera_changed) {
            cam_eye = arcball.eye();
            cam_dir = arcball.dir();
            cam_up = arcball.up();

            camera.setParam("position", cam_eye);
            camera.setParam("direction", cam_dir);
            camera.setParam("up", cam_up);
            pending_commits.push_back(camera.handle());
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // Rendering
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);

        if (future.isReady()) {
            if (!window_changed) {
                uint32_t *img = (uint32_t *)fb.map(OSP_FB_COLOR);
                glTexSubImage2D(GL_TEXTURE_2D,
                                0,
                                0,
                                0,
                                win_width,
                                win_height,
                                GL_RGBA,
                                GL_UNSIGNED_BYTE,
                                img);
                if (take_screenshot) {
                    take_screenshot = false;
                    stbi_flip_vertically_on_write(1);
                    stbi_write_png(
                        "screenshot.png", win_width, win_height, 4, img, win_width * 4);
                    std::cout << "Screenshot saved to 'screenshot.png'" << std::endl;
                    stbi_flip_vertically_on_write(0);
                }
                fb.unmap(img);
            }
            window_changed = false;

            if (!pending_commits.empty()) {
                fb.clear();
            }
            for (auto &c : pending_commits) {
                ospCommit(c);
            }
            pending_commits.clear();

            future = fb.renderFrame(renderer, camera, world);
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(display_render.program);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SDL_GL_SwapWindow(window);

        camera_changed = false;
    }
}
