#include "Viewer.h"

#include <sstream>

#include <engine/GameStateManager.h>
#include <ray/RendererFactory.h>
#include <ren/Context.h>
#include <sys/AssetFile.h>
#include <sys/Log.h>
#include <sys/Json.h>

#include "renderer/Renderer.h"
#include "states/GSCreate.h"
#include "ui/FontStorage.h"

namespace ViewerConstants {
const int CURSOR_SIZE = 64;
}

Viewer::Viewer(int w, int h, const char *local_dir) : GameBase(w, h, local_dir) {
    using namespace ViewerConstants;
    using namespace math;

    auto ctx = GetComponent<ren::Context>(REN_CONTEXT_KEY);

    JsObject main_config;

    {
        // load config
        sys::AssetFile config_file("assets/config.json", sys::AssetFile::IN);
        size_t config_file_size = config_file.size();
        std::unique_ptr<char[]> buf(new char[config_file_size]);
        config_file.Read(buf.get(), config_file_size);

        std::stringstream ss;
        ss.write(buf.get(), config_file_size);

        if (!main_config.Read(ss)) {
            throw std::runtime_error("Unable to load main config!");
        }
    }

    const JsObject &gfx_settings = main_config.at("gfx_settings");

    {
        // create renderer for player etc.
        const JsObject &general_settings = gfx_settings.at("general");
        auto renderer = std::make_shared<Renderer>(*ctx, general_settings);
        AddComponent(RENDERER_KEY, renderer);
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
        // create ray renderer
        auto ray_renderer = ray::CreateRenderer(w, h, ray::PreferOCL);
        AddComponent(RAY_RENDERER_KEY, ray_renderer);
    }

    auto input_manager = GetComponent<InputManager>(INPUT_MANAGER_KEY);
    input_manager->SetConverter(InputManager::RAW_INPUT_P1_MOVE, nullptr);
    input_manager->SetConverter(InputManager::RAW_INPUT_P2_MOVE, nullptr);

    auto state_manager = GetComponent<GameStateManager>(STATE_MANAGER_KEY);
    state_manager->Push(GSCreate(GS_RAY_TEST, this));
}

void Viewer::Resize(int w, int h) {
    using namespace ViewerConstants;
    using namespace math;

    auto ray_renderer = GetComponent<ray::RendererBase>(RAY_RENDERER_KEY);
    if (ray_renderer) {
        ray_renderer->Resize(w, h);
    }

    GameBase::Resize(w, h);
}

