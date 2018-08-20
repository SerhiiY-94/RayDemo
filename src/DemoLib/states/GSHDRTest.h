#pragma once

#include <Eng/GameState.h>
#include <Eng/go/Go.h>
#include <Ren/Camera.h>
#include <Ren/Program.h>
#include <Ren/Texture.h>

class GameBase;
class GameStateManager;
class GCursor;
class FontStorage;
class Random;
class Renderer;

namespace Gui {
class BaseElement;
class BitmapFont;
class Renderer;
}

class GSHDRTest : public GameState {
    GameBase *game_;
    std::weak_ptr<GameStateManager> state_manager_;
    std::shared_ptr<Ren::Context> ctx_;
    std::shared_ptr<Renderer> renderer_;

    std::shared_ptr<Random> random_;

    std::shared_ptr<Gui::Renderer> ui_renderer_;
    std::shared_ptr<Gui::BaseElement> ui_root_;
    std::shared_ptr<Gui::BitmapFont> font_;

    std::vector<uint8_t> img_;
    int img_w_, img_h_;
    float mul_ = 1.0f;
    std::vector<float> pixels_;
    float iteration_ = 0;

public:
    explicit GSHDRTest(GameBase *game);

    void Enter() override;
    void Exit() override;

    void Draw(float dt_s) override;

    void Update(int dt_ms) override;

    void HandleInput(InputManager::Event) override;
};