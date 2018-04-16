#pragma once

#include <engine/GameState.h>
#include <engine/go/Go.h>
#include <ren/Camera.h>
#include <ren/Program.h>
#include <ren/Texture.h>

#include <ray/RendererBase.h>

class GameBase;
class GameStateManager;
class FontStorage;

namespace sys {
class ThreadPool;
}

namespace ui {
class BaseElement;
class BitmapFont;
class Renderer;
}

class GSRayBucketTest : public GameState {
    GameBase *game_;
    std::weak_ptr<GameStateManager> state_manager_;
    std::shared_ptr<ren::Context> ctx_;

    std::shared_ptr<ui::Renderer> ui_renderer_;
    std::shared_ptr<ui::BaseElement> ui_root_;
    std::shared_ptr<ui::BitmapFont> font_;

    std::shared_ptr<ray::RendererBase> ray_renderer_;
    std::shared_ptr<ray::SceneBase> ray_scene_;

    std::shared_ptr<sys::ThreadPool> threads_;

    bool animate_ = false;
    bool view_grabbed_ = false;
    bool view_targeted_ = false;
    math::vec3 view_origin_ = { 0, 20, 3 },
               view_dir_ = { -1, 0, 0 },
               view_target_ = { 0, 0, 0 };

    math::vec3 sun_dir_ = { 0, 1, 0 };

    bool invalidate_preview_ = true;

    float forward_speed_ = 0, side_speed_ = 0;

    float cur_time_stat_ms_ = 0;

    unsigned int time_acc_ = 0;
    int time_counter_ = 0;

    int num_buckets_;

    std::vector<ray::RegionContext> region_contexts_;

    int last_reg_context_ = 0;
    int cur_spp_ = 0;

    std::vector<float> color_table_;

    void UpdateRegionContexts();
    void UpdateEnvironment(const math::vec3 &sun_dir);
public:
    explicit GSRayBucketTest(GameBase *game);

    void Enter() override;
    void Exit() override;

    void Draw(float dt_s) override;

    void Update(int dt_ms) override;

    void HandleInput(InputManager::Event) override;
};