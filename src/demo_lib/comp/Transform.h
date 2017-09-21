#pragma once

#include <math/math.hpp>

#include <engine/go/GoComponent.h>
#include <engine/go/GoID.h>

#include "ISerializable.h"

class Transform : public GoComponent,
    public ISerializable {
    math::mat4 matr_;
    math::vec3 bbox_[2], orig_bbox_[2];

    void UpdateBBox();
public:
    //OVERRIDE_NEW(Transform)
    DEF_ID("Transform");

    Transform(const math::vec3 &bbox_min = math::vec3{}, const math::vec3 &bbox_max = math::vec3{}) : matr_(1) {
        bbox_[0] = orig_bbox_[0] = bbox_min;
        bbox_[1] = orig_bbox_[1] = bbox_max;
    }

    const math::vec3 &bbox_min() const {
        return bbox_[0];
    }
    const math::vec3 &bbox_max() const {
        return bbox_[1];
    }

    const math::vec3 &orig_bbox_min() const {
        return orig_bbox_[0];
    }
    const math::vec3 &orig_bbox_max() const {
        return orig_bbox_[1];
    }

    const math::mat4 &matrix() const {
        return matr_;
    }

    math::vec3 pos() const {
        return math::vec3(matr_[3]);
    }
    math::quat rot() const;
    math::vec3 angles() const;

    void SetPos(float x, float y, float z) {
        matr_[3] = math::vec4(x, y, z, 1.0f);
        UpdateBBox();
    }

    void SetPos(const math::vec3 &v) {
        SetPos(v.x, v.y, v.z);
    }

    void SetMatrix(const math::mat4 &mat) {
        matr_ = mat;
        UpdateBBox();
    }

    void SetRot(float w, float x, float y, float z);
    void SetRot(const math::quat &q);

    void SetAngles(float x, float y, float z);
    void SetAngles(const math::vec3 &v);

    void Move(float dx, float dy, float dz);
    void Move(const math::vec3 &v);

    void Rotate(float dx, float dy, float dz);
    void Rotate(const math::vec3 &axis, float angle);

    bool Check(const math::vec3 &bbox_min, const math::vec3 &bbox_max) const;
    bool Check(const Transform &rhs) const;

    bool TraceL(const math::vec3 &orig, const math::vec3 &dir, float *dist) const;

    // ISerializable
    bool Read(const JsObject &js_in) override;
    void Write(JsObject &js_out) override;

    void *operator new(size_t size) {
        return math::aligned_malloc(size, math::alignment_m256);
    }

    void operator delete(void *p) {
        math::aligned_free(p);
    }
};
