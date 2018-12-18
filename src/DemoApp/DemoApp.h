#pragma once

#include <memory>
#include <string>
#include <vector>

#include <Eng/TimedInput.h>
#include <Sys/DynLib.h>

struct SDL_Renderer;
struct SDL_Texture;
struct SDL_Window;

class GameBase;

class DemoApp {
#if defined(USE_GL_RENDER)
    void            *gl_ctx_ = nullptr;
#elif defined(USE_SW_RENDER)
    SDL_Renderer    *renderer_ = nullptr;
    SDL_Texture     *texture_ = nullptr;
#endif
    SDL_Window		*window_ = nullptr;

    std::string     scene_name_;
    bool            nogpu_, copy_lib_;

    Sys::DynLib		demo_lib_;
    const void *(*p_get_renderer_pixels_)(GameBase *);

    bool quit_;

#if !defined(__ANDROID__)
    bool ConvertToRawButton(int32_t key, InputManager::RawInputButton &button);
    void PollEvents();
#endif

    std::unique_ptr<GameBase> viewer_;

    void LoadLib(int w, int h, const char *scene_name, bool nogpu, bool copy_lib);
public:
    DemoApp();
    ~DemoApp();

    int Init(int w, int h, const char *scene_name, bool nogpu, bool copy_lib);
    void Destroy();

    void Frame();

#if !defined(__ANDROID__)
    int Run(const std::vector<std::string> &args);
#endif

    bool terminated() const {
        return quit_;
    }
};
