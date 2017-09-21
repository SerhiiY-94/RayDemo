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

class GSCurveTest : public GameState {
    GameBase *game_;
    std::weak_ptr<GameStateManager> state_manager_;
    std::shared_ptr<ren::Context> ctx_;
    std::shared_ptr<Renderer> renderer_;

    ren::Camera cam_;

    math::aligned_vector<math::vec3> points_;

    bool finalize_curve_ = false;
    std::vector<std::array<math::vec3, 4>> curve_;

    math::vec3 color_table_[255];
    math::vec3 current_col_;

    bool grabbed_ = false;

    void ProcessCurveRec(const math::vec3 &p1, const math::vec3 &p2, const math::vec3 &p3, const math::vec3 &p4);

public:
    explicit GSCurveTest(GameBase *game);
    ~GSCurveTest();

    void Enter() override;
    void Exit() override;

    void Draw(float dt_s) override;

    void Update(int dt_ms) override;

    void HandleInput(InputManager::Event) override;
};