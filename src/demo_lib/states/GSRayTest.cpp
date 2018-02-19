#include "GSRayTest.h"

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

#include <ray/RendererBase.h>

#include "../Viewer.h"
#include "../ui/FontStorage.h"

namespace {
std::tuple<std::vector<float>, std::vector<unsigned>, std::vector<unsigned>> LoadOBJ(const std::string &file_name) {
    std::vector<float> attrs;
    std::vector<unsigned> indices;
    std::vector<unsigned> groups;

    std::vector<float> v, vn, vt;

    std::ifstream in_file(file_name, std::ios::binary);
    std::string line;
    while (std::getline(in_file, line)) {
        std::stringstream ss(line);
        std::string tok;
        ss >> tok;
        if (tok == "v") {
            float val;
            ss >> val;
            v.push_back(val);
            ss >> val;
            v.push_back(val);
            ss >> val;
            v.push_back(val);
        } else if (tok == "vn") {
            float val;
            ss >> val;
            vn.push_back(val);
            ss >> val;
            vn.push_back(val);
            ss >> val;
            vn.push_back(val);
        } else if (tok == "vt") {
            float val;
            ss >> val;
            vt.push_back(val);
            ss >> val;
            vt.push_back(val);
        } else if (tok == "f") {
            for (int j = 0; j < 3; j++) {
                ss >> tok;

                std::stringstream sss(tok);

                std::getline(sss, tok, '/');
                unsigned i1 = atoi(tok.c_str()) - 1;
                std::getline(sss, tok, '/');
                unsigned i2 = atoi(tok.c_str()) - 1;
                std::getline(sss, tok, '/');
                unsigned i3 = atoi(tok.c_str()) - 1;

                bool found = false;
#if 1
                for (int i = (int)attrs.size()/8 - 1; i >= std::max(0, (int)attrs.size() / 8 - 1000); i--) {
                    if (std::abs(attrs[i * 8 + 0] - v[i1 * 3]) < 0.0000001f &&
                            std::abs(attrs[i * 8 + 1] - v[i1 * 3 + 1]) < 0.0000001f &&
                            std::abs(attrs[i * 8 + 2] - v[i1 * 3 + 2]) < 0.0000001f &&
                            std::abs(attrs[i * 8 + 3] - vn[i3 * 3]) < 0.0000001f &&
                            std::abs(attrs[i * 8 + 4] - vn[i3 * 3 + 1]) < 0.0000001f &&
                            std::abs(attrs[i * 8 + 5] - vn[i3 * 3 + 2]) < 0.0000001f &&
                            std::abs(attrs[i * 8 + 6] - vt[i2 * 2]) < 0.0000001f &&
                            std::abs(attrs[i * 8 + 7] - vt[i2 * 2 + 1]) < 0.0000001f) {
                        indices.push_back(i);
                        found = true;
                        break;
                    }
                }
#endif

                if (!found) {
                    indices.push_back(attrs.size() / 8);
                    attrs.push_back(v[i1 * 3]);
                    attrs.push_back(v[i1 * 3 + 1]);
                    attrs.push_back(v[i1 * 3 + 2]);

                    attrs.push_back(vn[i3 * 3]);
                    attrs.push_back(vn[i3 * 3 + 1]);
                    attrs.push_back(vn[i3 * 3 + 2]);

                    attrs.push_back(vt[i2 * 2]);
                    attrs.push_back(vt[i2 * 2 + 1]);
                }
            }
        } else if (tok == "g") {
            if (!groups.empty()) {
                groups.push_back(indices.size() - groups.back());
            }
            groups.push_back(indices.size());
        }
    }

    if (groups.empty()) {
        groups.push_back(0);
    }

    groups.push_back(indices.size() - groups.back());

    return std::make_tuple(attrs, indices, groups);
}

std::pair<std::vector<float>, std::vector<unsigned>> LoadRAW(const std::string &file_name) {
    std::ifstream in_file(file_name, std::ios::binary);
    uint32_t num_indices;
    in_file.read((char *)&num_indices, 4);
    uint32_t num_attrs;
    in_file.read((char *)&num_attrs, 4);
    std::vector<unsigned> indices;
    indices.resize((size_t)num_indices);
    in_file.read((char *)&indices[0], (size_t)num_indices * 4);
    std::vector<float> attrs;
    attrs.resize(num_attrs);
    in_file.read((char *)&attrs[0], (size_t)num_attrs * 4);

    return std::make_pair(attrs, indices);
}

std::vector<ray::pixel_color8_t> LoadTGA(const std::string &name, int &w, int &h) {
    std::vector<ray::pixel_color8_t> tex_data;

    {
        std::ifstream in_file(name, std::ios::binary);
        if (!in_file) return {};

        in_file.seekg(0, std::ios::end);
        size_t in_file_size = (size_t)in_file.tellg();
        in_file.seekg(0, std::ios::beg);

        std::vector<char> in_file_data(in_file_size);
        in_file.read(&in_file_data[0], in_file_size);

        ren::eTex2DFormat format;
        auto pixels = ren::ReadTGAFile(&in_file_data[0], w, h, format);

        if (format == ren::RawRGB888) {
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    tex_data.push_back({ pixels[3 * (y * w + x)], pixels[3 * (y * w + x) + 1], pixels[3 * (y * w + x) + 2], 255 });
                }
            }
        } else if (format == ren::RawRGBA8888) {
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    tex_data.push_back({ pixels[4 * (y * w + x)], pixels[4 * (y * w + x) + 1], pixels[4 * (y * w + x) + 2], pixels[4 * (y * w + x) + 3] });
                }
            }
        } else {
            assert(false);
        }
    }

    return tex_data;
}

std::shared_ptr<ray::SceneBase> LoadScene(ray::RendererBase *r, const JsObject &js_scene) {
    auto new_scene = r->CreateScene();

    math::vec3 view_origin, view_dir = { 0, 0, -1 };

    std::map<std::string, uint32_t> textures;
    std::map<std::string, uint32_t> materials;
    std::map<std::string, uint32_t> meshes;

    auto get_texture = [&](const std::string &name) {
        auto it = textures.find(name);
        if (it == textures.end()) {
            int w, h;
            auto data = LoadTGA(name, w, h);
            if (data.empty()) throw std::runtime_error("error loading texture");

            ray::tex_desc_t tex_desc;
            tex_desc.data = &data[0];
            tex_desc.w = w;
            tex_desc.h = h;

            uint32_t tex_id = new_scene->AddTexture(tex_desc);
            textures[name] = tex_id;
            return tex_id;
        } else {
            return it->second;
        }
    };

    try {
        if (js_scene.Has("camera")) {
            const JsObject &js_cam = js_scene.at("camera");
            if (js_cam.Has("view_origin")) {
                const JsArray &js_view_origin = js_cam.at("view_origin");

                view_origin.x = (float)((const JsNumber &)js_view_origin.at(0)).val;
                view_origin.y = (float)((const JsNumber &)js_view_origin.at(1)).val;
                view_origin.z = (float)((const JsNumber &)js_view_origin.at(2)).val;
            }

            if (js_cam.Has("view_dir")) {
                const JsArray &js_view_dir = js_cam.at("view_dir");

                view_dir.x = (float)((const JsNumber &)js_view_dir.at(0)).val;
                view_dir.y = (float)((const JsNumber &)js_view_dir.at(1)).val;
                view_dir.z = (float)((const JsNumber &)js_view_dir.at(2)).val;
            }
        }

        if (js_scene.Has("environment")) {
            const JsObject &js_env = js_scene.at("environment");
            const JsArray &js_sun_dir = js_env.at("sun_dir");
            const JsArray &js_sun_col = js_env.at("sun_col");
            const JsArray &js_sky_col = js_env.at("sky_col");

            ray::environment_desc_t env_desc;

            env_desc.sun_dir[0] = (float)((const JsNumber &)js_sun_dir.at(0)).val;
            env_desc.sun_dir[1] = (float)((const JsNumber &)js_sun_dir.at(1)).val;
            env_desc.sun_dir[2] = (float)((const JsNumber &)js_sun_dir.at(2)).val;

            env_desc.sun_col[0] = (float)((const JsNumber &)js_sun_col.at(0)).val;
            env_desc.sun_col[1] = (float)((const JsNumber &)js_sun_col.at(1)).val;
            env_desc.sun_col[2] = (float)((const JsNumber &)js_sun_col.at(2)).val;

            env_desc.sky_col[0] = (float)((const JsNumber &)js_sky_col.at(0)).val;
            env_desc.sky_col[1] = (float)((const JsNumber &)js_sky_col.at(1)).val;
            env_desc.sky_col[2] = (float)((const JsNumber &)js_sky_col.at(2)).val;

            env_desc.sun_softness = 0;
            if (js_env.Has("sun_softness")) {
                const JsNumber &js_sun_softness = js_env.at("sun_softness");
                env_desc.sun_softness = (float)js_sun_softness.val;
            }

            new_scene->SetEnvironment(env_desc);
        }

        const JsObject &js_materials = js_scene.at("materials");
        for (const auto &js_mat : js_materials.elements) {
            const JsString &js_mat_name = js_mat.first;
            const JsObject &js_mat_obj = js_mat.second;

            ray::mat_desc_t mat_desc;

            const JsString &js_type = js_mat_obj.at("type");

            const JsString &js_main_tex = js_mat_obj.at("main_texture");
            mat_desc.main_texture = get_texture(js_main_tex.val);

            if (js_mat_obj.Has("main_color")) {
                const JsArray &js_main_color = js_mat_obj.at("main_color");
                mat_desc.main_color[0] = (float)((const JsNumber &)js_main_color.at(0)).val;
                mat_desc.main_color[1] = (float)((const JsNumber &)js_main_color.at(1)).val;
                mat_desc.main_color[2] = (float)((const JsNumber &)js_main_color.at(2)).val;
            }

            if (js_mat_obj.Has("normal_map")) {
                const JsString &js_normal_map = js_mat_obj.at("normal_map");
                mat_desc.normal_map = get_texture(js_normal_map.val);
            }

            if (js_mat_obj.Has("roughness")) {
                const JsNumber &js_roughness = js_mat_obj.at("roughness");
                mat_desc.roughness = (float)js_roughness.val;
            } else if (js_mat_obj.Has("strength")) {
                const JsNumber &js_strength = js_mat_obj.at("strength");
                mat_desc.strength = (float)js_strength.val;
            }

            if (js_mat_obj.Has("fresnel")) {
                const JsNumber &js_fresnel = js_mat_obj.at("fresnel");
                mat_desc.fresnel = (float)js_fresnel.val;
            }

            if (js_mat_obj.Has("ior")) {
                const JsNumber &js_ior = js_mat_obj.at("ior");
                mat_desc.ior = (float)js_ior.val;
            }

            if (js_type.val == "diffuse") {
                mat_desc.type = ray::DiffuseMaterial;
            } else if (js_type.val == "glossy") {
                mat_desc.type = ray::GlossyMaterial;
            } else if (js_type.val == "refractive") {
                mat_desc.type = ray::RefractiveMaterial;
            } else if (js_type.val == "emissive") {
                mat_desc.type = ray::EmissiveMaterial;
            } else if (js_type.val == "mix") {
                mat_desc.type = ray::MixMaterial;

                const JsArray &mix_materials = js_mat_obj.at("materials");
                for (const auto &m : mix_materials.elements) {
                    auto it = materials.find(static_cast<const JsString &>(m).val);
                    if (it != materials.end()) {
                        if (mat_desc.mix_materials[0] == 0xffffffff) {
                            mat_desc.mix_materials[0] = it->second;
                        } else {
                            mat_desc.mix_materials[1] = it->second;
                        }
                    }
                }
            } else if (js_type.val == "transparent") {
                mat_desc.type = ray::TransparentMaterial;
            } else {
                throw std::runtime_error("unknown material type");
            }

            materials[js_mat_name.val] = new_scene->AddMaterial(mat_desc);
        }

        const JsObject &js_meshes = js_scene.at("meshes");
        for (const auto &js_mesh : js_meshes.elements) {
            const JsString &js_mesh_name = js_mesh.first;
            const JsObject &js_mesh_obj = js_mesh.second;

            std::vector<float> attrs;
            std::vector<unsigned> indices, groups;

            const JsString &js_vtx_data = js_mesh_obj.at("vertex_data");
            if (js_vtx_data.val.find(".obj") != std::string::npos) {
                std::tie(attrs, indices, groups) = LoadOBJ(js_vtx_data.val);
            } else if (js_vtx_data.val.find(".raw") != std::string::npos) {

            } else {
                throw std::runtime_error("unknown mesh type");
            }

            const JsArray &js_materials = js_mesh_obj.at("materials");

            ray::mesh_desc_t mesh_desc;
            mesh_desc.prim_type = ray::TriangleList;
            mesh_desc.layout = ray::PxyzNxyzTuv;
            mesh_desc.vtx_attrs = &attrs[0];
            mesh_desc.vtx_attrs_count = attrs.size() / 8;
            mesh_desc.vtx_indices = &indices[0];
            mesh_desc.vtx_indices_count = indices.size();

            for (size_t i = 0; i < groups.size(); i += 2) {
                const JsString &js_mat_name = js_materials.at(i / 2);
                uint32_t mat_index = materials.at(js_mat_name.val);
                mesh_desc.shapes.push_back({ mat_index, groups[i], groups[i + 1] });
            }

            meshes[js_mesh_name.val] = new_scene->AddMesh(mesh_desc);
        }

        const JsArray &js_mesh_instances = js_scene.at("mesh_instances");
        for (const auto &js_mesh_instance : js_mesh_instances.elements) {
            const JsObject &js_mesh_instance_obj = js_mesh_instance;
            const JsString &js_mesh_name = js_mesh_instance_obj.at("mesh");

            math::mat4 transform;

            if (js_mesh_instance_obj.Has("pos")) {
                const JsArray &js_pos = js_mesh_instance_obj.at("pos");

                float px = (float)((const JsNumber &)js_pos.at(0)).val;
                float py = (float)((const JsNumber &)js_pos.at(1)).val;
                float pz = (float)((const JsNumber &)js_pos.at(2)).val;

                transform = math::translate(transform, { px, py, pz });
            }

            uint32_t mesh_index = meshes.at(js_mesh_name.val);
            new_scene->AddMeshInstance(mesh_index, math::value_ptr(transform));
        }
    } catch (std::runtime_error &e) {
        LOGE("Error in parsing json file! %s", e.what());
        return nullptr;
    }

    new_scene->AddCamera(ray::Persp, math::value_ptr(view_origin), math::value_ptr(view_dir), 45.0f);

    return new_scene;
}

const float FORWARD_SPEED = 8.0f;
}

GSRayTest::GSRayTest(GameBase *game) : game_(game) {
    state_manager_  = game->GetComponent<GameStateManager>(STATE_MANAGER_KEY);
    ctx_            = game->GetComponent<ren::Context>(REN_CONTEXT_KEY);

    ui_renderer_    = game->GetComponent<ui::Renderer>(UI_RENDERER_KEY);
    ui_root_        = game->GetComponent<ui::BaseElement>(UI_ROOT_KEY);

    const auto fonts = game->GetComponent<FontStorage>(UI_FONTS_KEY);
    font_ = fonts->FindFont("main_font");

    ray_renderer_   = game->GetComponent<ray::RendererBase>(RAY_RENDERER_KEY);
}

void GSRayTest::UpdateEnvironment(const math::vec3 &sun_dir) {
    if (ray_scene_) {
        ray::environment_desc_t env_desc = {};

        ray_scene_->GetEnvironment(env_desc);

        memcpy(&env_desc.sun_dir[0], math::value_ptr(sun_dir), 3 * sizeof(float));

        ray_scene_->SetEnvironment(env_desc);

        invalidate_preview_ = true;
    }
}

void GSRayTest::Enter() {
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
}

void GSRayTest::Exit() {

}

void GSRayTest::Draw(float dt_s) {
    //renderer_->ClearColorAndDepth(0, 0, 0, 1);

    ray_scene_->SetCamera(0, ray::Persp, value_ptr(view_origin_), value_ptr(view_dir_), 0);

    auto t1 = sys::GetTicks();

    if (invalidate_preview_) {
        ray_renderer_->Clear();
        invalidate_preview_ = false;
    }

    const auto rt = ray_renderer_->type();
    const auto sz = ray_renderer_->size();

    if (rt == ray::RendererRef || rt == ray::RendererSSE || rt == ray::RendererAVX) {
        const int BUCKET_SIZE = 128;

        auto render_job = [this](const ray::region_t &r) { ray_renderer_->RenderScene(ray_scene_, r); };
        
        std::vector<std::future<void>> events;

        for (int y = 0; y < sz.second; y += BUCKET_SIZE) {
            for (int x = 0; x < sz.first; x += BUCKET_SIZE) {
                auto reg = ray::region_t{ x, y, 
                    std::min(sz.first - x, BUCKET_SIZE),
                    std::min(sz.second - y, BUCKET_SIZE) };

                events.push_back(threads_->enqueue(render_job, reg));
            }
        }

        for (const auto &r : events) {
            r.wait();
        }
    } else {
        ray_renderer_->RenderScene(ray_scene_);
    }

    int w, h;

    std::tie(w, h) = ray_renderer_->size();
    const auto *pixel_data = ray_renderer_->get_pixels_ref();

#if defined(USE_SW_RENDER)
    swBlitPixels(0, 0, SW_FLOAT, SW_FRGBA, w, h, (const void *)pixel_data, 1);
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
        stats4 += std::to_string(st.iterations_count);

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

void GSRayTest::Update(int dt_ms) {
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

void GSRayTest::HandleInput(InputManager::Event evt) {
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

        break;
    default:
        break;
    }
}
