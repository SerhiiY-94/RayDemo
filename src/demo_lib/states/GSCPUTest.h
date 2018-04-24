#pragma once

#include <atomic>

#include <engine/GameState.h>
#include <engine/go/Go.h>
#include <ren/Camera.h>
#include <ren/Program.h>
#include <ren/Texture.h>

class GameBase;
class GameStateManager;
class GCursor;
class FontStorage;
class Random;
class Renderer;

namespace sys {
class ThreadPool;
}

namespace ui {
class BaseElement;
class BitmapFont;
class Renderer;
}

class GSCPUTest : public GameState {
    GameBase *game_;
    std::weak_ptr<GameStateManager> state_manager_;
    std::shared_ptr<ren::Context> ctx_;
    std::shared_ptr<Renderer> renderer_;

    std::shared_ptr<Random> random_;

    std::shared_ptr<ui::Renderer> ui_renderer_;
    std::shared_ptr<ui::BaseElement> ui_root_;
    std::shared_ptr<ui::BitmapFont> font_;

    std::shared_ptr<sys::ThreadPool> threads_;

    enum { eWarmup, eStarted, eFinished } state_ = eWarmup;

    std::atomic_bool warmup_done_ = { false };
    std::atomic_int num_ready_ = { 0 };
    int counter_ = 0;
public:
    explicit GSCPUTest(GameBase *game);
    ~GSCPUTest() override;

    void Enter() override;
    void Exit() override;

    void Draw(float dt_s) override;

    void Update(int dt_ms) override;

    void HandleInput(InputManager::Event) override;
};