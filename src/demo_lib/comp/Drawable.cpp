#include "Drawable.h"

#include <ren/Context.h>
#include <sys/AssetFile.h>
#include <sys/AssetFileIO.h>
#include <sys/Json.h>
#include <sys/Log.h>

#pragma warning(disable : 4996)

Drawable::Drawable(ren::Context &ctx) : ctx_(ctx) {}

bool Drawable::Read(const JsObject &js_in) {
    const std::string MODELS_PATH = "./assets/models/";
    const std::string ANIMS_PATH = "./assets/models/";

    try {
        mesh_name_ = ((const JsString &)js_in.at("mesh_name")).val;

        const JsArray &anims = (const JsArray &)js_in.at("anims");
        for (const auto &anim : anims.elements) {
            anim_names_.push_back(((const JsString &)anim).val);
        }
    } catch (...) {
        LOGE("Error loading player drawable!");
        return false;
    }

    std::string mesh_path = MODELS_PATH + mesh_name_;
    auto anim_paths = anim_names_;
    for (auto &anim : anim_paths) {
        anim = ANIMS_PATH + anim;
    }

    // Ugly loading proc
    sys::LoadAssetComplete(mesh_path.c_str(), [this, mesh_path, anim_paths](void *data, int size) {
        ctx_.ProcessSingleTask([this, mesh_path, anim_paths, data]() {
            using namespace std::placeholders;
            mesh_ = ctx_.LoadMesh(mesh_path.c_str(), data, std::bind(&Drawable::OnMaterialNeeded, this, _1));

            for (const auto &anim : anim_paths) {
                sys::LoadAssetComplete(anim.c_str(), [this, anim](void *data, int size) {
                    ctx_.ProcessSingleTask([this, anim, data]() {
                        ren::AnimSeqRef anim_seq = ctx_.LoadAnimSequence(anim.c_str(), data);
                        mesh_->skel()->AddAnimSequence(anim_seq);
                    });
                }, []() {
                    LOGE("Error loading asset!");
                });
            }
        });
    }, []() {
        LOGE("Error loading asset!");
    });

    return true;
}

/*****************************************************************/

ren::Texture2DRef Drawable::OnTextureNeeded(const char *name) {
    const std::string TEXTURES_PATH = "./assets/textures/";

    ren::eTexLoadStatus status;
    ren::Texture2DRef ret = ctx_.LoadTexture2D(name, nullptr, 0, {}, &status);
    if (!ret->ready()) {
        std::string tex_name = name;
        sys::LoadAssetComplete((TEXTURES_PATH + tex_name).c_str(),
        [this, tex_name](void *data, int size) {

            ctx_.ProcessSingleTask([this, tex_name, data, size]() {
                ren::Texture2DParams p;
                p.filter = ren::Trilinear;
                p.repeat = ren::Repeat;
                ctx_.LoadTexture2D(tex_name.c_str(), data, size, p, nullptr);
                LOGI("Texture %s loaded", tex_name.c_str());
            });
        }, [tex_name]() {
            LOGE("Error loading %s", tex_name.c_str());
        });
    }

    return ret;
}

ren::ProgramRef Drawable::OnProgramNeeded(const char *name, const char *vs_shader, const char *fs_shader) {
#if defined(USE_GL_RENDER)
    ren::eProgLoadStatus status;
    ren::ProgramRef ret = ctx_.LoadProgramGLSL(name, nullptr, nullptr, &status);
    if (!ret->ready()) {
        using namespace std;

        sys::AssetFile vs_file(string("game_assets/shaders/") + vs_shader),
            fs_file(string("game_assets/shaders/") + fs_shader);
        if (!vs_file || !fs_file) {
            LOGE("Error loading program %s", name);
            return ret;
        }

        size_t vs_size = vs_file.size(),
               fs_size = fs_file.size();

        string vs_src, fs_src;
        vs_src.resize(vs_size);
        fs_src.resize(fs_size);
        vs_file.Read((char *)vs_src.data(), vs_size);
        fs_file.Read((char *)fs_src.data(), fs_size);

        ret = ctx_.LoadProgramGLSL(name, vs_src.c_str(), fs_src.c_str(), &status);
        assert(status == ren::ProgCreatedFromData);
    }
    return ret;
#elif defined(USE_SW_RENDER)
    ren::eProgLoadStatus status;
    ren::ProgramRef ret = ctx_.LoadProgramSW(name, nullptr, nullptr, 0, nullptr, nullptr, &status);
    if (!ret->ready()) {
        ren::ProgramRef LoadSWProgram(ren::Context &, const char *);
        ret = LoadSWProgram(ctx_, name);

    }
    return ret;
#endif
}

ren::MaterialRef Drawable::OnMaterialNeeded(const char *name) {
    const std::string MATERIALS_PATH = "./assets/materials/";

    ren::eMatLoadStatus status;
    ren::MaterialRef ret = ctx_.LoadMaterial(name, nullptr, &status, nullptr, nullptr);
    if (!ret->ready()) {
        sys::AssetFile in_file(MATERIALS_PATH + name);
        if (!in_file) {
            LOGE("Error loading material %s", name);
            return ret;
        }

        size_t file_size = in_file.size();

        std::string mat_src;
        mat_src.resize(file_size);
        in_file.Read((char *)mat_src.data(), file_size);

        using namespace std::placeholders;

        ret = ctx_.LoadMaterial(name, mat_src.data(), &status,
                                std::bind(&Drawable::OnProgramNeeded, this, _1, _2, _3),
                                std::bind(&Drawable::OnTextureNeeded, this, _1));
        assert(status == ren::MatCreatedFromData);
    }
    return ret;
}
