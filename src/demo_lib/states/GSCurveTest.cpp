#include "GSCurveTest.h"

#include <fstream>

#if defined(USE_SW_RENDER)
#include <ren/SW/SW.h>
#endif

#include <engine/GameStateManager.h>
#include <ren/Context.h>
#include <sys/Json.h>
#include <sys/Log.h>
#include <ui/Renderer.h>

#include "../Viewer.h"
#include "../comp/Drawable.h"
#include "../comp/Transform.h"
#include "../renderer/Renderer.h"

namespace GSCurveTestInternal {
const float CAM_CENTER[] = { 0, 0, -1 },
                           CAM_TARGET[] = { 0, 0, 0 },
                                   CAM_UP[] = { 0, 1, 0 };

math::vec3 EvalBezier(const std::array<math::vec3, 4> &p, float t) {
    return (1 - t) * (1 - t) * (1 - t) * p[0] + 3 * (1 - t) * (1 - t) * t * p[1] + 3 * (1 - t) * t * t * p[2] + t * t * t * p[3];
}

math::vec3 EvalBezierDerivative(const std::array<math::vec3, 4> &p, float t) {
    return 3 * (1 - t) * (1 - t) * (p[1] - p[0]) + 6 * (1 - t) * t * (p[2] - p[1]) + 3 * t * t * (p[3] - p[2]);
}

math::vec3 EvalBezierSecondDerivative(const std::array<math::vec3, 4> &p, float t) {
    return 6 * (1 - t) * (p[2] - 2 * p[1] + p[0]) + 6 * t * (p[3] - 2 * p[2] + p[1]);
}

void ChordLengthParameterize(const math::vec3 *p, int num, float *p_out) {
    p_out[0] = 0;

    for (int i = 1; i < num; i++) {
        p_out[i] = p_out[i - 1] + math::distance(p[i], p[i - 1]);
    }

    float inv_last = 1.0f / p_out[num - 1];

    for (int i = 0; i < num; i++) {
        p_out[i] *= inv_last;
    }
}

std::array<math::vec3, 4> GenerateBezier(const math::vec3 *p, int num, const float *params, const math::vec3 &l_tan, const math::vec3 &r_tan) {
    std::array<math::vec3, 4> ret = { p[0], {}, {}, p[num - 1] };

    math::aligned_vector<std::array<math::vec3, 2>, math::vec3::alignment> A(num);
    for (int i = 0; i < num; i++) {
        A[i][0] = l_tan * 3 * (1 - params[i]) * (1 - params[i]) * params[i];
        A[i][1] = r_tan * 3 * (1 - params[i]) * params[i] * params[i];
    }

    float C[2][2] = { 0, 0, 0, 0 };
    float X[2] = { 0, 0 };

    for (int i = 0; i < num; i++) {
        C[0][0] += math::dot(A[i][0], A[i][0]);
        C[0][1] += math::dot(A[i][0], A[i][1]);
        C[1][0] += math::dot(A[i][0], A[i][1]);
        C[1][1] += math::dot(A[i][1], A[i][1]);

        math::vec3 tmp = p[i] - EvalBezier( { p[0], p[0], p[num - 1], p[num - 1] }, params[i]);

        X[0] += math::dot(A[i][0], tmp);
        X[1] += math::dot(A[i][1], tmp);
    }

    float det_C0_C1 = C[0][0] * C[1][1] - C[1][0] * C[0][1];
    float det_C0_X = C[0][0] * X[1] - C[1][0] * X[0];
    float det_X_C1 = X[0] * C[1][1] - X[1] * C[0][1];

    float alpha_l = 0, alpha_r = 0;

    if (math::abs(det_C0_C1) > 0.0001f) {
        alpha_l = det_X_C1 / det_C0_C1;
        alpha_r = det_C0_X / det_C0_C1;
    }

    float seg_len = math::distance(p[0], p[num - 1]);
    float eps = 0.000001f * seg_len;

    if (alpha_l < eps || alpha_r < eps) {
        ret[1] = ret[0] + l_tan * (seg_len / 3);
        ret[2] = ret[3] + r_tan * (seg_len / 3);
    } else {
        ret[1] = ret[0] + l_tan * alpha_l;
        ret[2] = ret[3] + r_tan * alpha_r;
    }

    return ret;
}

std::pair<float, int> ComputeMaxError(const math::vec3 *p, int num, const std::array<math::vec3, 4> &bez, const float *params) {
    float max_dist = 0;
    int split_point = num / 2;

    for (int i = 0; i < num; i++) {
        float dist = math::distance2(EvalBezier(bez, params[i]), p[i]);
        if (dist > max_dist) {
            max_dist = dist;
            split_point = i;
        }
    }

    return { max_dist, split_point };
}

float NewtonRaphsonRootFind(const std::array<math::vec3, 4> &bez, const math::vec3 &p, float t) {
    math::vec3 d = EvalBezier(bez, t) - p;
    math::vec3 der = EvalBezierDerivative(bez, t);
    math::vec3 v1 = d * der;
    math::vec3 v2 = der * der + d * EvalBezierSecondDerivative(bez, t);

    float numerator = v1[0] + v1[1] + v1[2];
    float denominator = v2[0] + v2[1] + v2[2];

    if (math::abs(denominator) < 0.00001f) {
        return t;
    } else {
        return t - numerator / denominator;
    }
}

void Reparametrize(const std::array<math::vec3, 4> &bez, const math::vec3 *p, int num, const float *params, float *out_params) {
    for (int i = 0; i < num; i++) {
        out_params[i] = NewtonRaphsonRootFind(bez, p[i], params[i]);
    }
}

std::vector<std::array<math::vec3, 4>> FitCubic(const math::vec3 *p, int num, const math::vec3 &l_tan, const math::vec3 &r_tan, float max_error) {
    if (num == 2) {
        float dist = math::distance(p[0], p[1]) / 3;
        return { { p[0], p[0] + l_tan * dist, p[1] + r_tan * dist, p[1] } };
    }

    std::unique_ptr<float[]> buf1(new float[num]), buf2(new float[num]);
    float *u = &buf1[0], *u_prime = &buf2[0];

    ChordLengthParameterize(p, num, u);
    std::array<math::vec3, 4> bez = GenerateBezier(p, num, u, l_tan, r_tan);

    float error;
    int split_point;
    std::tie(error, split_point) = ComputeMaxError(p, num, bez, u);
    if (error < max_error) {
        return { bez };
    }

    if (error < max_error * max_error) {
        for (int i = 0; i < 20; i++) {
            Reparametrize(bez, p, num, u, u_prime);
            bez = GenerateBezier(p, num, u_prime, l_tan, r_tan);
            std::tie(error, split_point) = ComputeMaxError(p, num, bez, u_prime);
            if (error < max_error) {
                return { bez };
            }
            std::swap(u, u_prime);
        }
    }

    math::vec3 c_tan = math::normalize(p[split_point - 1] - p[split_point + 1]);

    auto bez1 = FitCubic(p, split_point + 1, l_tan, c_tan, max_error);
    auto bez2 = FitCubic(p + split_point, num - split_point, -c_tan, r_tan, max_error);

    bez1.insert(bez1.end(), bez2.begin(), bez2.end());

    return bez1;
}

std::vector<std::array<math::vec3, 4>> FitCurve(const math::vec3 *p, int num, float max_error) {
    if (num < 2) return {};

    math::vec3 l_tan = math::normalize(p[1] - p[0]);
    math::vec3 r_tan = math::normalize(p[num - 2] - p[num - 1]);

    return FitCubic(p, num, l_tan, r_tan, max_error);
}
}

GSCurveTest::GSCurveTest(GameBase *game) : game_(game),
    cam_(GSCurveTestInternal::CAM_CENTER,
         GSCurveTestInternal::CAM_TARGET,
         GSCurveTestInternal::CAM_UP) {
    state_manager_	= game->GetComponent<GameStateManager>(STATE_MANAGER_KEY);
    ctx_			= game->GetComponent<ren::Context>(REN_CONTEXT_KEY);
    renderer_		= game->GetComponent<Renderer>(RENDERER_KEY);

    //cam_.Perspective(8, float(game_->width) / game_->height, 0.1f, 100.0f);
    cam_.Orthographic(-10, 10, -10, 10, 0.1f, 100.0f);
}

GSCurveTest::~GSCurveTest() {

}

void GSCurveTest::Enter() {
#if defined(USE_SW_RENDER)
    swEnable(SW_FAST_PERSPECTIVE_CORRECTION);
    swEnable(SW_DEPTH_TEST);

    swSetFloat(SW_CURVE_TOLERANCE, 1.0f);
#endif

    for (int i = 0; i < 255; i++) {
        color_table_[i] = { (rand() % 255) / 255.0f, (rand() % 255) / 255.0f, (rand() % 255) / 255.0f };
    }
}

void GSCurveTest::Exit() {

}

void GSCurveTest::Draw(float dt_s) {
    using namespace math;

    renderer_->set_current_cam(&cam_);
    renderer_->ClearColorAndDepth(0, 0, 0, 1);

    /*vec3 curve_pos[] = { { 0.75f, -0.5f, 0 },
    					 { 0.25f, -0.25f, 0 },
    					 { 1.0f, 0, 0 },
    				     { 0.5f, 0.25f, 0 },
    				     { 0.25f, 0.5f, 0 },
    				     { 0.5f, 0.75f, 0 },
    					 { 0.75f, 0.75f, 0 } };

    for (int i = 0; i < 7; i++) {
    	curve_pos[i] *= 100.0f;
    }

    renderer_->DrawCurve(&curve_pos[0], 7);*/

    //for (const auto &p : points_) {
    if (!points_.empty())
        for (int i = 0; i < (int)points_.size() - 1; i++) {
            renderer_->DrawPoint(points_[i], 1.0f);
            //renderer_->DrawLine(p[i], p[i + 1]);
        }
    //}

    if (!points_.empty()) {
        auto bez = GSCurveTestInternal::FitCurve(&points_[0], (int)points_.size(), 1.0f);

        if (finalize_curve_) {
            points_.clear();

            if (!curve_.empty()) {
                float d = math::distance(curve_.back()[3], bez.front()[0]);

                if (d < 1) {
                    math::vec3 v = curve_.back()[3] - curve_.back()[2];
                    float d = math::distance(bez.front()[1], bez.front()[0]);
                    bez.front()[1] = bez.front()[0] + math::normalize(v) * d;
                }
            }

            curve_.insert(curve_.end(), bez.begin(), bez.end());
            bez.clear();

            finalize_curve_ = false;
        } else if (bez.size() > 2) {
            float dist2 = std::numeric_limits<float>::max();
            size_t index = 0;
            for (size_t i = 0; i < points_.size(); i++) {
                float d2 = math::distance2(points_[i], bez.front()[3]);
                if (d2 < dist2) {
                    dist2 = d2;
                    index = i;
                }
            }
            points_.erase(points_.begin(), points_.begin() + index);

            if (!curve_.empty()) {
                float d = math::distance(curve_.back()[3], bez.front()[0]);

                if (d < 1) {
                    math::vec3 v = curve_.back()[3] - curve_.back()[2];
                    float d = math::distance(bez.front()[1], bez.front()[0]);
                    bez.front()[1] = bez.front()[0] + math::normalize(v) * d;
                }
            }

            curve_.insert(curve_.end(), bez.begin(), bez.begin() + 1);
            bez.erase(bez.begin());
        }

        for (const auto &b : bez) {
            renderer_->DrawCurve(&b[0], 4, vec3{ 1, 1, 1 });
        }
    }

    int counter = 0;

    if (!curve_.empty()) {
        /*math::vec3 der = GSCurveTestInternal::EvalBezierDerivative(curve_.front(), 0);
        der = { der.y, -der.x, der.z };

        der = math::normalize(der);

        float alpha = math::radians(15.0f);
        float ca = math::cos(alpha), sa = math::sin(alpha);

        math::vec3 p1 = curve_.front()[0] - 4 * der;
        renderer_->DrawLine(curve_.front()[0], p1);

        math::vec3 p3 = p1 - 1 * der;

        for (int i = 0; i < 12; i++) {
        	der = { ca * der.x - sa * der.y, sa * der.x + ca * der.y, der.z };

        	math::vec3 p2 = curve_.front()[0] - 4 * der;
        	renderer_->DrawLine(curve_.front()[0], p2);

        	renderer_->DrawLine(p1, p2);
        	p1 = p2;

        	math::vec3 p4 = p2 - 1 * der;

        	renderer_->DrawLine(p1, p4);

        	renderer_->DrawLine(p3, p4);
        	p3 = p4;
        }*/
    }

    for (const auto &b : curve_) {
        //renderer_->DrawCurve(&b[0], 4, color_table_[(counter++) % 255]);

        current_col_ = color_table_[(counter++) % 255];

        /*renderer_->DrawPoint(b[0], 1.0f);
        renderer_->DrawPoint(b[1], 1.0f);
        renderer_->DrawPoint(b[2], 1.0f);
        renderer_->DrawPoint(b[3], 1.0f);*/

        ProcessCurveRec(b[0], b[1], b[2], b[3]);
    }

    if (!points_.empty()) {
        LOGI("%i %i\n", (int)(points_.size()), (int)(curve_.size()));
    }

    ctx_->ProcessTasks();
}

void GSCurveTest::Update(int dt_ms) {

}

void GSCurveTest::HandleInput(InputManager::Event evt) {
    switch (evt.type) {
    case InputManager::RAW_INPUT_P1_DOWN: {
        int w = game_->width, h = game_->height;

        math::vec3 p = { 100.0f - 200.0f * (evt.point.x / w), 100.0f - 200.0f * (evt.point.y / h), 0 };
        points_.push_back(p);

        grabbed_ = true;
    }
    break;
    case InputManager::RAW_INPUT_P1_UP: {
        finalize_curve_ = true;
        grabbed_ = false;
    }
    break;
    case InputManager::RAW_INPUT_P1_MOVE: {
        int w = game_->width, h = game_->height;

        math::vec3 p = { 100.0f - 200.0f * (evt.point.x / w), 100.0f - 200.0f * (evt.point.y / h), 0 };

        if (grabbed_ && !points_.empty() && math::distance(p, points_.back()) > 2.5f) {

            bool should_add = true;

            if (points_.size() >= 2) {
                math::vec3 v1 = points_.back() - points_[points_.size() - 2];
                math::vec3 v2 = p - points_.back();

                v1 = math::normalize(v1);

                float l = math::length(v2);
                v2 /= l;

                float k = math::dot(v1, v2);

                if (k < 0 || math::acos(k) * l < 0.05f) {
                    should_add = false;
                }
            }

            if (should_add) {
                points_.push_back(p);
            } else {
                points_.back() = p;
            }
        }

    }
    break;
    case InputManager::RAW_INPUT_KEY_UP: {

    } break;
    case InputManager::RAW_INPUT_RESIZE:
        //cam_.Perspective(8, float(game_->width) / game_->height, 0.1f, 100.0f);
        break;
    default:
        break;
    }
}

void GSCurveTest::ProcessCurveRec(const math::vec3 &p1, const math::vec3 &p2, const math::vec3 &p3, const math::vec3 &p4) {
    using namespace math;
    using namespace GSCurveTestInternal;

    const float tolerance = 0.001f;

    vec3 p12 = (p1 + p2) * 0.5f;
    vec3 p23 = (p2 + p3) * 0.5f;
    vec3 p34 = (p3 + p4) * 0.5f;
    vec3 p123 = (p12 + p23) * 0.5f;
    vec3 p234 = (p23 + p34) * 0.5f;
    vec3 p1234 = (p123 + p234) * 0.5f;

    float dx = p4.x - p1.x,
          dy = p4.y - p1.y;

    float d2 = math::abs((p2.x - p4.x) * dy - (p2.y - p4.y) * dx),
          d3 = math::abs((p3.x - p4.x) * dy - (p3.y - p4.y) * dx);

    if ((d2 + d3) * (d2 + d3) < tolerance * (dx * dx + dy * dy)) {
        vec3 v = p4 - p1;
        vec3 pp = { v.y, -v.x, v.z };
        pp = math::normalize(pp);

        vec3 pp1 = p2 - p1,
             pp2 = p4 - p3;

        pp1 = { pp1.y, -pp1.x, pp1.z };
        pp1 = math::normalize(pp1);

        pp2 = { pp2.y, -pp2.x, pp2.z };
        pp2 = math::normalize(pp2);

        vec3 p1_1 = p1 + 4 * pp1;
        vec3 p1_2 = p1 - 4 * pp1;
        vec3 p4_1 = p4 + 4 * pp2;
        vec3 p4_2 = p4 - 4 * pp2;

        renderer_->DrawLine(p1_1, p4_1, current_col_);
        renderer_->DrawLine(p1_2, p4_2, current_col_);

        renderer_->DrawLine(p1_1, p1_2, current_col_);
        renderer_->DrawLine(p1_1, p4_2, current_col_);
        renderer_->DrawLine(p4_1, p4_2, current_col_);
    } else {
        ProcessCurveRec(p1, p12, p123, p1234);
        ProcessCurveRec(p1234, p234, p34, p4);
    }
}