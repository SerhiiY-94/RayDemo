#include "GSClipTest.h"

#include <fstream>

#if defined(USE_SW_RENDER)
#include <ren/SW/SW.h>
#endif

#include <engine/GameStateManager.h>
#include <ren/Context.h>
#include <sys/Json.h>
#include <sys/Log.h>
#include <ui/Renderer.h>

#include "../Viewer.h"
#include "../comp/Drawable.h"
#include "../comp/Transform.h"
#include "../renderer/Renderer.h"

namespace GSClipTestInternal {
const float CAM_CENTER[] = { 0, 0, -1 },
                           CAM_TARGET[] = { 0, 0, 0 },
                                   CAM_UP[] = { 0, 1, 0 };
}

GSClipTest::GSClipTest(GameBase *game) : game_(game),
    cam_(GSClipTestInternal::CAM_CENTER,
         GSClipTestInternal::CAM_TARGET,
         GSClipTestInternal::CAM_UP) {
    state_manager_	= game->GetComponent<GameStateManager>(STATE_MANAGER_KEY);
    ctx_			= game->GetComponent<ren::Context>(REN_CONTEXT_KEY);
    renderer_		= game->GetComponent<Renderer>(RENDERER_KEY);

    //cam_.Perspective(8, float(game_->width) / game_->height, 0.1f, 100.0f);
    cam_.Orthographic(-10, 10, -10, 10, 0.1f, 100.0f);
}

GSClipTest::~GSClipTest() {

}

void GSClipTest::Enter() {
    grabbed_ = false;

#if defined(USE_SW_RENDER)
    swEnable(SW_FAST_PERSPECTIVE_CORRECTION);
    swEnable(SW_DEPTH_TEST);
#endif
}

void GSClipTest::Exit() {

}

namespace ray {
struct bbox_t {
    math::vec3 min = math::vec3{ std::numeric_limits<float>::max() },
               max = math::vec3{ std::numeric_limits<float>::lowest() };
    bbox_t() {}
    bbox_t(const math::vec3 &_min, const math::vec3 &_max) : min(_min), max(_max) {}
};

bbox_t GetClippedAABB(const math::vec3 &_v0, const math::vec3 &_v1, const math::vec3 &_v2, const bbox_t &limits);
}

void GSClipTest::Draw(float dt_s) {
    using namespace math;

    renderer_->set_current_cam(&cam_);
    renderer_->ClearColorAndDepth(0, 0, 0, 1);

    //vec3 p[] = { { shift_.x - 80, shift_.y -10, 0 }, { 5 + shift_.x, shift_.y + 100, 0 }, { 5 + shift_.x, shift_.y - 100, 0 } };

    float lims[2] = { -10, 10 };

    {
        vec3 b[] = { { lims[0], -100, 0 }, { lims[0], 100, 0 },
            { lims[1], -100, 0 }, { lims[1], 100, 0 }
        };

        renderer_->DrawLine(b[0], b[1], { 1, 1, 1 });
        renderer_->DrawLine(b[2], b[3], { 1, 1, 1 });
    }

    renderer_->DrawLine(p[0], p[1], { 1, 1, 1 });
    renderer_->DrawLine(p[1], p[2], { 1, 1, 1 });
    renderer_->DrawLine(p[2], p[0], { 1, 1, 1 });

    float bmin = lims[0], bmax = lims[1];
    ray::bbox_t limits;
    limits.min = { bmin, -60, 0 };
    limits.max = { bmax, 60, 0 };

    math::vec3 v0 = p[0], v1 = p[1], v2 = p[2];

    ray::bbox_t extends = ray::GetClippedAABB(v0, v1, v2, limits);

    extends.min = max(extends.min, limits.min);
    extends.max = min(extends.max, limits.max);

    if (extends.min.x < extends.max.x && extends.min.y < extends.max.y && extends.min.z <= extends.max.z) {
        renderer_->DrawLine(extends.min, { extends.min.x, extends.max.y, 0 }, { 1, 1, 1 });
        renderer_->DrawLine(extends.max, { extends.max.x, extends.min.y, 0 }, { 1, 1, 1 });

        renderer_->DrawLine(extends.min, { extends.max.x, extends.min.y, 0 }, { 1, 1, 1 });
        renderer_->DrawLine(extends.max, { extends.min.x, extends.max.y, 0 }, { 1, 1, 1 });
    }

    /////////////////////////////////////

    /*lims[0] += 20;
    lims[1] += 20;
    limits.min[0] += 20;
    limits.max[0] += 20;

    {
    	vec3 b[] = { { lims[0], -100, 0 }, { lims[0], 100, 0 },
    				 { lims[1], -100, 0 }, { lims[1], 100, 0 } };

    	renderer_->DrawLine(b[0], b[1]);
    	renderer_->DrawLine(b[2], b[3]);
    }

    extends = ray::GetClippedAABB(v0, v1, v2, limits);

    extends.min = max(extends.min, limits.min);
    extends.max = min(extends.max, limits.max);

    if (extends.min.x < extends.max.x && extends.min.y < extends.max.y && extends.min.z <= extends.max.z) {
    	renderer_->DrawLine(extends.min, { extends.min.x, extends.max.y, 0 });
    	renderer_->DrawLine(extends.max, { extends.max.x, extends.min.y, 0 });

    	renderer_->DrawLine(extends.min, { extends.max.x, extends.min.y, 0 });
    	renderer_->DrawLine(extends.max, { extends.min.x, extends.max.y, 0 });
    }*/

    ctx_->ProcessTasks();
}

void GSClipTest::Update(int dt_ms) {

}

void GSClipTest::HandleInput(InputManager::Event evt) {
    switch (evt.type) {
    case InputManager::RAW_INPUT_P1_DOWN: {
        int w = game_->width, h = game_->height;

        for (int i = 0; i < 3; i++) {
            if (math::distance(p[i],
                               math::vec3{ 100.0f - 200.0f * (evt.point.x / w), 100.0f - 200.0f * (evt.point.y / h), 0 }) < 10.0f) {
                point_grabbed_ = i;
                break;
            }
        }

        if (point_grabbed_ == -1) {
            grabbed_ = true;
        }
    }
    break;
    case InputManager::RAW_INPUT_P1_UP:
        grabbed_ = false;
        point_grabbed_ = -1;
        break;
    case InputManager::RAW_INPUT_P1_MOVE: {
        int w = game_->width, h = game_->height;

        if (grabbed_) {
            for (int i = 0; i < 3; i++) {
                p[i][0] += 200.0f * (-evt.move.dx / w);
                p[i][1] += 200.0f * (-evt.move.dy / h);
            }
        } else if (point_grabbed_ != -1) {
            p[point_grabbed_][0] += 200.0f * (-evt.move.dx / w);
            p[point_grabbed_][1] += 200.0f * (-evt.move.dy / h);
        }
    }
    break;
    case InputManager::RAW_INPUT_KEY_UP: {

    } break;
    case InputManager::RAW_INPUT_RESIZE:
        //cam_.Perspective(8, float(game_->width) / game_->height, 0.1f, 100.0f);
        break;
    default:
        break;
    }
}
