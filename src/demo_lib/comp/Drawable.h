#pragma once

#include <engine/go/GoComponent.h>
#include <ren/Mesh.h>

#include "ISerializable.h"

class Drawable : public GoComponent, public ISerializable {
    ren::Context &ctx_;
    ren::MeshRef mesh_;

    std::string mesh_name_;
    std::vector<std::string> anim_names_;

    ren::Texture2DRef OnTextureNeeded(const char *name);
    ren::MaterialRef OnMaterialNeeded(const char *name);
    ren::ProgramRef OnProgramNeeded(const char *name, const char *vs_shader, const char *fs_shader);
public:
    //OVERRIDE_NEW(Drawable)
    DEF_ID("Drawable")

    explicit Drawable(ren::Context &ctx);

    const ren::MeshRef &mesh() const {
        return mesh_;
    }

    // ISerializable
    bool Read(const JsObject &js_in) override;
    void Write(JsObject &js_out) override {}

    float anim_time = 0;
};