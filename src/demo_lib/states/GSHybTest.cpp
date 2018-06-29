#include "GSHybTest.h"

#include <fstream>
#include <sstream>

#if defined(USE_SW_RENDER)
#include <ren/SW/SW.h>
#include <ren/SW/SWframebuffer.h>
#endif

#include <engine/GameStateManager.h>
#include <ray/RendererFactory.h>
#include <sys/Json.h>
#include <sys/Log.h>
#include <sys/Time_.h>
#include <sys/ThreadPool.h>
#include <ui/Renderer.h>

#include "../Viewer.h"
#include "../load/Load.h"
#include "../ui/FontStorage.h"

namespace GSHybTestInternal {
const float FORWARD_SPEED = 8.0f;
}

GSHybTest::GSHybTest(GameBase *game) : game_(game) {
    state_manager_  = game->GetComponent<GameStateManager>(STATE_MANAGER_KEY);
    ctx_            = game->GetComponent<ren::Context>(REN_CONTEXT_KEY);

    ui_renderer_    = game->GetComponent<ui::Renderer>(UI_RENDERER_KEY);
    ui_root_        = game->GetComponent<ui::BaseElement>(UI_ROOT_KEY);

    const auto fonts = game->GetComponent<FontStorage>(UI_FONTS_KEY);
    font_ = fonts->FindFont("main_font");

    gpu_tracer_ = game->GetComponent<ray::RendererBase>(RAY_RENDERER_KEY);
    if (gpu_tracer_->type() != ray::RendererOCL) throw std::runtime_error("gpu renderer is needed!");

    cpu_tracer_ = ray::CreateRenderer(game->width, game->height, ray::RendererAVX | ray::RendererSSE | ray::RendererRef);

    threads_        = game->GetComponent<sys::ThreadPool>(THREAD_POOL_KEY);
}

void GSHybTest::UpdateRegionContexts() {
    gpu_region_contexts_.clear();
    cpu_region_contexts_.clear();

    const int gpu_start_hor = (int)(ctx_->h() * gpu_cpu_div_fac_);

    {   // setup gpu renderers
        auto rect = ray::rect_t{ 0, 0, ctx_->w(), gpu_start_hor };
        gpu_region_contexts_.emplace_back(rect);
    }

    {   // setup cpu renderers
        const int BUCKET_SIZE_X = 128, BUCKET_SIZE_Y = 64;

        for (int y = gpu_start_hor; y < ctx_->h(); y += BUCKET_SIZE_Y) {
            for (int x = 0; x < ctx_->w(); x += BUCKET_SIZE_X) {
                auto rect = ray::rect_t{ x, y,
                    std::min(ctx_->w() - x, BUCKET_SIZE_X),
                    std::min(ctx_->h() - y, BUCKET_SIZE_Y) };

                cpu_region_contexts_.emplace_back(rect);
            }
        }
    }

    gpu_cpu_div_fac_dirty_ = false;
}

void GSHybTest::UpdateEnvironment(const math::vec3 &sun_dir) {
    /*if (ray_scene_) {
        ray::environment_desc_t env_desc = {};

        ray_scene_->GetEnvironment(env_desc);

        memcpy(&env_desc.sun_dir[0], math::value_ptr(sun_dir), 3 * sizeof(float));

        ray_scene_->SetEnvironment(env_desc);

        invalidate_preview_ = true;
    }*/
}

void GSHybTest::Enter() {
    using namespace math;

    JsObject js_scene;

    { 
        std::ifstream in_file("./assets/scenes/sponza.json", std::ios::binary);
        if (!js_scene.Read(in_file)) {
            LOGE("Failed to parse scene file!");
        }
    }

    if (js_scene.Size()) {
        auto ev = threads_->enqueue([&]() { cpu_scene_ = LoadScene(cpu_tracer_.get(), js_scene); });
        gpu_scene_ = LoadScene(gpu_tracer_.get(), js_scene);
        ev.wait();

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

    const auto &cam = gpu_scene_->GetCamera(0);
    view_origin_ = { cam.origin[0], cam.origin[1], cam.origin[2] };
    view_dir_ = { cam.fwd[0], cam.fwd[1], cam.fwd[2] };

    UpdateRegionContexts();
}

void GSHybTest::Exit() {

}

void GSHybTest::Draw(float dt_s) {
    //renderer_->ClearColorAndDepth(0, 0, 0, 1);

    gpu_scene_->SetCamera(0, ray::Persp, value_ptr(view_origin_), value_ptr(view_dir_), 0);
    cpu_scene_->SetCamera(0, ray::Persp, value_ptr(view_origin_), value_ptr(view_dir_), 0);

    auto t1 = sys::GetTicks();

    if (invalidate_preview_) {
        gpu_tracer_->Clear();
        cpu_tracer_->Clear();
        UpdateRegionContexts();
        invalidate_preview_ = false;
    }

    {   // invoke renderers
        auto gpu_render_job = [this](int i) { gpu_tracer_->RenderScene(gpu_scene_, gpu_region_contexts_[i]); };
        auto cpu_render_job = [this](int i) { cpu_tracer_->RenderScene(cpu_scene_, cpu_region_contexts_[i]); };

        std::vector<std::future<void>> events;

        events.push_back(threads_->enqueue(gpu_render_job, 0));

        for (int i = 0; i < (int)cpu_region_contexts_.size(); i++) {
            events.push_back(threads_->enqueue(cpu_render_job, i));
        }

        for (const auto &e : events) {
            e.wait();
        }
    }

    ray::RendererBase::stats_t st = {};

    unsigned long long cpu_total = 0, gpu_total = 0;

    {
        cpu_tracer_->GetStats(st);
        cpu_tracer_->ResetStats();

        cpu_total = st.time_primary_ray_gen_us + st.time_primary_trace_us +
            st.time_primary_shade_us + st.time_secondary_sort_us + st.time_secondary_trace_us + st.time_secondary_shade_us;
        cpu_total /= (threads_->num_workers() - 1);
    }

    {
        ray::RendererBase::stats_t _st;
        gpu_tracer_->GetStats(_st);
        gpu_tracer_->ResetStats();

        st.time_primary_ray_gen_us += _st.time_primary_ray_gen_us;
        st.time_primary_trace_us += _st.time_primary_trace_us;
        st.time_primary_shade_us += _st.time_primary_shade_us;
        st.time_secondary_sort_us += _st.time_secondary_sort_us;
        st.time_secondary_trace_us += _st.time_secondary_trace_us;
        st.time_secondary_shade_us += _st.time_secondary_shade_us;

        gpu_total = _st.time_primary_ray_gen_us + _st.time_primary_trace_us +
            _st.time_primary_shade_us + _st.time_secondary_sort_us + _st.time_secondary_trace_us + _st.time_secondary_shade_us;
    }

    //LOGI("%i %i %f", int(cpu_total / 1000), int(gpu_total / 1000), float(cpu_total)/(cpu_total + gpu_total));
    //gpu_cpu_div_fac_ = float(cpu_total) / (cpu_total + gpu_total);

    if (!gpu_cpu_div_fac_dirty_) {
        if (cpu_total > gpu_total) {
            gpu_cpu_div_fac_ += 0.02;
        } else {
            gpu_cpu_div_fac_ -= 0.02;
        }
        gpu_cpu_div_fac_dirty_ = true;
    }

    LOGI("%f", gpu_cpu_div_fac_);

    st.time_primary_ray_gen_us /= threads_->num_workers();
    st.time_primary_trace_us /= threads_->num_workers();
    st.time_primary_shade_us /= threads_->num_workers();
    st.time_secondary_sort_us /= threads_->num_workers();
    st.time_secondary_trace_us /= threads_->num_workers();
    st.time_secondary_shade_us /= threads_->num_workers();

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

    std::tie(w, h) = cpu_tracer_->size();
    const auto *gpu_pixel_data = gpu_tracer_->get_pixels_ref();
    const auto *cpu_pixel_data = cpu_tracer_->get_pixels_ref();

#if defined(USE_SW_RENDER)
    if (draw_limits_) {
        for (const auto &r : gpu_region_contexts_) {
            const auto rect = r.rect();
            for (int j = rect.y; j < rect.y + rect.h; j++) {
                const_cast<ray::pixel_color_t*>(gpu_pixel_data)[j * w + rect.x] = ray::pixel_color_t{ 0.0f, 1.0f, 0.0f, 1.0f };
                const_cast<ray::pixel_color_t*>(gpu_pixel_data)[j * w + rect.x + rect.w - 1] = ray::pixel_color_t{ 0.0f, 1.0f, 0.0f, 1.0f };
            }

            for (int j = rect.x; j < rect.x + rect.w; j++) {
                const_cast<ray::pixel_color_t*>(gpu_pixel_data)[rect.y * w + j] = ray::pixel_color_t{ 0.0f, 1.0f, 0.0f, 1.0f };
                const_cast<ray::pixel_color_t*>(gpu_pixel_data)[(rect.y + rect.h - 1) * w + j] = ray::pixel_color_t{ 0.0f, 1.0f, 0.0f, 1.0f };
            }
        }
    }
    swBlitPixels(0, 0, SW_FLOAT, SW_FRGBA, w, gpu_region_contexts_[0].rect().h, (const void *)gpu_pixel_data, 1);
    const int gpu_h = gpu_region_contexts_[0].rect().h;

    if (draw_limits_) {
        for (const auto &r : cpu_region_contexts_) {
            const auto rect = r.rect();
            for (int j = rect.y; j < rect.y + rect.h; j++) {
                const_cast<ray::pixel_color_t*>(cpu_pixel_data)[j * w + rect.x] = ray::pixel_color_t{ 1.0f, 0.0f, 0.0f, 1.0f };
                const_cast<ray::pixel_color_t*>(cpu_pixel_data)[j * w + rect.x + rect.w - 1] = ray::pixel_color_t{ 1.0f, 0.0f, 0.0f, 1.0f };
            }

            for (int j = rect.x; j < rect.x + rect.w; j++) {
                const_cast<ray::pixel_color_t*>(cpu_pixel_data)[rect.y * w + j] = ray::pixel_color_t{ 1.0f, 0.0f, 0.0f, 1.0f };
                const_cast<ray::pixel_color_t*>(cpu_pixel_data)[(rect.y + rect.h - 1) * w + j] = ray::pixel_color_t{ 1.0f, 0.0f, 0.0f, 1.0f };
            }
        }
    }
    swBlitPixels(0, gpu_h, SW_FLOAT, SW_FRGBA, w, h - gpu_h, (const void *)(cpu_pixel_data + w * gpu_h), 1);

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

        swBlitPixels(180 + off_x, 4 + (64 - l), SW_UNSIGNED_BYTE, SW_RGB, 1, l, &stat_line[0][0], 1);
        off_x++;
    }

    uint8_t hor_line[128][3];
    memset(&hor_line[0][0], 255, sizeof(hor_line));
    swBlitPixels(180, 4, SW_UNSIGNED_BYTE, SW_RGB, 128, 1, &hor_line[0][0], 1);
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
        gpu_tracer_->GetStats(st);

        float font_height = font_->height(ui_root_.get());

        std::string stats1;
        stats1 += "res:   ";
        stats1 += std::to_string(gpu_tracer_->size().first);
        stats1 += "x";
        stats1 += std::to_string(gpu_tracer_->size().second);

        std::string stats2;
        stats2 += "tris:  ";
        stats2 += std::to_string(gpu_scene_->triangle_count());

        std::string stats3;
        stats3 += "nodes: ";
        stats3 += std::to_string(gpu_scene_->node_count());

        std::string stats4;
        stats4 += "pass:  ";
        stats4 += std::to_string(gpu_region_contexts_[0].iteration);

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

void GSHybTest::Update(int dt_ms) {
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
        gpu_scene_->SetMeshInstanceTransform(1, math::value_ptr(tr));
        cpu_scene_->SetMeshInstanceTransform(1, math::value_ptr(tr));
    }
    //_L = math::normalize(_L);

    //////////////////////////////////////////////////////////////////////////


}

void GSHybTest::HandleInput(InputManager::Event evt) {
    using namespace GSHybTestInternal;
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
        } else if (evt.raw_key == 'f') {
            draw_limits_ = !draw_limits_;
            invalidate_preview_ = true;
        }
    }
    break;
    case InputManager::RAW_INPUT_RESIZE:
        gpu_tracer_->Resize((int)evt.point.x, (int)evt.point.y);
        cpu_tracer_->Resize((int)evt.point.x, (int)evt.point.y);
        UpdateRegionContexts();
        break;
    default:
        break;
    }
}
