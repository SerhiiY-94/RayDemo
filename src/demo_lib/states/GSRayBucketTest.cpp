#include "GSRayBucketTest.h"

#include <fstream>
#include <sstream>

#if defined(USE_SW_RENDER)
#include <ren/SW/SW.h>
#include <ren/SW/SWframebuffer.h>
#endif

#include <engine/GameStateManager.h>
#include <ren/Context.h>
#include <ren/Utils.h>
#include <sys/AssetFile.h>
#include <sys/Json.h>
#include <sys/Log.h>
#include <sys/Time_.h>
#include <sys/ThreadPool.h>
#include <ui/Renderer.h>

#include "../Viewer.h"
#include "../load/Load.h"
#include "../ui/FontStorage.h"

namespace GSRayBucketTestInternal {
const float FORWARD_SPEED = 8.0f;
const int BUCKET_SIZE = 32;
const int SPP = 64;
const int SPP_PORTION = 8;
}

GSRayBucketTest::GSRayBucketTest(GameBase *game) : game_(game) {
    state_manager_  = game->GetComponent<GameStateManager>(STATE_MANAGER_KEY);
    ctx_            = game->GetComponent<ren::Context>(REN_CONTEXT_KEY);

    ui_renderer_    = game->GetComponent<ui::Renderer>(UI_RENDERER_KEY);
    ui_root_        = game->GetComponent<ui::BaseElement>(UI_ROOT_KEY);

    const auto fonts = game->GetComponent<FontStorage>(UI_FONTS_KEY);
    font_ = fonts->FindFont("main_font");

    ray_renderer_   = game->GetComponent<ray::RendererBase>(RAY_RENDERER_KEY);
}

void GSRayBucketTest::UpdateRegionContexts() {
    using namespace GSRayBucketTestInternal;

    region_contexts_.clear();
    last_reg_context_ = 0;
    cur_spp_ = 0;

    const auto rt = ray_renderer_->type();
    const auto sz = ray_renderer_->size();

    if (rt == ray::RendererRef || rt == ray::RendererSSE || rt == ray::RendererAVX) {
        for (int y = 0; y < sz.second; y += BUCKET_SIZE) {
            for (int x = 0; x < sz.first; x += BUCKET_SIZE) {
                auto rect = ray::rect_t{ x, y, 
                    std::min(sz.first - x, BUCKET_SIZE),
                    std::min(sz.second - y, BUCKET_SIZE) };

                region_contexts_.emplace_back(rect);
            }
        }
    } else {
        auto rect = ray::rect_t{ 0, 0, sz.first, sz.second };
        region_contexts_.emplace_back(rect);
    }
}

void GSRayBucketTest::UpdateEnvironment(const math::vec3 &sun_dir) {
    if (ray_scene_) {
        ray::environment_desc_t env_desc = {};

        ray_scene_->GetEnvironment(env_desc);

        memcpy(&env_desc.sun_dir[0], math::value_ptr(sun_dir), 3 * sizeof(float));

        ray_scene_->SetEnvironment(env_desc);

        invalidate_preview_ = true;
    }
}

void GSRayBucketTest::Enter() {
    using namespace math;

    JsObject js_scene;

    {
        std::ifstream in_file("./assets/scenes/sponza_simple.json", std::ios::binary);
        if (!js_scene.Read(in_file)) {
            LOGE("Failed to parse scene file!");
        }
    }

    if (js_scene.Size()) {
        ray_scene_ = LoadScene(ray_renderer_.get(), js_scene);

        if (js_scene.Has("camera")) {
            const JsObject &js_cam = js_scene.at("camera");
            if (js_cam.Has("view_target")) {
                const JsArray &js_view_target = (const JsArray &)js_cam.at("view_target");

                view_targeted_ = true;
                view_target_.x = (float)((const JsNumber &)js_view_target.at(0)).val;
                view_target_.y = (float)((const JsNumber &)js_view_target.at(1)).val;
                view_target_.z = (float)((const JsNumber &)js_view_target.at(2)).val;
            }
        }
    }

    const auto &cam = ray_scene_->GetCamera(0);
    view_origin_ = { cam.origin[0], cam.origin[1], cam.origin[2] };
    view_dir_ = { cam.fwd[0], cam.fwd[1], cam.fwd[2] };

    auto num_threads = std::max(1u, std::thread::hardware_concurrency());
    threads_ = std::make_shared<sys::ThreadPool>(num_threads);

    num_buckets_ = (int)num_threads;
    color_table_.resize(num_buckets_ * 3);

    for (int i = 0; i < num_buckets_; i++) {
        color_table_[i * 3 + 0] = float(rand())/RAND_MAX;
        color_table_[i * 3 + 1] = float(rand())/RAND_MAX;
        color_table_[i * 3 + 2] = float(rand())/RAND_MAX;
    }

    UpdateRegionContexts();
}

void GSRayBucketTest::Exit() {

}

void GSRayBucketTest::Draw(float dt_s) {
    using namespace GSRayBucketTestInternal;

    //renderer_->ClearColorAndDepth(0, 0, 0, 1);

    ray_scene_->SetCamera(0, ray::Persp, value_ptr(view_origin_), value_ptr(view_dir_), 0);

    auto t1 = sys::GetTicks();

    if (invalidate_preview_) {
        ray_renderer_->Clear();
        UpdateRegionContexts();
        invalidate_preview_ = false;
    }

    const auto rt = ray_renderer_->type();

    auto last_reg_context = last_reg_context_;

    if (rt == ray::RendererRef || rt == ray::RendererSSE || rt == ray::RendererAVX) {
        auto render_job = [this](int i) { for (int j = 0; j < SPP_PORTION; j++) ray_renderer_->RenderScene(ray_scene_, region_contexts_[i]); };
        
        std::vector<std::future<void>> events;

        for (int i = 0; i < num_buckets_; i++) {
            if (last_reg_context + i >= (int)region_contexts_.size()) break;

            events.push_back(threads_->enqueue(render_job, last_reg_context + i));
        }

        for (const auto &e : events) {
            e.wait();
        }

        cur_spp_ += SPP_PORTION;
        if (cur_spp_ >= SPP) {
            last_reg_context_ += num_buckets_;
            cur_spp_ = 0;
        }
    } else {
        ray_renderer_->RenderScene(ray_scene_, region_contexts_[0]);
    }

    if (last_reg_context_ >= (int)region_contexts_.size()) last_reg_context_ = 0;

    int w, h;

    std::tie(w, h) = ray_renderer_->size();
    const auto *pixel_data = ray_renderer_->get_pixels_ref();

#if defined(USE_SW_RENDER)
    swBlitPixels(0, 0, SW_FLOAT, SW_FRGBA, w, h, (const void *)pixel_data, 1);

    float pix_row[BUCKET_SIZE][4];

    for (int i = 0; i < num_buckets_; i++) {
        if (last_reg_context + i >= (int)region_contexts_.size()) break;

        for (int j = 0; j < BUCKET_SIZE; j++) {
            pix_row[j][0] = color_table_[i * 3 + 0];
            pix_row[j][1] = color_table_[i * 3 + 1];
            pix_row[j][2] = color_table_[i * 3 + 2];
            pix_row[j][3] = 1.0f;
        }

        const auto &rc = region_contexts_[last_reg_context + i];
        swBlitPixels(rc.rect().x, rc.rect().y, SW_FLOAT, SW_FRGBA, rc.rect().w, 1, (const void *)&pix_row[0][0], 1);
        swBlitPixels(rc.rect().x, rc.rect().y + 1, SW_FLOAT, SW_FRGBA, 1, 1, (const void *)&pix_row[0][0], 1);
        swBlitPixels(rc.rect().x, rc.rect().y + 2, SW_FLOAT, SW_FRGBA, 1, 1, (const void *)&pix_row[0][0], 1);
        swBlitPixels(rc.rect().x, rc.rect().y + 3, SW_FLOAT, SW_FRGBA, 1, 1, (const void *)&pix_row[0][0], 1);
        swBlitPixels(rc.rect().x, rc.rect().y + 4, SW_FLOAT, SW_FRGBA, 1, 1, (const void *)&pix_row[0][0], 1);
        swBlitPixels(rc.rect().x + BUCKET_SIZE - 1, rc.rect().y + 1, SW_FLOAT, SW_FRGBA, 1, 1, (const void *)&pix_row[0][0], 1);
        swBlitPixels(rc.rect().x + BUCKET_SIZE - 1, rc.rect().y + 2, SW_FLOAT, SW_FRGBA, 1, 1, (const void *)&pix_row[0][0], 1);
        swBlitPixels(rc.rect().x + BUCKET_SIZE - 1, rc.rect().y + 3, SW_FLOAT, SW_FRGBA, 1, 1, (const void *)&pix_row[0][0], 1);
        swBlitPixels(rc.rect().x + BUCKET_SIZE - 1, rc.rect().y + 4, SW_FLOAT, SW_FRGBA, 1, 1, (const void *)&pix_row[0][0], 1);

        swBlitPixels(rc.rect().x, rc.rect().y + BUCKET_SIZE - 1, SW_FLOAT, SW_FRGBA, rc.rect().w, 1, (const void *)&pix_row[0][0], 1);
        swBlitPixels(rc.rect().x, rc.rect().y + BUCKET_SIZE - 1 - 1, SW_FLOAT, SW_FRGBA, 1, 1, (const void *)&pix_row[0][0], 1);
        swBlitPixels(rc.rect().x, rc.rect().y + BUCKET_SIZE - 1 - 2, SW_FLOAT, SW_FRGBA, 1, 1, (const void *)&pix_row[0][0], 1);
        swBlitPixels(rc.rect().x, rc.rect().y + BUCKET_SIZE - 1 - 3, SW_FLOAT, SW_FRGBA, 1, 1, (const void *)&pix_row[0][0], 1);
        swBlitPixels(rc.rect().x, rc.rect().y + BUCKET_SIZE - 1 - 4, SW_FLOAT, SW_FRGBA, 1, 1, (const void *)&pix_row[0][0], 1);
        swBlitPixels(rc.rect().x + BUCKET_SIZE - 1, rc.rect().y + BUCKET_SIZE - 1 - 1, SW_FLOAT, SW_FRGBA, 1, 1, (const void *)&pix_row[0][0], 1);
        swBlitPixels(rc.rect().x + BUCKET_SIZE - 1, rc.rect().y + BUCKET_SIZE - 1 - 2, SW_FLOAT, SW_FRGBA, 1, 1, (const void *)&pix_row[0][0], 1);
        swBlitPixels(rc.rect().x + BUCKET_SIZE - 1, rc.rect().y + BUCKET_SIZE - 1 - 3, SW_FLOAT, SW_FRGBA, 1, 1, (const void *)&pix_row[0][0], 1);
        swBlitPixels(rc.rect().x + BUCKET_SIZE - 1, rc.rect().y + BUCKET_SIZE - 1 - 4, SW_FLOAT, SW_FRGBA, 1, 1, (const void *)&pix_row[0][0], 1);
    }
#endif

    auto dt_ms = int(sys::GetTicks() - t1);
    time_acc_ += dt_ms;
    time_counter_++;

    if (time_counter_ == 20) {
        cur_time_stat_ms_ = float(time_acc_) / time_counter_;
        time_acc_ = 0;
        time_counter_ = 0;
    }

    {
        // ui draw
        ui_renderer_->BeginDraw();

        ray::RendererBase::stats_t st = {};
        ray_renderer_->GetStats(st);

        float font_height = font_->height(ui_root_.get());

        std::string stats1;
        stats1 += "res:   ";
        stats1 += std::to_string(ray_renderer_->size().first);
        stats1 += "x";
        stats1 += std::to_string(ray_renderer_->size().second);

        std::string stats2;
        stats2 += "tris:  ";
        stats2 += std::to_string(ray_scene_->triangle_count());

        std::string stats3;
        stats3 += "nodes: ";
        stats3 += std::to_string(ray_scene_->node_count());

        std::string stats4;
        stats4 += "pass:  ";
        stats4 += std::to_string(region_contexts_[0].iteration);

        std::string stats5;
        stats5 += "time:  ";
        stats5 += std::to_string(cur_time_stat_ms_);
        stats5 += " ms";

        font_->DrawText(ui_renderer_.get(), stats1.c_str(), { -1, 1 - 1 * font_height }, ui_root_.get());
        font_->DrawText(ui_renderer_.get(), stats2.c_str(), { -1, 1 - 2 * font_height }, ui_root_.get());
        font_->DrawText(ui_renderer_.get(), stats3.c_str(), { -1, 1 - 3 * font_height }, ui_root_.get());
        font_->DrawText(ui_renderer_.get(), stats4.c_str(), { -1, 1 - 4 * font_height }, ui_root_.get());
        font_->DrawText(ui_renderer_.get(), stats5.c_str(), { -1, 1 - 5 * font_height }, ui_root_.get());

        ui_renderer_->EndDraw();
    }

    ctx_->ProcessTasks();
}

void GSRayBucketTest::Update(int dt_ms) {
    using namespace math;

    vec3 up = { 0, 1, 0 };
    vec3 side = normalize(cross(view_dir_, up));

    view_origin_ += view_dir_ * forward_speed_;
    view_origin_ += side * side_speed_;

    if (forward_speed_ != 0 || side_speed_ != 0 || animate_) {
        invalidate_preview_ = true;
    }

    //////////////////////////////////////////////////////////////////////////

    if (animate_) {
        /*float dt_s = 0.001f * dt_ms;
        float angle = 0.5f * dt_s;

        math::mat4 rot;
        rot = math::rotate(rot, angle, { 0, 1, 0 });

        math::mat3 rot_m3 = math::mat3(rot);
        _L = _L * rot_m3;*/

        static float angle = 0;
        angle += 0.05f * dt_ms;

        math::mat4 tr(1.0f);
        tr = math::translate(tr, math::vec3{ 0, math::sin(math::radians(angle)) * 200.0f, 0 });
        //tr = math::rotate(tr, math::radians(angle), math::vec3{ 1, 0, 0 });
        //tr = math::rotate(tr, math::radians(angle), math::vec3{ 0, 1, 0 });
        ray_scene_->SetMeshInstanceTransform(1, math::value_ptr(tr));
    }
    //_L = math::normalize(_L);

    //////////////////////////////////////////////////////////////////////////


}

void GSRayBucketTest::HandleInput(InputManager::Event evt) {
    using namespace GSRayBucketTestInternal;
    using namespace math;

    switch (evt.type) {
    case InputManager::RAW_INPUT_P1_DOWN:
        view_grabbed_ = true;
        break;
    case InputManager::RAW_INPUT_P1_UP:
        view_grabbed_ = false;
        break;
    case InputManager::RAW_INPUT_P1_MOVE:
        if (view_grabbed_) {
            vec3 up = { 0, 1, 0 };
            vec3 side = normalize(cross(view_dir_, up));
            up = cross(side, view_dir_);

            mat4 rot;
            rot = rotate(rot, 0.01f * evt.move.dx, up);
            rot = rotate(rot, 0.01f * evt.move.dy, side);

            mat3 rot_m3 = mat3(rot);

            if (!view_targeted_) {
                view_dir_ = view_dir_ * rot_m3;
            } else {
                vec3 dir = view_origin_ - view_target_;
                dir = dir * rot_m3;
                view_origin_ = view_target_ + dir;
                view_dir_ = normalize(-dir);
            }

            invalidate_preview_ = true;
        }
        break;
    case InputManager::RAW_INPUT_KEY_DOWN: {
        if (evt.key == InputManager::RAW_INPUT_BUTTON_UP) {
            forward_speed_ = FORWARD_SPEED;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_DOWN) {
            forward_speed_ = -FORWARD_SPEED;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_LEFT) {
            side_speed_ = -FORWARD_SPEED;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_RIGHT) {
            side_speed_ = FORWARD_SPEED;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_SPACE) {
            animate_ = !animate_;
        } else if (evt.raw_key == 'e' || evt.raw_key == 'q') {
            vec3 up = { 1, 0, 0 };
            vec3 side = normalize(cross(sun_dir_, up));
            up = cross(side, sun_dir_);

            mat4 rot;
            rot = rotate(rot, evt.raw_key == 'e' ? 0.025f : -0.025f, up);
            rot = rotate(rot, evt.raw_key == 'e' ? 0.025f : -0.025f, side);

            mat3 rot_m3 = mat3(rot);

            sun_dir_ = sun_dir_ * rot_m3;

            UpdateEnvironment(sun_dir_);
        }
    }
    break;
    case InputManager::RAW_INPUT_KEY_UP: {
        if (evt.key == InputManager::RAW_INPUT_BUTTON_UP) {
            forward_speed_ = 0;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_DOWN) {
            forward_speed_ = 0;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_LEFT) {
            side_speed_ = 0;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_RIGHT) {
            side_speed_ = 0;
        }
    }
    break;
    case InputManager::RAW_INPUT_RESIZE:
        ray_renderer_->Resize((int)evt.point.x, (int)evt.point.y);
        UpdateRegionContexts();
        break;
    default:
        break;
    }
}
