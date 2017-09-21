#pragma once

#include <math/math.hpp>

#include <ren/Program.h>
#include <ren/Texture.h>

struct JsObject;
class Drawable;
class PlayerDrawable;
class Transform;

namespace ren {
class Camera;
}

class Renderer {
    ren::Context &ctx_;
    ren::ProgramRef line_program_, matcap_program_, matcap_skel_program_;

    const ren::Camera *current_cam_;

    void Init();
public:
    Renderer(ren::Context &ctx, const JsObject &config);

    void set_current_cam(const ren::Camera *cam) {
        current_cam_ = cam;
    }

    void Temp();

    void DrawSkeletal(const Transform *tr, Drawable *dr, float dt_s);
    void DrawLine(const math::vec3 &start, const math::vec3 &end, const math::vec3 &col, bool depth_test = true);
    void DrawPoint(const math::vec3 &p, float size, bool depth_test = true);
    void DrawCurve(const math::vec3 *p, int num, const math::vec3 &col, bool depth_test = true);
    void DrawRect(const math::vec2 &p, const math::vec2 &d, const math::vec3 &col);
    void DrawMatcap(const Transform *tr, const Drawable *dr);
    void DrawSkeletalMatcap(const Transform *tr, const Drawable *dr, float dt_s);

    void ClearColorAndDepth(float r, float g, float b, float a);
};

