#include "GSColorsTest.h"

#include <fstream>

#if defined(USE_SW_RENDER)
#include <ren/SW/SW.h>
#endif

#include <engine/GameStateManager.h>
#include <ren/Context.h>
#include <sys/Json.h>
#include <sys/Log.h>
#include <sys/Time_.h>
#include <ui/Renderer.h>

#include "../Viewer.h"
#include "../comp/Drawable.h"
#include "../comp/Transform.h"
#include "../renderer/Renderer.h"

namespace GSColorsTestInternal {
const float CAM_CENTER[] = { 0, 0, -1 },
                           CAM_TARGET[] = { 0, 0, 0 },
                                   CAM_UP[] = { 0, 1, 0 };

struct RGB {
    float r;
    float g;
    float b;
    float a;
};

struct HSV {
    float hue;
    float saturation;
    float brightness;
    float a;
};

struct XYZ {
    float x, y, z, a;
};

struct CIE {
    float l, a, b, m_a;
};

RGB hsv_to_rgb(const HSV &hsv) {
    float r, g, b;

    if (hsv.saturation < 0.001f) {
        r = hsv.brightness;
        g = hsv.brightness;
        b = hsv.brightness;
    } else {
        float h = hsv.hue * (6.0f / (float)(2.0f * math::pi<float>()));

        int i = (int)h;
        float _1 = hsv.brightness * (1 - hsv.saturation);
        float _2 = hsv.brightness * (1 - hsv.saturation * (h - i));
        float _3 = hsv.brightness * (1 - hsv.saturation * (1 - (h - i)));

        if (i == 0 || i == 6) {
            r = hsv.brightness;
            g = _3;
            b = _1;
        } else if (i == 1) {
            r = _2;
            g = hsv.brightness;
            b = _1;
        } else if (i == 2) {
            r = _1;
            g = hsv.brightness;
            b = _3;
        } else if (i == 3) {
            r = _1;
            g = _2;
            b = hsv.brightness;
        } else if (i == 4) {
            r = _3;
            g = _1;
            b = hsv.brightness;
        } else { //i == 5
            r = hsv.brightness;
            g = _1;
            b = _2;
        };
    }

    return { r, g, b, hsv.a };
}

RGB xyz_to_rgb(const XYZ &xyz) {
    RGB rgb;
    rgb.r = (float)(xyz.x * 3.24071 + xyz.y * (-1.53726) + xyz.z * (-0.498571));
    rgb.g = (float)(xyz.x * (-0.969258) + xyz.y * 1.87599 + xyz.z * 0.0415557);
    rgb.b = (float)(xyz.x * 0.0556352 + xyz.y * (-0.203996) + xyz.z * 1.05707);

    /*rgb.r = (float)(xyz.x * 2.3706743 + xyz.y * (-0.9000405) + xyz.z * (-0.4706338));
    rgb.g = (float)(xyz.x * (-0.5138850) + xyz.y * 1.4253036 + xyz.z * 0.0885814);
    rgb.b = (float)(xyz.x * 0.0052982 + xyz.y * (-0.0146949) + xyz.z * 1.0093968);*/

    return rgb;
}

XYZ rgb_to_xyz(const RGB &rgb) {
    XYZ xyz;
    xyz.x = (float)(0.412424 * rgb.r + 0.357579 * rgb.g + 0.180464 * rgb.b);
    xyz.y = (float)(0.212656 * rgb.r + 0.715158 * rgb.g + 0.0721856 * rgb.b);
    xyz.z = (float)(0.0193324 * rgb.r + 0.119193 * rgb.g + 0.950444 * rgb.b);
    xyz.a = 1;
    return xyz;
}

XYZ cie_to_xyz(const CIE &cie) {
    double t0, t1, t2;

    t1 = cie.l * (1.0 / 116.0) + 16.0 / 116.0;
    t0 = cie.a * (1.0 / 500.0) + t1;
    t2 = cie.b * (-1.0 / 200.0) + t1;

    t0 = (t0 > 6.0 / 29.0) ? pow(t0, 3) : t0 * (108.0 / 841.0) - 432.0 / 24389.0;
    t1 = (cie.l > 8.0) ? pow(t1, 3) : cie.l * (27.0 / 24389.0);
    t2 = (t2 > 6.0 / 29.0) ? pow(t2, 3) : t2 * (108.0 / 841.0) - 432.0 / 24389.0;

    XYZ xyz;
    xyz.x = (float)(t0 * 0.950467);
    xyz.y = (float)(t1 * 1.00000);
    xyz.z = (float)(t2 * 1.088969);

    return xyz;
}
}

GSColorsTest::GSColorsTest(GameBase *game) : game_(game),
    cam_(GSColorsTestInternal::CAM_CENTER,
         GSColorsTestInternal::CAM_TARGET,
         GSColorsTestInternal::CAM_UP) {
    state_manager_	= game->GetComponent<GameStateManager>(STATE_MANAGER_KEY);
    ctx_			= game->GetComponent<ren::Context>(REN_CONTEXT_KEY);
    renderer_		= game->GetComponent<Renderer>(RENDERER_KEY);

    //cam_.Perspective(8, float(game_->width) / game_->height, 0.1f, 100.0f);
    cam_.Orthographic(1, -1, -1, 1, 0.1f, 10.0f);
}

GSColorsTest::~GSColorsTest() {

}

void GSColorsTest::Enter() {
#if defined(USE_SW_RENDER)
    swEnable(SW_FAST_PERSPECTIVE_CORRECTION);
    swEnable(SW_DEPTH_TEST);

    swSetFloat(SW_CURVE_TOLERANCE, 1.0f);
#endif


}

void GSColorsTest::Exit() {

}

void GSColorsTest::Draw(float dt_s) {
    using namespace GSColorsTestInternal;
    using namespace math;

    int w = game_->width;
    int h = game_->height;

    renderer_->set_current_cam(&cam_);
    renderer_->ClearColorAndDepth(0, 0, 0, 1);

    auto map_hue_sat = [](int x, int y, int w, int h) -> std::pair<float, float> {
        vec2 v = { float(2 * x - w) / w, float(2 * y - h) / h };
        float l = length(v);

        if (l == 0) return { 0.0f, 0.0f };

        if (l > 1) return { NAN, NAN };
        v /= l;

        float angle = math::acos(dot(v, { 1, 0 }));

        if (v.y < 0) {
            angle = 2 * math::pi<float>() - angle;
        }

        return { angle, l };
    };

    std::vector<HSV> colors_hsv(w * h);

    /*for (int y = 0; y < h; y++) {
    	for (int x = 0; x < w; x++) {
    		float hue, sat;
    		std::tie(hue, sat) = map_hue_sat(x, y, w, h);

    		if (std::isnan(hue)) continue;

    		int _y = h - 1 - y;

    		colors_hsv[_y * w + x].hue = hue;
    		colors_hsv[_y * w + x].saturation = sat;
    		colors_hsv[_y * w + x].brightness = 1;
    		colors_hsv[_y * w + x].a = 1;
    	}
    }*/

    /*for (int y = 0; y < h; y++) {
    	for (int x = 0; x < w; x++) {
    		float hue = (float(x) / w) * 2 * pi<float>();
    		float sat = float(y) / h;

    		int _y = h - 1 - y;

    		colors_hsv[_y * w + x].hue = hue;
    		colors_hsv[_y * w + x].saturation = sat;
    		colors_hsv[_y * w + x].brightness = 1;
    		colors_hsv[_y * w + x].a = 1;
    	}
    }*/

    std::vector<XYZ> colors_xyz(w * h);
    std::vector<CIE> colors_cie(w * h);

    std::vector<RGB> _colors_rgb;

    /*for (int y = 0; y < h; y++) {
    	for (int x = 0; x < w; x++) {
    		int _y = h - 1 - y;

    		const float k = 1.0f;

    		auto &rgb = _colors_rgb[_y * w + x];

    		rgb.r = float(x) / w;
    		rgb.g = float(y) / h;
    		rgb.b = (1.0f - rgb.r - rgb.g);

    		//float l = std::max(rgb.r, std::max(rgb.g, rgb.b)); //(rgb.r + rgb.g + rgb.b) / 3;
    		//rgb.r /= l;
    		//rgb.g /= l;
    		//rgb.b /= l;

    		rgb.a = 1;
    	}
    }*/

    for (float y = -0.3f; y < 1.6f; y += 0.002f) {
        for (float x = -2.0f; x < 3.0f; x += 0.002f) {
            RGB rgb = { x, y, 1 - x - y, 1 };

            _colors_rgb.push_back(rgb);
        }
    }

    /*for (int i = 0; i < w * h; i++)
    {
    	colors_xyz[i] = rgb_to_xyz(_colors_rgb);
    }*/

    float data_x[] = {	 0.1756,0.1752,0.1748,0.1745,0.1741,0.1740,0.1738,
                         0.1736,0.1733,0.1730,0.1726,0.1721,0.1714,
                         0.1703,0.1689,0.1669,0.1644,0.1611,0.1566,0.1510,0.1440,0.1355,
                         0.1241,0.1096,0.0913,0.0687,0.0454,0.0235,0.0082,
                         0.0039,0.0139,0.0389,0.0743,0.1142,0.1547,0.1929,
                         0.2296,0.2658,0.3016,0.3374,0.3731,0.4087,0.4441,
                         0.4788,0.5125,0.5448,0.5752,0.6029,0.6270,0.6482,
                         0.6658,0.6801,0.6915,0.7006,0.7079,0.7140,0.7190,
                         0.7230,0.7260,0.7283,0.7300,0.7311,0.7320,0.7327,
                         0.7334,0.7340,0.7344,0.7346,0.7347,0.7347,0.7347,
                         0.7347,0.7347,0.7347,0.7347,0.7347,0.7347,0.7347,
                         0.7347,0.7347,0.7347,0.7347,0.7347,0.7347,0.7347,
                         0.7347,0.7347,0.7347,0.7347,0.7347,0.7347,0.7347,0.7347,0.7347,0.7347,      0.1756
                     };

    float data_y[] = { 0.0053,0.0053,0.0052,0.0052,0.0050,0.0050,0.0049,
                       0.0049,0.0048,0.0048,0.0048,0.0048,0.0051,
                       0.0058,0.0069,0.0086,0.0109,0.0138,0.0177,
                       0.0227,0.0297,0.0399,0.0578,0.0868,0.1327,
                       0.2007,0.2950,0.4127,0.5384,0.6548,0.7502,
                       0.8120,0.8338,0.8262,0.8059,0.7816,
                       0.7543,0.7243,0.6923,0.6588,0.6245,0.5896,0.5547,
                       0.5202,0.4866,0.4544,0.4242,0.3965,0.3725,0.3514,
                       0.3340,0.3197,0.3083,0.2993,0.2920,0.2859,0.2809,0.2769,
                       0.2740,0.2717,0.2700,0.2689,0.2680
                       ,0.2673,0.2666,0.2660,0.2656,0.2654,0.2653,0.2653,0.2653,
                       0.2653,0.2653,0.2653,0.2653,0.2653,0.2653,0.2653,0.2653,
                       0.2653,0.2653,0.2653,0.2653,0.2653,0.2653,0.2653,0.2653,
                       0.2653,0.2653,0.2653,0.2653,0.2653,0.2653,0.2653,0.2653,       0.0053
                     };

    int num = sizeof(data_x) / sizeof(float);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int _y = h - 1 - y;

            //float X = float(x) / w;
            //float Y = float(y) / h;
            //float Z = 1 - colors_xyz[_y * w + x].x - colors_xyz[_y * w + x].y;

            auto &xyz = colors_xyz[_y * w + x];
            //xyz = rgb_to_xyz(_colors_rgb[_y * w + x]);



            xyz.x = float(x) / w;
            xyz.y = float(y) / h;
            xyz.z = (1 - xyz.x - xyz.y);
            xyz.a = 1;

            float l = std::max(xyz.x, std::max(xyz.y, xyz.z));
            //xyz.x /= l;
            //xyz.y /= l;
            //xyz.z /= l;

            colors_cie[_y * w + x].a = float(x) / w;
            colors_cie[_y * w + x].b = float(y) / h;
            colors_cie[_y * w + x].l = 1 - colors_cie[_y * w + x].a - colors_cie[_y * w + x].b;
            colors_cie[_y * w + x].m_a = 1;
        }
    }

    /*for (const auto &rgb : _colors_rgb) {
    	auto xyz = rgb_to_xyz(rgb);

    	int x = int(xyz.x * w);
    	int y = h - int(xyz.y * h) - 1;

    	if (x >= 0 && x < w && y >= 0 && y < h) {
    		colors_xyz[y * w + x] = xyz;
    	}
    }*/

    std::vector<RGB> colors_rgb(w * h);
    /*for (int i = 0; i < w * h; i++) {
    	colors_rgb[i] = hsv_to_rgb(colors_hsv[i]);

    	colors_rgb[i].r = clamp(colors_rgb[i].r, 0.0f, 1.0f);
    	colors_rgb[i].g = clamp(colors_rgb[i].g, 0.0f, 1.0f);
    	colors_rgb[i].b = clamp(colors_rgb[i].b, 0.0f, 1.0f);
    }*/

    /*for (int i = 0; i < w * h; i++)
    {
    	const auto &xyz = colors_xyz[i];

    	float x = xyz.x / (xyz.x + xyz.y + xyz.z);
    	float y = xyz.y / (xyz.x + xyz.y + xyz.z);

    	colors_rgb[(h - 1 - int(y * h)) * w + int(x * w)] = xyz_to_rgb(xyz);
    }*/

    for (int i = 0; i < w * h; i++) {
        auto &rgb = colors_rgb[i];

        //rgb = _colors_rgb[i];

        //CIE cie = { 50.0f, 0.5f, 0.5f, 1.0f };
        //XYZ xyz = cie_to_xyz(colors_cie[i]);
        //xyz.a = 1;
        rgb = xyz_to_rgb(colors_xyz[i]);
        /*rgb.r = colors_xyz[i].x;
        rgb.g = colors_xyz[i].y;
        rgb.b = colors_xyz[i].z;*/

        float k = 1.0f / (rgb.r + rgb.g + rgb.b);

        /*rgb.r *= k;
        rgb.g *= k;
        rgb.b *= k;*/

        rgb.a = 1;

        /*if (colors_rgb[i].r < 0 || colors_rgb[i].g < 0 || colors_rgb[i].b < 0 ||
        	colors_rgb[i].r > 1 || colors_rgb[i].g > 1 || colors_rgb[i].b > 1) {
        	colors_rgb[i] = { 0, 0, 0, 1 };
        }*/

        /*if (//rgb.r > 1 || rgb.g > 1 || rgb.b > 1 ||
        	rgb.r < 0 || rgb.g < 0 || rgb.b < 0) {
        	rgb = { 0, 0, 0, 1 };
        } else*/ {
            rgb.r = clamp(colors_rgb[i].r, 0.0f, 1.0f);
            rgb.g = clamp(colors_rgb[i].g, 0.0f, 1.0f);
            rgb.b = clamp(colors_rgb[i].b, 0.0f, 1.0f);
        }

        float l = std::max(rgb.r, std::max(rgb.g, rgb.b));
        /*rgb.r /= l;
        rgb.g /= l;
        rgb.b /= l;*/

        /*_colors_rgb[i].r = clamp(_colors_rgb[i].r, 0.0f, 1.0f);
        _colors_rgb[i].g = clamp(_colors_rgb[i].g, 0.0f, 1.0f);
        _colors_rgb[i].b = clamp(_colors_rgb[i].b, 0.0f, 1.0f);*/
    }

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int _y = h - 1 - y;

            float xx = float(x) / w;
            float yy = float(y) / h;

            for (int i = 1; i < num; i++) {
                math::vec2 p1 = { data_x[i - 1], data_y[i - 1] },
                           p2 = { data_x[i], data_y[i] };

                if (math::distance(p1, p2) < 0.01f) continue;

                /*float a1 = (xx - p1.x) / (p2.x - p1.x);
                float a2 = (yy - p1.y) / (p2.y - p1.y);

                if (math::sign(p2.x - p1.x) != math::sign(p2.y - p1.y) && a1 > a2 ||
                	math::sign(p2.x - p1.x) == math::sign(p2.y - p1.y) && a1 < a2) {*/

                if ((p2.x - p1.x) * (yy - p1.y) - (p2.y - p1.y) * (xx - p1.x) > 0) {
                    colors_rgb[_y * w + x].r = 0;
                    colors_rgb[_y * w + x].g = 0;
                    colors_rgb[_y * w + x].b = 0;
                    break;
                }
            }
        }
    }

#if defined(USE_SW_RENDER)
    swBlitPixels(0, 0, SW_FLOAT, SW_FRGBA, w, h, (void *)&colors_rgb[0], 1);
#endif

    for (int i = 1; i < num; i++) {
        math::vec2 p1 = { data_x[i - 1], data_y[i - 1] },
                   p2 = { data_x[i], data_y[i] };

        p1 *= 20.0f;
        p2 *= 20.0f;

        p1 -= { 10, 10 };
        p2 -= { 10, 10 };

        renderer_->DrawLine({ p1, 0 }, { p2, 0 }, { 1, 1, 1 });
    }

    renderer_->DrawLine({ -10, 0.9f * 20 - 10, 0 }, { 20, 0.9f * 20 - 10, 0 }, { 1, 1, 1 });
    renderer_->DrawLine({ 0.8f * 20 - 10, -20, 0 }, { 0.8f * 20 - 10, 20, 0 }, { 1, 1, 1 });

    ctx_->ProcessTasks();
}

void GSColorsTest::Update(int dt_ms) {
    using namespace GSColorsTestInternal;


}

void GSPackTest::HandleInput(InputManager::Event evt) {
    switch (evt.type) {
    case InputManager::RAW_INPUT_P1_DOWN: {
        int w = game_->width, h = game_->height;

        grabbed_ = true;
    }
    break;
    case InputManager::RAW_INPUT_P1_UP: {

    } break;
    case InputManager::RAW_INPUT_P1_MOVE: {
        int w = game_->width, h = game_->height;

    }
    break;
    case InputManager::RAW_INPUT_KEY_DOWN: {
        if (evt.raw_key == 'a') {
            should_add_ = true;
        } else if (evt.raw_key == 'd') {
            should_delete_ = true;
        }
    }
    break;
    case InputManager::RAW_INPUT_KEY_UP: {
        if (evt.raw_key == 'a') {
            should_add_ = false;
        } else if (evt.raw_key == 'd') {
            should_delete_ = false;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_SPACE) {

        }
    } break;
    case InputManager::RAW_INPUT_RESIZE:
        //cam_.Perspective(8, float(game_->width) / game_->height, 0.1f, 100.0f);
        break;
    default:
        break;
    }
}
