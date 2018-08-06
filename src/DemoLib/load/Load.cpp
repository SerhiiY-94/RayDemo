#include "Load.h"

#include <cassert>

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

std::shared_ptr<ray::SceneBase> LoadScene(ray::RendererBase *r, const JsObject &js_scene) {
    auto new_scene = r->CreateScene();

    Ren::Vec3f view_origin, view_dir = { 0, 0, -1 };

    std::map<std::string, uint32_t> textures;
    std::map<std::string, uint32_t> materials;
    std::map<std::string, uint32_t> meshes;

    auto get_texture = [&](const std::string &name, bool gen_mipmaps) {
        auto it = textures.find(name);
        if (it == textures.end()) {
            int w, h;
            auto data = LoadTGA(name, w, h);
            if (data.empty()) throw std::runtime_error("error loading texture");

            ray::tex_desc_t tex_desc;
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
                std::tie(attrs, indices, groups) = LoadRAW(js_vtx_data.val);
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

            if (js_mesh_obj.Has("allow_spatial_splits")) {
                JsLiteral splits = (JsLiteral)js_mesh_obj.at("allow_spatial_splits");
                mesh_desc.allow_spatial_splits = (splits.val == JS_TRUE);
            }

            meshes[js_mesh_name.val] = new_scene->AddMesh(mesh_desc);
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

            uint32_t mesh_index = meshes.at(js_mesh_name.val);
            new_scene->AddMeshInstance(mesh_index, Ren::ValuePtr(transform));
        }
    } catch (std::runtime_error &e) {
        LOGE("Error in parsing json file! %s", e.what());
        return nullptr;
    }

    new_scene->AddCamera(ray::Persp, Ren::ValuePtr(view_origin), Ren::ValuePtr(view_dir), 45.0f, 2.2f);

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

#if 1
    {
        std::string out_file_name = file_name;
        out_file_name[out_file_name.size() - 3] = 'r';
        out_file_name[out_file_name.size() - 2] = 'a';
        out_file_name[out_file_name.size() - 1] = 'w';

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

#if 1
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

std::tuple<std::vector<float>, std::vector<unsigned>, std::vector<unsigned>> LoadRAW(const std::string &file_name) {
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

        Ren::eTex2DFormat format;
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