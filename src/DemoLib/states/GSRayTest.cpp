#include "GSRayTest.h"

#include <fstream>
#include <sstream>
#include <iomanip>

#if defined(USE_SW_RENDER)
#include <Ren/SW/SW.h>
#include <Ren/SW/SWframebuffer.h>
#endif

#include <Eng/GameStateManager.h>
#include <Sys/Json.h>
#include <Sys/Log.h>
#include <Sys/Time_.h>
#include <Sys/ThreadPool.h>
#include <Gui/Renderer.h>

#include "../Viewer.h"
#include "../load/Load.h"
#include "../ui/FontStorage.h"

namespace GSRayTestInternal {
const float FORWARD_SPEED = 8.0f;
}

GSRayTest::GSRayTest(GameBase *game) : game_(game) {
    state_manager_  = game->GetComponent<GameStateManager>(STATE_MANAGER_KEY);
    ctx_            = game->GetComponent<Ren::Context>(REN_CONTEXT_KEY);

    ui_renderer_    = game->GetComponent<Gui::Renderer>(UI_RENDERER_KEY);
    ui_root_        = game->GetComponent<Gui::BaseElement>(UI_ROOT_KEY);

    const auto fonts = game->GetComponent<FontStorage>(UI_FONTS_KEY);
    font_ = fonts->FindFont("main_font");

    ray_renderer_   = game->GetComponent<Ray::RendererBase>(RAY_RENDERER_KEY);

    threads_        = game->GetComponent<Sys::ThreadPool>(THREAD_POOL_KEY);

    use_coherent_sampling_ = ((Viewer *)game)->use_coherent_sampling;
}

void GSRayTest::UpdateRegionContexts() {
    region_contexts_.clear();

    const auto rt = ray_renderer_->type();
    const auto sz = ray_renderer_->size();

    if (rt == Ray::RendererRef || rt == Ray::RendererSSE2 || rt == Ray::RendererAVX || rt == Ray::RendererAVX2) {
        const int BUCKET_SIZE = 128;

        for (int y = 0; y < sz.second; y += BUCKET_SIZE) {
            for (int x = 0; x < sz.first; x += BUCKET_SIZE) {
                auto rect = Ray::rect_t{ x, y,
                    std::min(sz.first - x, BUCKET_SIZE),
                    std::min(sz.second - y, BUCKET_SIZE) };

                region_contexts_.emplace_back(rect);
            }
        }
    } else {
        auto rect = Ray::rect_t{ 0, 0, sz.first, sz.second };
        region_contexts_.emplace_back(rect);
    }
}

void GSRayTest::UpdateEnvironment(const Ren::Vec3f &sun_dir) {
    /*if (ray_scene_) {
        Ray::environment_desc_t env_desc = {};

        ray_scene_->GetEnvironment(env_desc);

        memcpy(&env_desc.sun_dir[0], Ren::ValuePtr(sun_dir), 3 * sizeof(float));

        ray_scene_->SetEnvironment(env_desc);

        invalidate_preview_ = true;
    }*/
}

void GSRayTest::Enter() {
    using namespace Ren;

    max_fwd_speed_ = GSRayTestInternal::FORWARD_SPEED;

    JsObject js_scene;

    auto scene_name = game_->GetComponent<std::string>(SCENE_NAME_KEY);

    { 
        std::ifstream in_file(*scene_name, std::ios::binary);
        if (!js_scene.Read(in_file)) {
            LOGE("Failed to parse scene file!");
        }
    }

    if (js_scene.Size()) {
        try {
            ray_scene_ = LoadScene(ray_renderer_.get(), js_scene);
        } catch (std::exception &e) {
            LOGE("%s", e.what());
        }

        if (js_scene.Has("camera")) {
            const JsObject &js_cam = js_scene.at("camera");
            if (js_cam.Has("view_target")) {
                const JsArray &js_view_target = (const JsArray &)js_cam.at("view_target");

                view_targeted_ = true;
                view_target_[0] = (float)((const JsNumber &)js_view_target.at(0)).val;
                view_target_[1] = (float)((const JsNumber &)js_view_target.at(1)).val;
                view_target_[2] = (float)((const JsNumber &)js_view_target.at(2)).val;
            }

            if (js_cam.Has("fwd_speed")) {
                max_fwd_speed_ = (float)((const JsNumber &)js_cam.at("fwd_speed")).val;
            }
        }
    }

    Ray::camera_desc_t cam_desc;
    ray_scene_->GetCamera(0, cam_desc);

    memcpy(&view_origin_[0], &cam_desc.origin[0], 3 * sizeof(float));
    memcpy(&view_dir_[0], &cam_desc.fwd[0], 3 * sizeof(float));

    UpdateRegionContexts();
#if 0
    int w, h;
    auto pixels = LoadTGA("C:\\Users\\MA\\Documents\\untitled.tga", w, h);

    {
        std::ofstream out_file("test_img1.h", std::ios::binary);
        out_file << "static const int img_w = " << w << ";\n";
        out_file << "static const int img_h = " << h << ";\n";

        out_file << "static const uint8_t img_data[] = {\n\t";

        out_file << std::hex << std::setw(2) << std::setfill('0');

        for (size_t i = 0; i < pixels.size(); i++) {
            out_file << "0x" << std::setw(2) << (int)(pixels[i].r) << ", ";
            out_file << "0x" << std::setw(2) << (int)(pixels[i].g) << ", ";
            out_file << "0x" << std::setw(2) << (int)(pixels[i].b) << ", ";
            out_file << "0x" << std::setw(2) << (int)(pixels[i].a) << ", ";
            //if (i % 64 == 0 && i != 0) out_file << "\n\t";
        }

        out_file << "\n};\n";
    }
#endif
}

void GSRayTest::Exit() {

}

void GSRayTest::Draw(uint64_t dt_us) {
    {   // update camera
        Ray::camera_desc_t cam_desc;
        ray_scene_->GetCamera(0, cam_desc);

        memcpy(&cam_desc.origin[0], Ren::ValuePtr(view_origin_), 3 * sizeof(float));
        memcpy(&cam_desc.fwd[0], Ren::ValuePtr(view_dir_), 3 * sizeof(float));
        cam_desc.up[0] = 0.0f;
        cam_desc.up[1] = 1.0f;
        cam_desc.up[2] = 0.0f;
        cam_desc.focus_distance = focal_distance_;
        cam_desc.focus_factor = 0.0f;

        if (invalidate_preview_ || false) {
            cam_desc.max_total_depth = 1;
            last_invalidate_ = true;
        } else {
            cam_desc.max_refr_depth = 9;
            cam_desc.max_total_depth = 9;
            if (last_invalidate_) {
                invalidate_preview_ = true;
                last_invalidate_ = false;
            }
        }

        cam_desc.use_coherent_sampling = use_coherent_sampling_;

        ray_scene_->SetCamera(0, cam_desc);
    }

    uint32_t t1 = Sys::GetTimeMs();

    if (invalidate_preview_ || last_invalidate_) {
        ray_renderer_->Clear();
        UpdateRegionContexts();
        invalidate_preview_ = false;
    }

    const auto rt = ray_renderer_->type();

    if (rt == Ray::RendererRef || rt == Ray::RendererSSE2 || rt == Ray::RendererAVX || rt == Ray::RendererAVX2) {
        auto render_job = [this](int i) { ray_renderer_->RenderScene(ray_scene_, region_contexts_[i]); };
        
        std::vector<std::future<void>> events;

        for (int i = 0; i < (int)region_contexts_.size(); i++) {
            events.push_back(threads_->enqueue(render_job, i));
        }

        for (const auto &e : events) {
            e.wait();
        }
    } else {
        ray_renderer_->RenderScene(ray_scene_, region_contexts_[0]);
    }

    Ray::RendererBase::stats_t st;
    ray_renderer_->GetStats(st);
    ray_renderer_->ResetStats();

    if (rt == Ray::RendererRef || rt == Ray::RendererSSE2 || rt == Ray::RendererAVX || rt == Ray::RendererAVX2) {
        st.time_primary_ray_gen_us /= threads_->num_workers();
        st.time_primary_trace_us /= threads_->num_workers();
        st.time_primary_shade_us /= threads_->num_workers();
        st.time_secondary_sort_us /= threads_->num_workers();
        st.time_secondary_trace_us /= threads_->num_workers();
        st.time_secondary_shade_us /= threads_->num_workers();
    }

    //LOGI("%llu\t%llu\t%i", st.time_primary_trace_us, st.time_secondary_trace_us, region_contexts_[0].iteration);
    LOGI("%llu\t%llu\t%llu\t%i", st.time_primary_ray_gen_us, st.time_primary_trace_us, st.time_primary_shade_us, region_contexts_[0].iteration);
    //LOGI("%llu\t%llu", st.time_secondary_sort_us, st.time_secondary_trace_us);

    stats_.push_back(st);
    if (stats_.size() > 128) {
        stats_.erase(stats_.begin());
    }

    unsigned long long time_total = 0;

    for (const auto &st : stats_) {
        unsigned long long _time_total = st.time_primary_ray_gen_us + st.time_primary_trace_us +
            st.time_primary_shade_us + st.time_secondary_sort_us + st.time_secondary_trace_us + st.time_secondary_shade_us;
        time_total = std::max(time_total, _time_total);
    }

    if (time_total % 5000 != 0) {
        time_total += 5000 - (time_total % 5000);
    }

    int w, h;

    std::tie(w, h) = ray_renderer_->size();
    const auto *pixel_data = ray_renderer_->get_pixels_ref();

#if defined(USE_SW_RENDER)
    swBlitPixels(0, 0, 0, SW_FLOAT, SW_FRGBA, w, h, (const void *)pixel_data, 1);

    if (ui_enabled_) {
        uint8_t stat_line[64][3];
        int off_x = 128 - (int)stats_.size();

        for (const auto &st : stats_) {
            int p0 = (int)(64 * float(st.time_secondary_shade_us) / time_total);
            int p1 = (int)(64 * float(st.time_secondary_trace_us + st.time_secondary_shade_us) / time_total);
            int p2 = (int)(64 * float(st.time_secondary_sort_us + st.time_secondary_trace_us + st.time_secondary_shade_us) / time_total);
            int p3 = (int)(64 * float(st.time_primary_shade_us + st.time_secondary_sort_us + st.time_secondary_trace_us +
                st.time_secondary_shade_us) / time_total);
            int p4 = (int)(64 * float(st.time_primary_trace_us + st.time_primary_shade_us + st.time_secondary_sort_us +
                st.time_secondary_trace_us + st.time_secondary_shade_us) / time_total);
            int p5 = (int)(64 * float(st.time_primary_ray_gen_us + st.time_primary_trace_us + st.time_primary_shade_us +
                st.time_secondary_sort_us + st.time_secondary_trace_us + st.time_secondary_shade_us) / time_total);

            int l = p5;

            for (int i = 0; i < p0; i++) {
                stat_line[i][0] = 0; stat_line[i][1] = 255; stat_line[i][2] = 255;
            }

            for (int i = p0; i < p1; i++) {
                stat_line[i][0] = 255; stat_line[i][1] = 0; stat_line[i][2] = 255;
            }

            for (int i = p1; i < p2; i++) {
                stat_line[i][0] = 255; stat_line[i][1] = 255; stat_line[i][2] = 0;
            }

            for (int i = p2; i < p3; i++) {
                stat_line[i][0] = 255; stat_line[i][1] = 0; stat_line[i][2] = 0;
            }

            for (int i = p3; i < p4; i++) {
                stat_line[i][0] = 0; stat_line[i][1] = 255; stat_line[i][2] = 0;
            }

            for (int i = p4; i < p5; i++) {
                stat_line[i][0] = 0; stat_line[i][1] = 0; stat_line[i][2] = 255;
            }

            swBlitPixels(180 + off_x, 4 + (64 - l), 0, SW_UNSIGNED_BYTE, SW_RGB, 1, l, &stat_line[0][0], 1);
            off_x++;
        }

        uint8_t hor_line[128][3];
        memset(&hor_line[0][0], 255, sizeof(hor_line));
        swBlitPixels(180, 4, 0, SW_UNSIGNED_BYTE, SW_RGB, 128, 1, &hor_line[0][0], 1);
    }
#endif

    int dt_ms = int(Sys::GetTimeMs() - t1);
    time_acc_ += dt_ms;
    time_counter_++;

    if (time_counter_ == 20) {
        cur_time_stat_ms_ = float(time_acc_) / time_counter_;
        time_acc_ = 0;
        time_counter_ = 0;
    }

    if (ui_enabled_) {
        ui_renderer_->BeginDraw();

        Ray::RendererBase::stats_t st = {};
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

        std::string stats6 = std::to_string(time_total/1000);
        stats6 += " ms";

        font_->DrawText(ui_renderer_.get(), stats6.c_str(), { -1 + 2 * 135.0f/w, 1 - 2 * 4.0f/h - font_height }, ui_root_.get());

        ui_renderer_->EndDraw();
    }

    ctx_->ProcessTasks();
}

void GSRayTest::Update(uint64_t dt_us) {
    using namespace Ren;

    const float Pi = 3.14159265358979323846f;

    Vec3f up = { 0, 1, 0 };
    Vec3f side = Normalize(Cross(view_dir_, up));

    view_origin_ += view_dir_ * forward_speed_;
    view_origin_ += side * side_speed_;

    invalidate_timeout_ -= int(dt_us / 1000);

    if (forward_speed_ != 0 || side_speed_ != 0 || animate_) {
        invalidate_preview_ = true;
        invalidate_timeout_ = 100;
    } else if (invalidate_timeout_ > 0) {
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
        angle += 0.05f * (0.001f * dt_us);

        Mat4f tr(1.0f);
        tr = Translate(tr, Vec3f{ 0, std::sin(angle * Pi / 180.0f) * 200.0f, 0 });
        //tr = math::rotate(tr, math::radians(angle), math::vec3{ 1, 0, 0 });
        //tr = math::rotate(tr, math::radians(angle), math::vec3{ 0, 1, 0 });
        ray_scene_->SetMeshInstanceTransform(1, ValuePtr(tr));
    }
    //_L = math::normalize(_L);

    //////////////////////////////////////////////////////////////////////////


}

void GSRayTest::HandleInput(const InputManager::Event &evt) {
    using namespace GSRayTestInternal;
    using namespace Ren;

    switch (evt.type) {
    case InputManager::RAW_INPUT_P1_DOWN:
        view_grabbed_ = true;
        break;
    case InputManager::RAW_INPUT_P1_UP:
        view_grabbed_ = false;
        break;
    case InputManager::RAW_INPUT_P1_MOVE:
        if (view_grabbed_) {
            Vec3f up = { 0, 1, 0 };
            Vec3f side = Normalize(Cross(view_dir_, up));
            up = Cross(side, view_dir_);

            Mat4f rot;
            rot = Rotate(rot, -0.01f * evt.move.dx, up);
            rot = Rotate(rot, -0.01f * evt.move.dy, side);

            Mat3f rot_m3 = Mat3f(rot);

            if (!view_targeted_) {
                view_dir_ = rot_m3 * view_dir_;
            } else {
                Vec3f dir = view_origin_ - view_target_;
                dir = rot_m3 * dir;
                view_origin_ = view_target_ + dir;
                view_dir_ = Normalize(-dir);
            }

            LOGI("%f %f %f", view_origin_[0], view_origin_[1], view_origin_[2]);
            LOGI("%f %f %f", view_dir_[0], view_dir_[1], view_dir_[2]);

            invalidate_preview_ = true;
            invalidate_timeout_ = 100;
        }
        break;
    case InputManager::RAW_INPUT_MOUSE_WHEEL:
        focal_distance_ += evt.move.dy;
        LOGI("focal distance = %f", focal_distance_);
        invalidate_preview_ = true;
        invalidate_timeout_ = 100;
        break;
    case InputManager::RAW_INPUT_KEY_DOWN: {
        if (evt.key == InputManager::RAW_INPUT_BUTTON_UP || evt.raw_key == 'w') {
            forward_speed_ = max_fwd_speed_;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_DOWN || evt.raw_key == 's') {
            forward_speed_ = -max_fwd_speed_;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_LEFT || evt.raw_key == 'a') {
            side_speed_ = -max_fwd_speed_;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_RIGHT || evt.raw_key == 'd') {
            side_speed_ = max_fwd_speed_;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_SPACE) {
            animate_ = !animate_;
        } else if (evt.raw_key == 'e' || evt.raw_key == 'q') {
            Vec3f up = { 1, 0, 0 };
            Vec3f side = Normalize(Cross(sun_dir_, up));
            up = Cross(side, sun_dir_);

            Mat4f rot;
            rot = Rotate(rot, evt.raw_key == 'e' ? 0.025f : -0.025f, up);
            rot = Rotate(rot, evt.raw_key == 'e' ? 0.025f : -0.025f, side);

            Mat3f rot_m3 = Mat3f(rot);

            sun_dir_ = sun_dir_ * rot_m3;

            UpdateEnvironment(sun_dir_);
        }
    }
    break;
    case InputManager::RAW_INPUT_KEY_UP: {
        if (evt.key == InputManager::RAW_INPUT_BUTTON_UP || evt.raw_key == 'w') {
            forward_speed_ = 0;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_DOWN || evt.raw_key == 's') {
            forward_speed_ = 0;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_LEFT || evt.raw_key == 'a') {
            side_speed_ = 0;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_RIGHT || evt.raw_key == 'd') {
            side_speed_ = 0;
        } else if (evt.raw_key == 'u') {
            ui_enabled_ = !ui_enabled_;
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
