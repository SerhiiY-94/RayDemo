#pragma once

#include <engine/GameState.h>
#include <engine/go/Go.h>
#include <ren/Camera.h>
#include <ren/Program.h>
#include <ren/Texture.h>

class GameBase;
class GameStateManager;
class GCursor;
class FontStorage;
class Renderer;

namespace ui {
class BaseElement;
class Renderer;
}

class GSClipTest : public GameState {
    GameBase *game_;
    std::weak_ptr<GameStateManager> state_manager_;
    std::shared_ptr<ren::Context> ctx_;
    std::shared_ptr<Renderer> renderer_;

    ren::Camera cam_;

    bool grabbed_ = false;
    math::vec2 shift_;
    math::vec3 p[3] = { { 100, -10, 0 }, { 0, 100, 0 }, { 0, -100, 0 } };
    int point_grabbed_ = -1;
public:
    explicit GSClipTest(GameBase *game);
    ~GSClipTest();

    void Enter() override;
    void Exit() override;

    void Draw(float dt_s) override;

    void Update(int dt_ms) override;

    void HandleInput(InputManager::Event) override;
};