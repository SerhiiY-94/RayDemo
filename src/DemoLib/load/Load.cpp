#include "Load.h"

#include <cassert>
#include <cctype>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>

#include <Ren/MMat.h>
#include <Ren/Texture.h>
#include <Ren/Utils.h>
#include <Sys/AssetFile.h>
#include <Sys/Log.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_PKM
#include <Ren/SOIL2/stb_image.h>

#define _abs(x) ((x) > 0.0f ? (x) : -(x))

std::shared_ptr<Ray::SceneBase> LoadScene(Ray::RendererBase *r, const JsObject &js_scene) {
    auto new_scene = r->CreateScene();

    bool view_targeted = false;
    Ren::Vec3f view_origin, view_dir = { 0, 0, -1 }, view_target;
    float view_fov = 45.0f;

    std::map<std::string, uint32_t> textures;
    std::map<std::string, uint32_t> materials;
    std::map<std::string, uint32_t> meshes;

    auto get_texture = [&](const std::string &name, bool gen_mipmaps) {
        auto it = textures.find(name);
        if (it == textures.end()) {
            int w, h;
            std::vector<Ray::pixel_color8_t> data;
            if (std::tolower(name[name.length() - 1]) == 'r' &&
                std::tolower(name[name.length() - 2]) == 'd' &&
                std::tolower(name[name.length() - 3]) == 'h') {
                data = LoadHDR(name, w, h);
            } else {
                data = Load_stb_image(name, w, h);
            }
            if (data.empty()) throw std::runtime_error("error loading texture");

            Ray::tex_desc_t tex_desc;
            tex_desc.data = &data[0];
            tex_desc.w = w;
            tex_desc.h = h;
            tex_desc.generate_mipmaps = gen_mipmaps;

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

                view_origin[0] = (float)((const JsNumber &)js_view_origin.at(0)).val;
                view_origin[1] = (float)((const JsNumber &)js_view_origin.at(1)).val;
                view_origin[2] = (float)((const JsNumber &)js_view_origin.at(2)).val;
            }

            if (js_cam.Has("view_dir")) {
                const JsArray &js_view_dir = js_cam.at("view_dir");

                view_dir[0] = (float)((const JsNumber &)js_view_dir.at(0)).val;
                view_dir[1] = (float)((const JsNumber &)js_view_dir.at(1)).val;
                view_dir[2] = (float)((const JsNumber &)js_view_dir.at(2)).val;
            }

            if (js_cam.Has("view_target")) {
                const JsArray &js_view_target = (const JsArray &)js_cam.at("view_target");

                view_target[0] = (float)((const JsNumber &)js_view_target.at(0)).val;
                view_target[1] = (float)((const JsNumber &)js_view_target.at(1)).val;
                view_target[2] = (float)((const JsNumber &)js_view_target.at(2)).val;

                view_targeted = true;
            }

            if (js_cam.Has("fov")) {
                const JsNumber &js_view_fov = js_cam.at("fov");

                view_fov = (float)js_view_fov.val;
            }
        }

        if (js_scene.Has("environment")) {
            const JsObject &js_env = js_scene.at("environment");
            const JsArray &js_sun_dir = js_env.at("sun_dir");
            const JsArray &js_sun_col = js_env.at("sun_col");

            {   Ray::environment_desc_t env_desc;
                env_desc.env_col[0] = env_desc.env_col[1] = env_desc.env_col[2] = 0.0f;

                if (js_env.Has("env_col")) {
                    const JsArray &js_env_col = js_env.at("env_col");
                    env_desc.env_col[0] = (float)((const JsNumber &)js_env_col.at(0)).val;
                    env_desc.env_col[1] = (float)((const JsNumber &)js_env_col.at(1)).val;
                    env_desc.env_col[2] = (float)((const JsNumber &)js_env_col.at(2)).val;
                }

                if (js_env.Has("env_clamp")) {
                    const JsNumber &js_env_clamp = js_env.at("env_clamp");
                    env_desc.env_clamp = (float)js_env_clamp.val;
                }

                if (js_env.Has("env_map")) {
                    const JsString &js_env_map = js_env.at("env_map");
                    env_desc.env_map = get_texture(js_env_map.val, false);
                }

                new_scene->SetEnvironment(env_desc);
            }

            {   Ray::light_desc_t sun_desc;
                sun_desc.type = Ray::DirectionalLight;

                sun_desc.direction[0] = (float)((const JsNumber &)js_sun_dir.at(0)).val;
                sun_desc.direction[1] = (float)((const JsNumber &)js_sun_dir.at(1)).val;
                sun_desc.direction[2] = (float)((const JsNumber &)js_sun_dir.at(2)).val;

                sun_desc.color[0] = (float)((const JsNumber &)js_sun_col.at(0)).val;
                sun_desc.color[1] = (float)((const JsNumber &)js_sun_col.at(1)).val;
                sun_desc.color[2] = (float)((const JsNumber &)js_sun_col.at(2)).val;

                sun_desc.angle = 0.0f;
                if (js_env.Has("sun_softness")) {
                    const JsNumber &js_sun_softness = js_env.at("sun_softness");
                    sun_desc.angle = (float)js_sun_softness.val;
                }

                new_scene->AddLight(sun_desc);
            }
        }

        const JsObject &js_materials = js_scene.at("materials");
        for (const auto &js_mat : js_materials.elements) {
            const std::string &js_mat_name = js_mat.first;
            const JsObject &js_mat_obj = js_mat.second;

            Ray::mat_desc_t mat_desc;

            const JsString &js_type = js_mat_obj.at("type");

            const JsString &js_main_tex = js_mat_obj.at("main_texture");
            mat_desc.main_texture = get_texture(js_main_tex.val, js_type.val != "mix");

            if (js_mat_obj.Has("main_color")) {
                const JsArray &js_main_color = js_mat_obj.at("main_color");
                mat_desc.main_color[0] = (float)((const JsNumber &)js_main_color.at(0)).val;
                mat_desc.main_color[1] = (float)((const JsNumber &)js_main_color.at(1)).val;
                mat_desc.main_color[2] = (float)((const JsNumber &)js_main_color.at(2)).val;
            }

            if (js_mat_obj.Has("normal_map")) {
                const JsString &js_normal_map = js_mat_obj.at("normal_map");
                mat_desc.normal_map = get_texture(js_normal_map.val, false);
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

            if (js_mat_obj.Has("int_ior")) {
                const JsNumber &js_int_ior = js_mat_obj.at("int_ior");
                mat_desc.int_ior = (float)js_int_ior.val;
            }

            if (js_mat_obj.Has("ext_ior")) {
                const JsNumber &js_ext_ior = js_mat_obj.at("ext_ior");
                mat_desc.ext_ior = (float)js_ext_ior.val;
            }

            if (js_type.val == "diffuse") {
                mat_desc.type = Ray::DiffuseMaterial;
            } else if (js_type.val == "glossy") {
                mat_desc.type = Ray::GlossyMaterial;
            } else if (js_type.val == "refractive") {
                mat_desc.type = Ray::RefractiveMaterial;
            } else if (js_type.val == "emissive") {
                mat_desc.type = Ray::EmissiveMaterial;
            } else if (js_type.val == "mix") {
                mat_desc.type = Ray::MixMaterial;

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
                mat_desc.type = Ray::TransparentMaterial;
            } else {
                throw std::runtime_error("unknown material type");
            }

            materials[js_mat_name] = new_scene->AddMaterial(mat_desc);
        }

        const JsObject &js_meshes = js_scene.at("meshes");
        for (const auto &js_mesh : js_meshes.elements) {
            const std::string &js_mesh_name = js_mesh.first;
            const JsObject &js_mesh_obj = js_mesh.second;

            std::vector<float> attrs;
            std::vector<unsigned> indices, groups;

            const JsString &js_vtx_data = js_mesh_obj.at("vertex_data");
            if (js_vtx_data.val.find(".obj") != std::string::npos) {
                std::tie(attrs, indices, groups) = LoadOBJ(js_vtx_data.val);
            } else if (js_vtx_data.val.find(".bin") != std::string::npos) {
                std::tie(attrs, indices, groups) = LoadBIN(js_vtx_data.val);
            } else {
                throw std::runtime_error("unknown mesh type");
            }

            const JsArray &js_materials = js_mesh_obj.at("materials");

            Ray::mesh_desc_t mesh_desc;
            mesh_desc.prim_type = Ray::TriangleList;
            mesh_desc.layout = Ray::PxyzNxyzTuv;
            mesh_desc.vtx_attrs = &attrs[0];
            mesh_desc.vtx_attrs_count = attrs.size() / 8;
            mesh_desc.vtx_indices = &indices[0];
            mesh_desc.vtx_indices_count = indices.size();

            for (size_t i = 0; i < groups.size(); i += 2) {
                const JsString &js_mat_name = js_materials.at(i / 2);
                uint32_t mat_index = materials.at(js_mat_name.val);
                mesh_desc.shapes.push_back({ mat_index, mat_index, groups[i], groups[i + 1] });
            }

            if (js_mesh_obj.Has("allow_spatial_splits")) {
                JsLiteral splits = (JsLiteral)js_mesh_obj.at("allow_spatial_splits");
                mesh_desc.allow_spatial_splits = (splits.val == JS_TRUE);
            }

            if (js_mesh_obj.Has("use_fast_bvh_build")) {
                JsLiteral use_fast = (JsLiteral)js_mesh_obj.at("use_fast_bvh_build");
                mesh_desc.use_fast_bvh_build = (use_fast.val == JS_TRUE);
            }

            meshes[js_mesh_name] = new_scene->AddMesh(mesh_desc);
        }

        const JsArray &js_mesh_instances = js_scene.at("mesh_instances");
        for (const auto &js_mesh_instance : js_mesh_instances.elements) {
            const JsObject &js_mesh_instance_obj = js_mesh_instance;
            const JsString &js_mesh_name = js_mesh_instance_obj.at("mesh");

            Ren::Mat4f transform;

            if (js_mesh_instance_obj.Has("pos")) {
                const JsArray &js_pos = js_mesh_instance_obj.at("pos");

                float px = (float)((const JsNumber &)js_pos.at(0)).val;
                float py = (float)((const JsNumber &)js_pos.at(1)).val;
                float pz = (float)((const JsNumber &)js_pos.at(2)).val;

                transform = Ren::Translate(transform, { px, py, pz });
            }

            if (js_mesh_instance_obj.Has("rot")) {
                const JsArray &js_pos = (const JsArray &)js_mesh_instance_obj.at("rot");

                float rx = (float)((const JsNumber &)js_pos.at(0)).val;
                float ry = (float)((const JsNumber &)js_pos.at(1)).val;
                float rz = (float)((const JsNumber &)js_pos.at(2)).val;

                rx *= Ren::Pi<float>() / 180.0f;
                ry *= Ren::Pi<float>() / 180.0f;
                rz *= Ren::Pi<float>() / 180.0f;

                transform = Ren::Rotate(transform, (float)rz, Ren::Vec3f{ 0.0f, 0.0f, 1.0f });
                transform = Ren::Rotate(transform, (float)rx, Ren::Vec3f{ 1.0f, 0.0f, 0.0f });
                transform = Ren::Rotate(transform, (float)ry, Ren::Vec3f{ 0.0f, 1.0f, 0.0f });
            }

            uint32_t mesh_index = meshes.at(js_mesh_name.val);
            new_scene->AddMeshInstance(mesh_index, Ren::ValuePtr(transform));
        }
    } catch (std::runtime_error &e) {
        LOGE("Error in parsing json file! %s", e.what());
        return nullptr;
    }

    if (view_targeted) {
        Ren::Vec3f dir = view_origin - view_target;
        view_dir = Normalize(-dir);
    }

    Ray::camera_desc_t cam_desc;
    cam_desc.type = Ray::Persp;
    cam_desc.filter = Ray::Tent;
    memcpy(&cam_desc.origin[0], Ren::ValuePtr(view_origin), 3 * sizeof(float));
    memcpy(&cam_desc.fwd[0], Ren::ValuePtr(view_dir), 3 * sizeof(float));
    cam_desc.fov = view_fov;
    cam_desc.gamma = 2.2f;
    cam_desc.focus_distance = 1.0f;
    cam_desc.focus_factor = 0.0f;
    cam_desc.clamp = true;

    new_scene->AddCamera(cam_desc);

    {
        /*ray::light_desc_t l_desc;
        l_desc.type = ray::PointLight;
        l_desc.position[0] = -500;
        l_desc.position[1] = 50;
        l_desc.position[2] = 100;
        l_desc.radius = 5;
        l_desc.color[0] = 50.0f;
        l_desc.color[1] = 50.0f;
        l_desc.color[2] = 50.0f;
        new_scene->AddLight(l_desc);

        l_desc.position[0] = -500;
        l_desc.position[1] = 50;
        l_desc.position[2] = -100;
        new_scene->AddLight(l_desc);

        l_desc.position[0] = 0;
        l_desc.position[1] = 50;
        l_desc.position[2] = 100;
        new_scene->AddLight(l_desc);

        l_desc.position[0] = 0;
        l_desc.position[1] = 50;
        l_desc.position[2] = -100;
        new_scene->AddLight(l_desc);

        l_desc.position[0] = 500;
        l_desc.position[1] = 50;
        l_desc.position[2] = 100;
        new_scene->AddLight(l_desc);

        l_desc.position[0] = 500;
        l_desc.position[1] = 50;
        l_desc.position[2] = -100;
        new_scene->AddLight(l_desc);*/
    }

    {
        /*ray::light_desc_t l_desc;
        l_desc.type = ray::SpotLight;
        l_desc.position[0] = -1000;
        l_desc.position[1] = dist;
        l_desc.position[2] = -100;
        l_desc.radius = 10;
        l_desc.color[0] = d;
        l_desc.color[1] = 50000.0f;
        l_desc.color[2] = 50000.0f;
        l_desc.direction[0] = 0.0f;
        l_desc.direction[1] = -1.0f;
        l_desc.direction[2] = 0.0f;
        l_desc.angle = 90.0f;
        new_scene->AddLight(l_desc);*/
    }

    {
        /*ray::light_desc_t l_desc;
        l_desc.type = ray::DirectionalLight;
        l_desc.direction[0] = 0.707f;
        l_desc.direction[1] = -0.707f;
        l_desc.direction[2] = 0.0f;
        l_desc.angle = 0.1f;
        l_desc.color[0] = 5.75f;
        l_desc.color[1] = 5.75f;
        l_desc.color[2] = 5.75f;
        new_scene->AddLight(l_desc);*/
    }

    /*{
        std::random_device rd;
        std::mt19937 mt{};// (rd());
        std::uniform_real_distribution<float> norm_float_dist(0.0f, 1.0f);

        for (int i = 0; i < 0; i++) {
            Ren::Mat4f transform;

            transform = Ren::Translate(transform, { norm_float_dist(mt) * 200.0f, norm_float_dist(mt) * 200.0f, norm_float_dist(mt) * 200.0f });

            new_scene->AddMeshInstance(0, Ren::ValuePtr(transform));
        }
    }*/

    return new_scene;
}

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
                if (!attrs.empty()) {
                    // avoid bound checks in debug
                    const float *_attrs = &attrs[0];
                    const float *_v = &v[0];
                    const float *_vn = &vn[0];
                    const float *_vt = &vt[0];
                    int last_index = std::max(0, (int)attrs.size() / 8 - 1000);
                    for (int i = (int)attrs.size() / 8 - 1; i >= last_index; i--) {
                        if (_abs(_attrs[i * 8 + 0] - _v[i1 * 3]) < 0.0000001f &&
                            _abs(_attrs[i * 8 + 1] - _v[i1 * 3 + 1]) < 0.0000001f &&
                            _abs(_attrs[i * 8 + 2] - _v[i1 * 3 + 2]) < 0.0000001f &&
                            _abs(_attrs[i * 8 + 3] - _vn[i3 * 3]) < 0.0000001f &&
                            _abs(_attrs[i * 8 + 4] - _vn[i3 * 3 + 1]) < 0.0000001f &&
                            _abs(_attrs[i * 8 + 5] - _vn[i3 * 3 + 2]) < 0.0000001f &&
                            _abs(_attrs[i * 8 + 6] - _vt[i2 * 2]) < 0.0000001f &&
                            _abs(_attrs[i * 8 + 7] - _vt[i2 * 2 + 1]) < 0.0000001f) {
                            indices.push_back(i);
                            found = true;
                            break;
                        }
                    }
                }
#endif

                if (!found) {
                    indices.push_back((uint32_t)(attrs.size() / 8));
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
                groups.push_back((uint32_t)(indices.size() - groups.back()));
            }
            groups.push_back((uint32_t)indices.size());
        }
    }

    if (groups.empty()) {
        groups.push_back(0);
    }

    groups.push_back((uint32_t)(indices.size() - groups.back()));

#if 0
    {
        std::string out_file_name = file_name;
        out_file_name[out_file_name.size() - 3] = 'b';
        out_file_name[out_file_name.size() - 2] = 'i';
        out_file_name[out_file_name.size() - 1] = 'n';

        std::ofstream out_file(out_file_name, std::ios::binary);

        uint32_t s;

        s = (uint32_t)attrs.size();
        out_file.write((char *)&s, sizeof(s));

        s = (uint32_t)indices.size();
        out_file.write((char *)&s, sizeof(s));

        s = (uint32_t)groups.size();
        out_file.write((char *)&s, sizeof(s));

        out_file.write((char *)&attrs[0], attrs.size() * sizeof(attrs[0]));
        out_file.write((char *)&indices[0], indices.size() * sizeof(indices[0]));
        out_file.write((char *)&groups[0], groups.size() * sizeof(groups[0]));
    }
#endif

#if 0
    {
        std::string out_file_name = file_name;
        out_file_name[out_file_name.size() - 3] = 'h';
        out_file_name[out_file_name.size() - 2] = '\0';
        out_file_name[out_file_name.size() - 1] = '\0';

        std::ofstream out_file(out_file_name, std::ios::binary);
        out_file << std::setprecision(16) << std::fixed;

        out_file << "static float attrs[] = {\n\t";
        for (size_t i = 0; i < attrs.size(); i++) {
            out_file << attrs[i] << "f, ";
            if (i % 10 == 0 && i != 0) out_file << "\n\t";
        }
        out_file << "\n};\n";
        out_file << "static size_t attrs_count = " << attrs.size() << ";\n\n";

        out_file << "static uint32_t indices[] = {\n\t";
        for (size_t i = 0; i < indices.size(); i++) {
            out_file << indices[i] << ", ";
            if (i % 10 == 0 && i != 0) out_file << "\n\t";
        }
        out_file << "\n};\n";
        out_file << "static size_t indices_count = " << indices.size() << ";\n\n";

        out_file << "static uint32_t groups[] = {\n\t";
        for (size_t i = 0; i < groups.size(); i++) {
            out_file << groups[i] << ", ";
            if (i % 10 == 0 && i != 0) out_file << "\n\t";
        }
        out_file << "\n};\n";
        out_file << "static size_t groups_count = " << groups.size() << ";\n\n";
    }
#endif

    return std::make_tuple(std::move(attrs), std::move(indices), std::move(groups));
}

std::tuple<std::vector<float>, std::vector<unsigned>, std::vector<unsigned>> LoadBIN(const std::string &file_name) {
    std::ifstream in_file(file_name, std::ios::binary);
    uint32_t num_attrs;
    in_file.read((char *)&num_attrs, 4);
    uint32_t num_indices;
    in_file.read((char *)&num_indices, 4);
    uint32_t num_groups;
    in_file.read((char *)&num_groups, 4);

    std::vector<float> attrs;
    attrs.resize(num_attrs);
    in_file.read((char *)&attrs[0], (size_t)num_attrs * 4);

    std::vector<unsigned> indices;
    indices.resize((size_t)num_indices);
    in_file.read((char *)&indices[0], (size_t)num_indices * 4);
    
    std::vector<unsigned> groups;
    groups.resize((size_t)num_groups);
    in_file.read((char *)&groups[0], (size_t)num_groups * 4);

    return std::make_tuple(std::move(attrs), std::move(indices), std::move(groups));
}

std::vector<Ray::pixel_color8_t> LoadTGA(const std::string &name, int &w, int &h) {
    std::vector<Ray::pixel_color8_t> tex_data;

    {
        std::ifstream in_file(name, std::ios::binary);
        if (!in_file) return {};

        in_file.seekg(0, std::ios::end);
        size_t in_file_size = (size_t)in_file.tellg();
        in_file.seekg(0, std::ios::beg);

        std::vector<char> in_file_data(in_file_size);
        in_file.read(&in_file_data[0], in_file_size);

        Ren::eTexColorFormat format;
        auto pixels = Ren::ReadTGAFile(&in_file_data[0], w, h, format);

        if (format == Ren::RawRGB888) {
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    tex_data.push_back({ pixels[3 * (y * w + x)], pixels[3 * (y * w + x) + 1], pixels[3 * (y * w + x) + 2], 255 });
                }
            }
        } else if (format == Ren::RawRGBA8888) {
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

std::vector<Ray::pixel_color8_t> LoadHDR(const std::string &name, int &out_w, int &out_h) {
    std::ifstream in_file(name, std::ios::binary);

    std::string line;
    if (!std::getline(in_file, line) || line != "#?RADIANCE") {
        throw std::runtime_error("Is not HDR file!");
    }

    float exposure = 1.0f;
    std::string format;

    while (std::getline(in_file, line)) {
        if (line.empty()) break;

        if (!line.compare(0, 6, "FORMAT")) {
            format = line.substr(7);
        } else if (!line.compare(0, 8, "EXPOSURE")) {
            exposure = (float)atof(line.substr(9).c_str());
        }
    }

    if (format != "32-bit_rle_rgbe") {
        throw std::runtime_error("Wrong format!");
    }

    int res_x = 0, res_y = 0;

    std::string resolution;
    if (!std::getline(in_file, resolution)) {
        throw std::runtime_error("Cannot read resolution!");
    }

    {
        std::stringstream ss(resolution);
        std::string tok;

        ss >> tok;
        if (tok != "-Y") {
            throw std::runtime_error("Unsupported format!");
        }

        ss >> tok;
        res_y = atoi(tok.c_str());

        ss >> tok;
        if (tok != "+X") {
            throw std::runtime_error("Unsupported format!");
        }

        ss >> tok;
        res_x = atoi(tok.c_str());
    }

    if (!res_x || !res_y) {
        throw std::runtime_error("Unsupported format!");
    }

    out_w = res_x;
    out_h = res_y;

    std::vector<Ray::pixel_color8_t> data(res_x * res_y * 4);
    int data_offset = 0;

    int scanlines_left = res_y;
    std::vector<uint8_t> scanline(res_x * 4);

    while (scanlines_left) {
        {
            uint8_t rgbe[4];

            if (!in_file.read((char *)&rgbe[0], 4)) {
                throw std::runtime_error("Cannot read file!");
            }

            if ((rgbe[0] != 2) || (rgbe[1] != 2) || ((rgbe[2] & 0x80) != 0)) {
                data[0].r = rgbe[0];
                data[0].g = rgbe[1];
                data[0].b = rgbe[2];
                data[0].a = rgbe[3];

                if (!in_file.read((char *)&data[4], (res_x * scanlines_left - 1) * 4)) {
                    throw std::runtime_error("Cannot read file!");
                }
                return data;
            }

            if ((((rgbe[2] & 0xFF) << 8) | (rgbe[3] & 0xFF)) != res_x) {
                throw std::runtime_error("Wrong scanline width!");
            }
        }

        int index = 0;
        for (int i = 0; i < 4; i++) {
            int index_end = (i + 1) * res_x;
            while (index < index_end) {
                uint8_t buf[2];
                if (!in_file.read((char *)&buf[0], 2)) {
                    throw std::runtime_error("Cannot read file!");
                }

                if (buf[0] > 128) {
                    int count = buf[0] - 128;
                    if ((count == 0) || (count > index_end - index)) {
                        throw std::runtime_error("Wrong data!");
                    }
                    while (count-- > 0) {
                        scanline[index++] = buf[1];
                    }
                } else {
                    int count = buf[0];
                    if ((count == 0) || (count > index_end - index)) {
                        throw std::runtime_error("Wrong data!");
                    }
                    scanline[index++] = buf[1];
                    if (--count > 0) {
                        if (!in_file.read((char *)&scanline[index], count)) {
                            throw std::runtime_error("Cannot read file!");
                        }
                        index += count;
                    }
                }
            }
        }

        for (int i = 0; i < res_x; i++) {
            data[data_offset].r = scanline[i + 0 * res_x];
            data[data_offset].g = scanline[i + 1 * res_x];
            data[data_offset].b = scanline[i + 2 * res_x];
            data[data_offset].a = scanline[i + 3 * res_x];
            data_offset++;
        }

        scanlines_left--;
    }

    return data;
}

std::vector<Ray::pixel_color8_t> Load_stb_image(const std::string &name, int &w, int &h) {
    stbi_set_flip_vertically_on_load(1);
    
    int channels;
    uint8_t *img_data = stbi_load(name.c_str(), &w, &h, &channels, 4);

    std::vector<Ray::pixel_color8_t> tex_data(w * h);
    memcpy(&tex_data[0].r, img_data, w * h * sizeof(Ray::pixel_color8_t));

    stbi_image_free(img_data);

    return tex_data;
}

#undef _abs