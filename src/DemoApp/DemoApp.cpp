#include "DemoApp.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <html5.h>
#endif

#if defined(USE_GL_RENDER)
#include <Ren/GL.h>
#elif defined(USE_SW_RENDER)

#endif

#if !defined(__ANDROID__)
#include <SDL2/SDL.h>
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_video.h>
#endif

#include <Eng/GameBase.h>
#include <Eng/TimedInput.h>
#include <Sys/Log.h>
#include <Sys/Time_.h>

#include <SDL2/SDL_events.h>

namespace {
DemoApp *g_app = nullptr;
}

extern "C" {
    // Enable High Performance Graphics while using Integrated Graphics
    DLL_EXPORT int32_t NvOptimusEnablement = 0x00000001;        // Nvidia
    DLL_EXPORT int AmdPowerXpressRequestHighPerformance = 1;    // AMD

#ifdef _WIN32
    DLL_IMPORT int __stdcall SetProcessDPIAware();
#endif
}

DemoApp::DemoApp() : quit_(false) {
    g_app = this;
}

DemoApp::~DemoApp() {

}

int DemoApp::Init(int w, int h, const char *scene_name, bool nogpu, bool coherent, bool copy_lib) {
#if !defined(__ANDROID__)
#ifdef _WIN32
    int dpi_result = SetProcessDPIAware();
    (void)dpi_result;
#endif

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        return -1;
    }

    window_ = SDL_CreateWindow("View", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h,
                               SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

#if defined(__EMSCRIPTEN__)
    emscripten_set_resize_callback(nullptr, nullptr, true,
    [](int event_type, const EmscriptenUiEvent *ui_event, void *user_data) -> int {
        if (event_type == EMSCRIPTEN_EVENT_RESIZE) {
            int new_w = ui_event->documentBodyClientWidth,
            new_h = (new_w / 16) * 9;
            SDL_SetWindowSize(g_app->window_, new_w, new_h);
        }
        return EMSCRIPTEN_RESULT_SUCCESS;
    });

    emscripten_set_fullscreenchange_callback(nullptr, nullptr, true,
    [](int event_type, const EmscriptenFullscreenChangeEvent *fullscreen_change_event, void *user_data) -> int {
        if (event_type == EMSCRIPTEN_EVENT_FULLSCREENCHANGE) {
            //int new_w = fullscreen_change_event->screenWidth,
            //    new_h = (new_w / 16) * 9;
            int new_h = fullscreen_change_event->screenHeight,
            new_w = (new_w / 9) * 16;
            SDL_SetWindowSize(g_app->window_, new_w, new_h);
        }

        return EMSCRIPTEN_RESULT_SUCCESS;
    });
#endif

#if defined(USE_GL_RENDER)
    gl_ctx_ = SDL_GL_CreateContext(window_);
#if !defined(__EMSCRIPTEN__)
    //SDL_GL_SetSwapInterval(1);
#endif
#elif defined(USE_SW_RENDER)
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer_) {
        const char *s = SDL_GetError();
        printf("%s\n", s);
    }
    texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
    if (!texture_) {
        const char *s = SDL_GetError();
        printf("%s\n", s);
    }
#endif
#endif
    try {
        LoadLib(w, h, scene_name, nogpu, coherent, copy_lib);
    } catch (std::exception &e) {
        fprintf(stderr, "%s", e.what());
        return -1;
    }

    return 0;
}

void DemoApp::Destroy() {
    viewer_.reset();

#if !defined(__ANDROID__)
#if defined(USE_GL_RENDER)
    SDL_GL_DeleteContext(gl_ctx_);
#elif defined(USE_SW_RENDER)
    SDL_DestroyTexture(texture_);
    SDL_DestroyRenderer(renderer_);
#endif
    SDL_DestroyWindow(window_);
    SDL_Quit();
#endif
}

void DemoApp::Frame() {
    viewer_->Frame();
}

#if !defined(__ANDROID__)
int DemoApp::Run(const std::vector<std::string> &args) {
    int w = 640, h = 360;
    scene_name_ = "assets/scenes/sponza_simple.json";
    nogpu_ = false;
    coherent_ = false;
    copy_lib_ = false;

    for (size_t i = 0; i < args.size(); i++) {
        if (args[i] == "-w" && (++i != args.size())) {
            w = atoi(args[i].c_str());
        } else if (args[i] == "-h" && (++i != args.size())) {
            h = atoi(args[i].c_str());
        } else if ((args[i] == "-scene" || args[i] == "-s") && (++i != args.size())) {
            scene_name_ = args[i];
        } else if (args[i] == "-nogpu") {
            nogpu_ = true;
        } else if (args[i] == "-coherent") {
            coherent_ = true;
        } else if (args[i] == "-copy_lib") {
            copy_lib_ = true;
        }
    }

    if (Init(w, h, scene_name_.c_str(), nogpu_, coherent_, copy_lib_) < 0) {
        return -1;
    }

#if defined(__EMSCRIPTEN__)
    emscripten_set_main_loop([]() {
        g_app->PollEvents();
        g_app->Frame();
    }, 0, 0);
#else
    while (!terminated()) {
        this->PollEvents();

        this->Frame();

#if defined(USE_GL_RENDER)
        SDL_GL_SwapWindow(window_);
#elif defined(USE_SW_RENDER)
        SDL_UpdateTexture(texture_, NULL, p_get_renderer_pixels_(viewer_.get()), viewer_->width * sizeof(Uint32));

        //SDL_RenderClear(renderer_);
        SDL_RenderCopy(renderer_, texture_, NULL, NULL);
        SDL_RenderPresent(renderer_);
#endif
    }

    this->Destroy();
#endif
    return 0;
}

bool DemoApp::ConvertToRawButton(int32_t key, InputManager::RawInputButton &button) {
    switch (key) {
    case SDLK_UP:
        button = InputManager::RAW_INPUT_BUTTON_UP;
        break;
    case SDLK_DOWN:
        button = InputManager::RAW_INPUT_BUTTON_DOWN;
        break;
    case SDLK_LEFT:
        button = InputManager::RAW_INPUT_BUTTON_LEFT;
        break;
    case SDLK_RIGHT:
        button = InputManager::RAW_INPUT_BUTTON_RIGHT;
        break;
    case SDLK_ESCAPE:
        button = InputManager::RAW_INPUT_BUTTON_EXIT;
        break;
    case SDLK_TAB:
        button = InputManager::RAW_INPUT_BUTTON_TAB;
        break;
    case SDLK_BACKSPACE:
        button = InputManager::RAW_INPUT_BUTTON_BACKSPACE;
        break;
    case SDLK_LSHIFT:
    case SDLK_RSHIFT:
        button = InputManager::RAW_INPUT_BUTTON_SHIFT;
        break;
    case SDLK_DELETE:
        button = InputManager::RAW_INPUT_BUTTON_DELETE;
        break;
    case SDLK_SPACE:
        button = InputManager::RAW_INPUT_BUTTON_SPACE;
        break;
    default:
        button = InputManager::RAW_INPUT_BUTTON_OTHER;
        break;
    }
    return true;
}

void DemoApp::PollEvents() {
    auto input_manager = viewer_->GetComponent<InputManager>(INPUT_MANAGER_KEY);
    if (!input_manager) return;

    SDL_Event e = {};
    InputManager::RawInputButton button;
    InputManager::Event evt = {};
    while (SDL_PollEvent(&e)) {
        evt.type = InputManager::RAW_INPUT_NONE;
        switch (e.type) {
        case SDL_KEYDOWN: {
            if (e.key.keysym.sym == SDLK_ESCAPE) {
                quit_ = true;
                return;
            } /*else if (e.key.keysym.sym == SDLK_TAB) {
            bool is_fullscreen = bool(SDL_GetWindowFlags(window_) & SDL_WINDOW_FULLSCREEN);
            SDL_SetWindowFullscreen(window_, is_fullscreen ? 0 : SDL_WINDOW_FULLSCREEN);
            return;
        }*/ else if (ConvertToRawButton(e.key.keysym.sym, button)) {
                evt.type = InputManager::RAW_INPUT_KEY_DOWN;
                evt.key = button;
                evt.raw_key = e.key.keysym.sym;
            }
        }
        break;
        case SDL_KEYUP:
            if (e.key.keysym.sym == SDLK_F5) {
                input_manager = nullptr;
                LoadLib(0, 0, scene_name_.c_str(), nogpu_, coherent_, true);
                return;
            } else if (ConvertToRawButton(e.key.keysym.sym, button)) {
                evt.type = InputManager::RAW_INPUT_KEY_UP;
                evt.key = button;
                evt.raw_key = e.key.keysym.sym;
            }
            break;
        case SDL_FINGERDOWN:
            evt.type = e.tfinger.fingerId == 0 ? InputManager::RAW_INPUT_P1_DOWN : InputManager::RAW_INPUT_P2_DOWN;
            evt.point.x = e.tfinger.x * viewer_->width;
            evt.point.y = e.tfinger.y * viewer_->height;
            break;
        case SDL_MOUSEBUTTONDOWN:
            evt.type = InputManager::RAW_INPUT_P1_DOWN;
            evt.point.x = (float) e.motion.x;
            evt.point.y = (float) e.motion.y;
            break;
        case SDL_FINGERUP:
            evt.type = e.tfinger.fingerId == 0 ? InputManager::RAW_INPUT_P1_UP : InputManager::RAW_INPUT_P2_UP;
            evt.point.x = e.tfinger.x * viewer_->width;
            evt.point.y = e.tfinger.y * viewer_->height;
            break;
        case SDL_MOUSEBUTTONUP:
            evt.type = InputManager::RAW_INPUT_P1_UP;
            evt.point.x = (float) e.motion.x;
            evt.point.y = (float) e.motion.y;
            break;
        case SDL_QUIT: {
            quit_ = true;
            return;
        }
        case SDL_FINGERMOTION:
            evt.type = e.tfinger.fingerId == 0 ? InputManager::RAW_INPUT_P1_MOVE : InputManager::RAW_INPUT_P2_MOVE;
            evt.point.x = e.tfinger.x * viewer_->width;
            evt.point.y = e.tfinger.y * viewer_->height;
            evt.move.dx = e.tfinger.dx * viewer_->width;
            evt.move.dy = e.tfinger.dy * viewer_->height;
            break;
        case SDL_MOUSEMOTION:
            evt.type = InputManager::RAW_INPUT_P1_MOVE;
            evt.point.x = (float) e.motion.x;
            evt.point.y = (float) e.motion.y;
            evt.move.dx = (float) e.motion.xrel;
            evt.move.dy = (float) e.motion.yrel;
            break;
        case SDL_MOUSEWHEEL:
            evt.type = InputManager::RAW_INPUT_MOUSE_WHEEL;
            evt.move.dx = (float)e.wheel.x;
            evt.move.dy = (float)e.wheel.y;
            break;
        case SDL_WINDOWEVENT:
            if (e.window.event == SDL_WINDOWEVENT_RESIZED || e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                evt.type = InputManager::RAW_INPUT_RESIZE;
                evt.point.x = (float)e.window.data1;
                evt.point.y = (float)e.window.data2;

                // TODO: ???
#if defined(__EMSCRIPTEN__)
                emscripten_set_canvas_size(e.window.data1, e.window.data2);
#endif
                viewer_->Resize(e.window.data1, e.window.data2);
#if defined(USE_SW_RENDER)
                SDL_RenderPresent(renderer_);

                SDL_DestroyTexture(texture_);
                texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                             e.window.data1, e.window.data2);
#endif
            }
            break;
        default:
            return;
        }
        if (evt.type != InputManager::RAW_INPUT_NONE) {
            evt.time_stamp = Sys::GetTimeMs() - (SDL_GetTicks() - e.common.timestamp);
            input_manager->AddRawInputEvent(evt);
        }
    }
}

#endif

void DemoApp::LoadLib(int w, int h, const char *scene_name, bool nogpu, bool coherent, bool copy_lib) {
    if (viewer_) {
        w = viewer_->width;
        h = viewer_->height;
    }

    viewer_.reset();

    demo_lib_ = {};
#if defined(WIN32)
    GameBase * (__cdecl *p_create_viewer)(int w, int h, const char *local_dir, const char *scene_name, int nogpu, int coherent) = nullptr;

    if (copy_lib) {
        system("copy \"DemoLib.dll\" \"DemoLib_.dll\"");
        demo_lib_ = Sys::DynLib{ "DemoLib_.dll" };
    } else {
        demo_lib_ = Sys::DynLib{ "DemoLib.dll" };
    }
#elif defined(__linux__)
    GameBase * (*p_create_viewer)(int w, int h, const char *local_dir, const char *scene_name, int nogpu, int coherent) = nullptr;

    if (copy_lib) {
        if (system(R"(cp "DemoLib.so" "DemoLib_.so")") == -1) LOGE("system call failed");
        demo_lib_ = Sys::DynLib{ "./DemoLib_.so" };
    } else {
        demo_lib_ = Sys::DynLib{ "./DemoLib.so" };
    }
#else
    GameBase * (*p_create_viewer)(int w, int h, const char *local_dir, const char *scene_name, int nogpu, int coherent) = nullptr;

    if (copy_lib) {
        if (system(R"(cp "DemoLib.dylib" "DemoLib_.dylib")") == -1) LOGE("system call failed");
        demo_lib_ = Sys::DynLib{ "./DemoLib_.dylib" };
    } else {
        demo_lib_ = Sys::DynLib{ "./DemoLib.dylib" };
    }

#endif

    if (demo_lib_) {
        p_create_viewer = (decltype(p_create_viewer))demo_lib_.GetProcAddress("CreateViewer");
        p_get_renderer_pixels_ = (decltype(p_get_renderer_pixels_))demo_lib_.GetProcAddress("GetRendererPixels");
    }

	if (p_create_viewer) {
		viewer_.reset(p_create_viewer(w, h, "./", scene_name, nogpu ? 1 : 0, coherent ? 1 : 0));
	}
}
