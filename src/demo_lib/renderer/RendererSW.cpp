#include "Renderer.h"

#include <ren/Camera.h>
#include <sys/AssetFile.h>
#include <sys/Log.h>
#include <ren/Context.h>
#include <ren/SW/SW.h>

#include "../comp/Drawable.h"
#include "../comp/Transform.h"

namespace RendererSWInternal {
enum { A_POS,
       A_UVS,
       A_NORMAL,
       A_INDICES,
       A_WEIGHTS
     };

enum { V_UVS };

enum { U_MVP,
       U_AMBIENT,
       U_FLASHLIGHT_POS,
       U_MV,
       U_MAT_PALETTE,

       U_COL
     };

enum { DIFFUSEMAP_SLOT };

const int MAT_PALETTE_SIZE = 32;

inline void BindTexture(int slot, uint32_t tex) {
    swActiveTexture(SW_TEXTURE0 + slot);
    swBindTexture((SWint)tex);
}
}

void Renderer::DrawSkeletal(const Transform *tr, Drawable *dr, float dt_s) {
    using namespace RendererSWInternal;
    using namespace math;

    ren::MeshRef ref = dr->mesh();
    if (!ref) return;

    ren::Mesh *m			 = ref.get();
    const ren::Material *mat = m->strip(0).mat.get();
    const ren::Program *p	 = mat->program().get();

    ren::Skeleton *skel = m->skel();
    int cur_anim = 0;
    if (!skel->anims.empty()) {
        skel->UpdateAnim(cur_anim, dt_s, &dr->anim_time);
        skel->ApplyAnim(cur_anim);
        skel->UpdateBones();
    }

    swBindBuffer(SW_ARRAY_BUFFER, m->attribs_buf_id());
    swBindBuffer(SW_INDEX_BUFFER, m->indices_buf_id());

    swUseProgram(p->prog_id());

    swEnable(SW_PERSPECTIVE_CORRECTION);
    swEnable(SW_FAST_PERSPECTIVE_CORRECTION);

    mat4 world_from_object = tr->matrix(),
         view_from_world = make_mat4(current_cam_->view_matrix()),
         proj_from_view = make_mat4(current_cam_->projection_matrix());

    mat4 view_from_object = view_from_world * world_from_object,
         proj_from_object = proj_from_view * view_from_object;

    swSetUniform(U_MV, SW_MAT4, value_ptr(view_from_object));
    swSetUniform(U_MVP, SW_MAT4, value_ptr(proj_from_object));

    size_t num_bones = skel->matr_palette.size();
    swSetUniformv(U_MAT_PALETTE, SW_MAT4, (SWint)num_bones, value_ptr(skel->matr_palette[0]));

    int stride = sizeof(float) * 16;
    swVertexAttribPointer(A_POS, 3 * sizeof(float), (SWuint)stride, (void *)0);
    swVertexAttribPointer(A_UVS, 3 * sizeof(float), (SWuint)stride, (void *)(6 * sizeof(float)));
    swVertexAttribPointer(A_INDICES, 4 * sizeof(float), (SWuint)stride, (void *)(8 * sizeof(float)));
    swVertexAttribPointer(A_WEIGHTS, 4 * sizeof(float), (SWuint)stride, (void *)(12 * sizeof(float)));

    for (const ren::TriStrip *s = &m->strip(0); s->offset != -1; ++s) {
        mat = s->mat.get();
        BindTexture(DIFFUSEMAP_SLOT, mat->texture(0)->tex_id());
        swDrawElements(SW_TRIANGLE_STRIP, (SWuint)s->num_indices, SW_UNSIGNED_SHORT, (void *)uintptr_t(s->offset));
    }
}

void Renderer::DrawLine(const math::vec3 &start, const math::vec3 &end, const math::vec3 &col, bool depth_test) {
    using namespace RendererSWInternal;
    using namespace math;

    if (!depth_test) {
        swDisable(SW_DEPTH_TEST);
    }

    const ren::Program *p = line_program_.get();

    swBindBuffer(SW_ARRAY_BUFFER, 0);
    swBindBuffer(SW_INDEX_BUFFER, 0);

    swUseProgram(p->prog_id());

    mat4 world_from_object,
         view_from_world = make_mat4(current_cam_->view_matrix()),
         proj_from_view = make_mat4(current_cam_->projection_matrix());

    mat4 view_from_object = view_from_world * world_from_object,
         proj_from_object = proj_from_view * view_from_object;

    swSetUniform(U_MVP, SW_MAT4, value_ptr(proj_from_object));

    swSetUniform(U_COL, SW_VEC3, value_ptr(col));

    const vec3 points[] = { start, end };

    swVertexAttribPointer(A_POS, 3 * sizeof(float), 0, value_ptr(points[0]));
    swDrawArrays(SW_LINES, 0, 2);

    if (!depth_test) {
        swEnable(SW_DEPTH_TEST);
    }
}

void Renderer::DrawPoint(const math::vec3 &_p, float size, bool depth_test) {
    using namespace RendererSWInternal;
    using namespace math;

    if (!depth_test) {
        swDisable(SW_DEPTH_TEST);
    }

    const ren::Program *p = line_program_.get();

    swBindBuffer(SW_ARRAY_BUFFER, 0);
    swBindBuffer(SW_INDEX_BUFFER, 0);

    swUseProgram(p->prog_id());

    mat4 world_from_object,
         view_from_world = make_mat4(current_cam_->view_matrix()),
         proj_from_view = make_mat4(current_cam_->projection_matrix());

    mat4 view_from_object = view_from_world * world_from_object,
         proj_from_object = proj_from_view * view_from_object;

    swSetUniform(U_MVP, SW_MAT4, value_ptr(proj_from_object));

    vec3 col = { 1, 0, 0 };

    swSetUniform(U_COL, SW_VEC3, value_ptr(col));

    const vec3 points[] = { _p + vec3{ size, size, 0 },
                            _p + vec3{ -size, -size, 0 },
                            _p + vec3{ -size, size, 0 },
                            _p + vec3{ size, -size, 0 }
                          };

    swVertexAttribPointer(A_POS, 3 * sizeof(float), 0, value_ptr(points[0]));
    swDrawArrays(SW_LINES, 0, 4);

    if (!depth_test) {
        swEnable(SW_DEPTH_TEST);
    }
}

void Renderer::DrawCurve(const math::vec3 *points, int num, const math::vec3 &col, bool depth_test) {
    using namespace RendererSWInternal;
    using namespace math;

    if (!depth_test) {
        swDisable(SW_DEPTH_TEST);
    }

    const ren::Program *p = line_program_.get();

    swBindBuffer(SW_ARRAY_BUFFER, 0);
    swBindBuffer(SW_INDEX_BUFFER, 0);

    swUseProgram(p->prog_id());

    mat4 world_from_object,
         view_from_world = make_mat4(current_cam_->view_matrix()),
         proj_from_view = make_mat4(current_cam_->projection_matrix());

    mat4 view_from_object = view_from_world * world_from_object,
         proj_from_object = proj_from_view * view_from_object;

    swSetUniform(U_MVP, SW_MAT4, value_ptr(proj_from_object));

    swSetUniform(U_COL, SW_VEC3, value_ptr(col));

    swVertexAttribPointer(A_POS, 3 * sizeof(float), 0, value_ptr(points[0]));
    swDrawArrays(SW_CURVE_STRIP, 0, num);

    if (!depth_test) {
        swEnable(SW_DEPTH_TEST);
    }
}

void Renderer::DrawRect(const math::vec2 &_p, const math::vec2 &d, const math::vec3 &col) {
    using namespace RendererSWInternal;
    using namespace math;

    const ren::Program *p = line_program_.get();

    swBindBuffer(SW_ARRAY_BUFFER, 0);
    swBindBuffer(SW_INDEX_BUFFER, 0);

    swUseProgram(p->prog_id());

    mat4 world_from_object,
         view_from_world = make_mat4(current_cam_->view_matrix()),
         proj_from_view = make_mat4(current_cam_->projection_matrix());

    mat4 view_from_object = view_from_world * world_from_object,
         proj_from_object = proj_from_view * view_from_object;

    swSetUniform(U_MVP, SW_MAT4, value_ptr(proj_from_object));

    swSetUniform(U_COL, SW_VEC3, value_ptr(col));

    const vec3 points[] = { { _p, 0 }, { _p.x + d.x, _p.y, 0 }, { _p.x, _p.y + d.y, 0 }, { _p + d, 0 } };

    swVertexAttribPointer(A_POS, 3 * sizeof(float), 0, value_ptr(points[0]));
    swDrawArrays(SW_TRIANGLE_STRIP, 0, 4);
}

void Renderer::DrawMatcap(const Transform *tr, const Drawable *dr) {
    using namespace RendererSWInternal;
    using namespace math;

    ren::MeshRef ref = dr->mesh();
    if (!ref) return;

    ren::Mesh *m			 = ref.get();
    const ren::Material *mat = m->strip(0).mat.get();
    const ren::Program *p	 = mat->program().get();

    swBindBuffer(SW_ARRAY_BUFFER, m->attribs_buf_id());
    swBindBuffer(SW_INDEX_BUFFER, m->indices_buf_id());

    swUseProgram(p->prog_id());

    swEnable(SW_PERSPECTIVE_CORRECTION);
    swEnable(SW_FAST_PERSPECTIVE_CORRECTION);

    mat4 world_from_object,
         view_from_world = make_mat4(current_cam_->view_matrix()),
         proj_from_view = make_mat4(current_cam_->projection_matrix());

    mat4 view_from_object = view_from_world * world_from_object,
         proj_from_object = proj_from_view * view_from_object;

    swSetUniform(U_MV, SW_MAT4, value_ptr(view_from_object));
    swSetUniform(U_MVP, SW_MAT4, value_ptr(proj_from_object));

    int stride = sizeof(float) * 8;
    swVertexAttribPointer(A_POS, 3 * sizeof(float), (SWuint)stride, (void *)0);
    swVertexAttribPointer(A_NORMAL, 3 * sizeof(float), (SWuint)stride, (void *)(3 * sizeof(float)));

    for (const ren::TriStrip *s = &m->strip(0); s->offset != -1; ++s) {
        mat = s->mat.get();
        BindTexture(DIFFUSEMAP_SLOT, mat->texture(0)->tex_id());
        swDrawElements(SW_TRIANGLE_STRIP, (SWuint)s->num_indices, SW_UNSIGNED_SHORT, (void *)uintptr_t(s->offset));
    }
}

void Renderer::DrawSkeletalMatcap(const Transform *tr, const Drawable *dr, float dt_s) {
    using namespace RendererSWInternal;
    using namespace math;

    ren::MeshRef ref = dr->mesh();
    if (!ref) return;

    ren::Mesh *m = ref.get();
    const ren::Material *mat = m->strip(0).mat.get();
    const ren::Program *p = mat->program().get();

    ren::Skeleton *skel = m->skel();
    int cur_anim = 0;
    if (!skel->anims.empty()) {
        skel->UpdateAnim(cur_anim, dt_s, nullptr);
        skel->ApplyAnim(cur_anim);
        skel->UpdateBones();
    }

    swBindBuffer(SW_ARRAY_BUFFER, m->attribs_buf_id());
    swBindBuffer(SW_INDEX_BUFFER, m->indices_buf_id());

    swUseProgram(matcap_skel_program_->prog_id());

    swEnable(SW_PERSPECTIVE_CORRECTION);
    swEnable(SW_FAST_PERSPECTIVE_CORRECTION);

    mat4 world_from_object = tr->matrix(),
         view_from_world = make_mat4(current_cam_->view_matrix()),
         proj_from_view = make_mat4(current_cam_->projection_matrix());

    mat4 view_from_object = view_from_world * world_from_object,
         proj_from_object = proj_from_view * view_from_object;

    swSetUniform(U_MV, SW_MAT4, value_ptr(view_from_object));
    swSetUniform(U_MVP, SW_MAT4, value_ptr(proj_from_object));

    size_t num_bones = skel->matr_palette.size();
    swSetUniformv(U_MAT_PALETTE, SW_MAT4, (SWint)num_bones, value_ptr(skel->matr_palette[0]));

    int stride = sizeof(float) * 16;
    swVertexAttribPointer(A_POS, 3 * sizeof(float), (SWuint)stride, (void *)0);
    swVertexAttribPointer(A_NORMAL, 3 * sizeof(float), (SWuint)stride, (void *)(3 * sizeof(float)));
    swVertexAttribPointer(A_INDICES, 4 * sizeof(float), (SWuint)stride, (void *)(8 * sizeof(float)));
    swVertexAttribPointer(A_WEIGHTS, 4 * sizeof(float), (SWuint)stride, (void *)(12 * sizeof(float)));

    for (const ren::TriStrip *s = &m->strip(0); s->offset != -1; ++s) {
        mat = s->mat.get();
        BindTexture(DIFFUSEMAP_SLOT, mat->texture(0)->tex_id());
        swDrawElements(SW_TRIANGLE_STRIP, (SWuint)s->num_indices, SW_UNSIGNED_SHORT, (void *)uintptr_t(s->offset));
    }
}

void Renderer::ClearColorAndDepth(float r, float g, float b, float a) {
    swClearColor(r, g, b, a);
    swClearDepth(1);
}

extern "C" {
    VSHADER skeletal_vs(VS_IN, VS_OUT) {
        using namespace math;
        using namespace RendererSWInternal;

        mat4 mat = make_mat4(F_UNIFORM(U_MAT_PALETTE) + ((int)V_FATTR(A_INDICES)[0]) * sizeof(mat4));
        const vec4 pos_out = (make_mat4(F_UNIFORM(U_MVP)) * mat) * vec4(make_vec3(V_FATTR(A_POS)), 1);
        V_POS_OUT[0] = pos_out[0];
        V_POS_OUT[1] = pos_out[1];
        V_POS_OUT[2] = pos_out[2];
        V_POS_OUT[3] = pos_out[3];

        const vec2 uvs = make_vec2(V_FATTR(A_UVS));
        V_FVARYING(V_UVS)[0] = uvs[0];
        V_FVARYING(V_UVS)[1] = uvs[1];
    }

    FSHADER skeletal_fs(FS_IN, FS_OUT) {
        using namespace math;
        using namespace RendererSWInternal;

        TEXTURE(DIFFUSEMAP_SLOT, F_FVARYING_IN(V_UVS), F_COL_OUT);
    }

    ///////////////////////////////////////////

    VSHADER line_vs(VS_IN, VS_OUT) {
        using namespace math;
        using namespace RendererSWInternal;

        const vec4 pos_out = make_mat4(F_UNIFORM(U_MVP)) * vec4(make_vec3(V_FATTR(A_POS)), 1);
        V_POS_OUT[0] = pos_out[0];
        V_POS_OUT[1] = pos_out[1];
        V_POS_OUT[2] = pos_out[2];
        V_POS_OUT[3] = pos_out[3];
    }

    FSHADER line_fs(FS_IN, FS_OUT) {
        using namespace math;
        using namespace RendererSWInternal;

        F_COL_OUT[0] = F_UNIFORM(U_COL)[0];
        F_COL_OUT[1] = F_UNIFORM(U_COL)[1];
        F_COL_OUT[2] = F_UNIFORM(U_COL)[2];
        F_COL_OUT[3] = 1;
    }

    ///////////////////////////////////////////

    VSHADER matcap_vs(VS_IN, VS_OUT) {
        using namespace math;
        using namespace RendererSWInternal;

        const vec4 pos_out = make_mat4(F_UNIFORM(U_MVP)) * vec4(make_vec3(V_FATTR(A_POS)), 1);
        V_POS_OUT[0] = pos_out[0];
        V_POS_OUT[1] = pos_out[1];
        V_POS_OUT[2] = pos_out[2];
        V_POS_OUT[3] = pos_out[3];

        vec3 n = vec3(make_mat4(F_UNIFORM(U_MV)) * vec4(make_vec3(V_FATTR(A_NORMAL)), 0));

        const vec2 uvs = vec2(n) * 0.5f + vec2(0.5f, 0.5f);
        V_FVARYING(V_UVS)[0] = uvs[0];
        V_FVARYING(V_UVS)[1] = uvs[1];
    }

    FSHADER matcap_fs(FS_IN, FS_OUT) {
        using namespace math;
        using namespace RendererSWInternal;

        TEXTURE(DIFFUSEMAP_SLOT, F_FVARYING_IN(V_UVS), F_COL_OUT);
    }

    ///////////////////////////////////////////

    VSHADER matcap_skel_vs(VS_IN, VS_OUT) {
        using namespace math;
        using namespace RendererSWInternal;

        mat4 mat = make_mat4(F_UNIFORM(U_MAT_PALETTE) + ((int)V_FATTR(A_INDICES)[0]) * 16);
        const vec4 pos_out = (make_mat4(F_UNIFORM(U_MVP)) * mat) * vec4(make_vec3(V_FATTR(A_POS)), 1);
        V_POS_OUT[0] = pos_out[0];
        V_POS_OUT[1] = pos_out[1];
        V_POS_OUT[2] = pos_out[2];
        V_POS_OUT[3] = pos_out[3];

        vec3 n = vec3(make_mat4(F_UNIFORM(U_MV)) * mat * vec4(make_vec3(V_FATTR(A_NORMAL)), 0));
        const vec2 uvs = vec2(n) * 0.5f + vec2(0.5f, 0.5f);
        V_FVARYING(V_UVS)[0] = uvs[0];
        V_FVARYING(V_UVS)[1] = uvs[1];
    }

    FSHADER matcap_skel_fs(FS_IN, FS_OUT) {
        using namespace math;
        using namespace RendererSWInternal;

        TEXTURE(DIFFUSEMAP_SLOT, F_FVARYING_IN(V_UVS), F_COL_OUT);
    }
}

void Renderer::Init() {
    using namespace RendererSWInternal;

    {
        ren::Attribute attribs[] = { { "pos", A_POS, SW_VEC3, 1 }, {} };
        ren::Uniform unifs[] = { { "mvp", U_MVP, SW_MAT4, 1 }, { "col", U_COL, SW_VEC3, 1 }, {} };
        line_program_ = ctx_.LoadProgramSW("line", (void *)line_vs, (void *)line_fs, 0, attribs, unifs, nullptr);
    }

    {
        ren::Uniform attribs[] = { { "pos", A_POS, SW_VEC3, 1 }, { "n", A_NORMAL, SW_VEC3, 1 }, {} };
        ren::Attribute unifs[] = { { "mvp", U_MVP, SW_MAT4, 1 }, { "mvm", U_MV, SW_MAT4, 1 }, {} };
        matcap_program_ = ctx_.LoadProgramSW("matcap", (void *)matcap_vs, (void *)matcap_fs, 2, attribs, unifs, nullptr);
    }

    {
        ren::Uniform attribs[] = { { "pos", A_POS, SW_VEC3, 1 }, { "n", A_NORMAL, SW_VEC3, 1 }, {} };
        ren::Attribute unifs[] = { { "mvp", U_MVP, SW_MAT4, 1 }, { "mvm", U_MV, SW_MAT4, 1 }, { "mat_palette", U_MAT_PALETTE, SW_MAT4, MAT_PALETTE_SIZE }, {} };
        matcap_skel_program_ = ctx_.LoadProgramSW("matcap_skel", (void *)matcap_skel_vs, (void *)matcap_skel_fs, 2, attribs, unifs, nullptr);
    }
}

// global
ren::ProgramRef LoadSWProgram(ren::Context &ctx, const char *name) {
    using namespace RendererSWInternal;

    assert(false);

    return {};
}