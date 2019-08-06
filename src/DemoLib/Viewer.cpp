#include "Viewer.h"

#include <sstream>

#include <Eng/GameStateManager.h>
#include <Ray/RendererFactory.h>
#include <Ren/Context.h>
#include <Sys/AssetFile.h>
#include <Sys/Json.h>

#include "states/GSCreate.h"
#include "ui/FontStorage.h"

Viewer::Viewer(int w, int h, const char *local_dir, const char *_scene_name, int nogpu, int coherent) : GameBase(w, h, local_dir) {
    auto ctx = GetComponent<Ren::Context>(REN_CONTEXT_KEY);

    JsObject main_config;

    {
        // load config
        Sys::AssetFile config_file("assets/config.json", Sys::AssetFile::FileIn);
        size_t config_file_size = config_file.size();
        std::unique_ptr<char[]> buf(new char[config_file_size]);
        config_file.Read(buf.get(), config_file_size);

        std::stringstream ss;
        ss.write(buf.get(), config_file_size);

        if (!main_config.Read(ss)) {
            throw std::runtime_error("Unable to load main config!");
        }
    }

    const JsObject &ui_settings = main_config.at("ui_settings");

    {
        // load fonts
        auto font_storage = std::make_shared<FontStorage>();
        AddComponent(UI_FONTS_KEY, font_storage);

        const JsObject &fonts = ui_settings.at("fonts");
        for (auto &el : fonts.elements) {
            const std::string &name = el.first;
            const JsString &file_name = el.second;

            font_storage->LoadFont(name, file_name.val, ctx.get());
        }
    }

    {
        auto test_result = std::make_shared<double>(0.0);
        AddComponent(TEST_RESULT_KEY, test_result);
    }

    {
        auto scene_name = std::make_shared<std::string>(_scene_name);
        AddComponent(SCENE_NAME_KEY, scene_name);
    }

    {   // create ray renderer
        Ray::settings_t s;
        s.w = w;
        s.h = h;

        std::shared_ptr<Ray::RendererBase> ray_renderer;

        if (nogpu) {
            ray_renderer = Ray::CreateRenderer(s, Ray::RendererRef | Ray::RendererSSE2 | Ray::RendererAVX | Ray::RendererAVX2);
        } else {
            ray_renderer = Ray::CreateRenderer(s);
        }

        AddComponent(RAY_RENDERER_KEY, ray_renderer);
    }

    use_coherent_sampling = coherent != 0;

    auto input_manager = GetComponent<InputManager>(INPUT_MANAGER_KEY);
    input_manager->SetConverter(InputManager::RAW_INPUT_P1_MOVE, nullptr);
    input_manager->SetConverter(InputManager::RAW_INPUT_P2_MOVE, nullptr);

    auto state_manager = GetComponent<GameStateManager>(STATE_MANAGER_KEY);
    state_manager->Push(GSCreate(GS_RAY_TEST, this));
}

