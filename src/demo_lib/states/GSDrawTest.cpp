#include "GSDrawTest.h"

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

namespace GSDrawTestInternal {
const float CAM_CENTER[] = { 0, 0.75f, 10 },
                           CAM_TARGET[] = { 0, 0.75f, 0 },
                                   CAM_UP[] = { 0, 1, 0 };
const char BIPED_OBJ_NAME[] = "assets/models/draw_test_02.json";
}

GSDrawTest::GSDrawTest(GameBase *game) : game_(game), biped_("Biped"),
    cam_(GSDrawTestInternal::CAM_CENTER,
         GSDrawTestInternal::CAM_TARGET,
         GSDrawTestInternal::CAM_UP) {
    state_manager_	= game->GetComponent<GameStateManager>(STATE_MANAGER_KEY);
    ctx_			= game->GetComponent<ren::Context>(REN_CONTEXT_KEY);
    renderer_		= game->GetComponent<Renderer>(RENDERER_KEY);

    cam_.Perspective(8, float(game_->width) / game_->height, 0.1f, 100.0f);

    {
        std::ifstream in_file(GSDrawTestInternal::BIPED_OBJ_NAME, std::ios::binary);
        JsObject js;
        if (!js.Read(in_file)) throw std::runtime_error("Failed to parse json file!");

        if (js.Has("drawable")) {
            const JsObject &dr_js = js.at("drawable");

            std::unique_ptr<Drawable> dr(new Drawable(*ctx_));
            if (dr->Read(dr_js)) {
                biped_.AddComponent(dr.release());
            } else {
                throw std::runtime_error("Failed to create drawable!");
            }
        }

        biped_.AddComponent(new Transform);
    }
}

GSDrawTest::~GSDrawTest() {

}

void GSDrawTest::Enter() {
    grabbed_ = false;
    angle_   = 90;

#if defined(USE_SW_RENDER)
    swEnable(SW_FAST_PERSPECTIVE_CORRECTION);
    swEnable(SW_DEPTH_TEST);
#endif
}

void GSDrawTest::Exit() {

}

void GSDrawTest::Draw(float dt_s) {
    renderer_->set_current_cam(&cam_);
    renderer_->ClearColorAndDepth(0, 0, 0, 1);

    DrawSkeletal(dt_s);

    ctx_->ProcessTasks();
}

void GSDrawTest::Update(int dt_ms) {

}

void GSDrawTest::HandleInput(InputManager::Event evt) {
    switch (evt.type) {
    case InputManager::RAW_INPUT_P1_DOWN:
        grabbed_ = true;
        break;
    case InputManager::RAW_INPUT_P1_UP:
        grabbed_ = false;
        break;
    case InputManager::RAW_INPUT_P1_MOVE:
        if (grabbed_) {
            angle_ += evt.move.dx;
        }
        break;
    case InputManager::RAW_INPUT_KEY_UP: {

    } break;
    case InputManager::RAW_INPUT_RESIZE:
        cam_.Perspective(8, float(game_->width) / game_->height, 0.1f, 100.0f);
        break;
    default:
        break;
    }
}

void GSDrawTest::DrawSkeletal(float dt_s) {
    auto tr = biped_.GetComponent<Transform>();
    auto dr = biped_.GetComponent<Drawable>();
    if (tr && dr) {
        tr->SetPos(0, -0.2f, -6);
        tr->SetAngles(0, angle_, 0);
        renderer_->DrawSkeletalMatcap(tr, dr, dt_s);

        auto mesh_ref = dr->mesh();
        if (!mesh_ref) return;
        auto *mesh = mesh_ref.get();

        auto *skel = mesh->skel();
        skel->UpdateBones();
        for (size_t i = 0 ; i < skel->bones.size(); i++) {
            if (skel->bones[i].parent_id == -1) continue;

            math::vec3 p1 = skel->bone_pos(skel->bones[i].parent_id),
                       p2 = skel->bone_pos(i);

            math::mat4 m(math::noinit);
            skel->bone_matrix(i, m);

            math::mat4 world_from_object = tr->matrix();

            p1 = math::vec3(world_from_object * math::vec4(p1, 1));
            p2 = math::vec3(world_from_object * math::vec4(p2, 1));

            renderer_->DrawLine(p1, p2, { 0, 1, 1 }, false);
        }
    }
}