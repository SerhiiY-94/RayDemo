#pragma once

#include <engine/GameState.h>
#include <engine/go/Go.h>
#include <ren/Camera.h>
#include <ren/Program.h>
#include <ren/Texture.h>

#include <math/math.hpp>

class GameBase;
class GameStateManager;
class GCursor;
class FontStorage;
class Renderer;

namespace ui {
class BaseElement;
class Renderer;
}

class GSPackTest : public GameState {
    GameBase *game_;
    std::weak_ptr<GameStateManager> state_manager_;
    std::shared_ptr<ren::Context> ctx_;
    std::shared_ptr<Renderer> renderer_;

    ren::Camera cam_;

    bool should_add_ = false,
         should_delete_ = false;

    int time_acc_ms_ = 0;
    int num_textures_ = 0;
    int num_free_pixels_;

    math::vec3 color_table_[255];
    math::vec3 current_col_;

    bool grabbed_ = false;

public:
    explicit GSPackTest(GameBase *game);
    ~GSPackTest();

    void Enter() override;
    void Exit() override;

    void Draw(float dt_s) override;

    void Update(int dt_ms) override;

    void HandleInput(InputManager::Event) override;
};