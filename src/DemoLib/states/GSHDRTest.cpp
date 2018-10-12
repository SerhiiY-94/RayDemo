#include "GSHDRTest.h"

#include <fstream>
#include <sstream>

#if defined(USE_SW_RENDER)
#include <Ren/SW/SW.h>
#endif

#include <Eng/GameStateManager.h>
#include <Eng/Random.h>
#include <Ren/Context.h>
#include <Ren/MMat.h>
#include <Sys/AssetFile.h>
#include <Sys/Json.h>
#include <Sys/Log.h>
#include <Gui/Renderer.h>

#include "../Viewer.h"
#include "../load/Load.h"
#include "../ui/FontStorage.h"

namespace GSHDRTestInternal {
    double LegendrePolynomial(int l, int m, double x) {
        double pmm = 1.0;
        if (m > 0) {
            double somx2 = std::sqrt((1.0 - x) * (1.0 + x));
            double fact = 1.0;
            for (int i = 1; i <= m; i++) {
                pmm *= (-fact) * somx2;
                fact += 2.0;
            }
        }
        if (l == m) return pmm;
        double pmmp1 = x * (2.0 * m + 1.0) * pmm;
        if (l == m + 1) return pmmp1;
        double pll = 0.0;
        for (int ll = m + 2; ll <= l; ll++) {
            pll = ((2.0 * ll - 1.0) * x * pmmp1 - (ll + m - 1.0) * pmm) / (ll - m);
            pmm = pmmp1;
            pmmp1 = pll;
        }
        return pll;
    }

    int factorial(int x) {
        int ret = 1;
        for (int i = 2; i <= x; i++) {
            ret *= i;
        }
        return ret;
    }

    double SH_RenormConstant(int l, int m) {
        const double Pi = 3.1415926535897932384626433832795;
        double temp = ((2.0 * l + 1.0) * factorial(l - m)) / (4.0 * Pi * factorial(l + m));
        return std::sqrt(temp);
    }

    // l - band in range [0..N]
    // m in range [-l..l]
    // theta in range [0..Pi]
    // phi in range [0..2 * Pi]
    double SH_Evaluate(int l, int m, double theta, double phi) {
        const double sqrt2 = std::sqrt(2.0);
        if (m == 0) {
            return SH_RenormConstant(l, 0) * LegendrePolynomial(l, m, std::cos(theta));
        } else if (m > 0) {
            return sqrt2 * SH_RenormConstant(l, m) * std::cos(m * phi) * LegendrePolynomial(l, m, std::cos(theta));
        } else {
            return sqrt2 * SH_RenormConstant(l, -m) * std::sin(-m * phi) * LegendrePolynomial(l, -m, std::cos(theta));
        }
    }
}

GSHDRTest::GSHDRTest(GameBase *game) : game_(game) {
    state_manager_	= game->GetComponent<GameStateManager>(STATE_MANAGER_KEY);
    ctx_			= game->GetComponent<Ren::Context>(REN_CONTEXT_KEY);
    renderer_		= game->GetComponent<Renderer>(RENDERER_KEY);

    random_         = game->GetComponent<Random>(RANDOM_KEY);

    ui_renderer_ = game->GetComponent<Gui::Renderer>(UI_RENDERER_KEY);
    ui_root_ = game->GetComponent<Gui::BaseElement>(UI_ROOT_KEY);

    const auto fonts = game->GetComponent<FontStorage>(UI_FONTS_KEY);
    font_ = fonts->FindFont("main_font");
}

void GSHDRTest::Enter() {
#if defined(USE_SW_RENDER)
    swEnable(SW_FAST_PERSPECTIVE_CORRECTION);
    swEnable(SW_DEPTH_TEST);
#endif
    using namespace GSHDRTestInternal;

    img_ = LoadHDR("assets/textures/grace-new.hdr", img_w_, img_h_);
}

void GSHDRTest::Exit() {

}

void GSHDRTest::Draw(float dt_s) {
    using namespace Ren;
    using namespace GSHDRTestInternal;

    //renderer_->set_current_cam(&cam_);
    //renderer_->ClearColorAndDepth(0, 0, 0, 1);

    uint32_t width = (uint32_t)game_->width, height = (uint32_t)game_->height;

    int sample_limit = 32;
    if (++iteration_ > sample_limit) {
        //return;
    }

    pixels_.resize(width * height * 4);

    if (iteration_ == 1) {
        //std::fill(pixels_.begin(), pixels_.end(), 0.0f);

        for (uint32_t j = 0; j < height; j++) {
            float v = float(j) / height;
            for (uint32_t i = 0; i < width; i++) {
                float u = float(i) / width;

                int ii = int(u * img_w_);
                int jj = int(v * img_h_);

                if (ii > img_w_ - 1) ii = img_w_ - 1;
                if (jj > img_h_ - 1) jj = img_h_ - 1;

                float r = img_[jj * img_w_ + ii].r / 255.0f;
                float g = img_[jj * img_w_ + ii].g / 255.0f;
                float b = img_[jj * img_w_ + ii].b / 255.0f;
                float e = (float)img_[jj * img_w_ + ii].a;

                float f = std::pow(2.0f, e - 128.0f);

                r *= f;
                g *= f;
                b *= f;

                pixels_[4 * (j * width + i) + 0] = std::min(r * mul_, 1.0f);
                pixels_[4 * (j * width + i) + 1] = std::min(g * mul_, 1.0f);
                pixels_[4 * (j * width + i) + 2] = std::min(b * mul_, 1.0f);
                pixels_[4 * (j * width + i) + 3] = 1.0f;
            }
        }
    }

    

#if defined(USE_SW_RENDER)
    swBlitPixels(0, 0, 0, SW_FLOAT, SW_FRGBA, width, height, &pixels_[0], 1);
#endif

#if 0
    {
        // ui draw
        ui_renderer_->BeginDraw();

        float font_height = font_->height(ui_root_.get());
        font_->DrawText(ui_renderer_.get(), "regular", { 0.25f, 1 - font_height }, ui_root_.get());
        font_->DrawText(ui_renderer_.get(), "random", { 0.25f, 1 - 2 * 0.25f - font_height }, ui_root_.get());
        font_->DrawText(ui_renderer_.get(), "jittered", { 0.25f, 1 - 2 * 0.5f - font_height }, ui_root_.get());
        font_->DrawText(ui_renderer_.get(), "halton", { 0.25f, 1 - 2 * 0.75f - font_height }, ui_root_.get());

        ui_renderer_->EndDraw();
    }
#endif

    ctx_->ProcessTasks();
}

void GSHDRTest::Update(int dt_ms) {

}

void GSHDRTest::HandleInput(InputManager::Event evt) {
    switch (evt.type) {
    case InputManager::RAW_INPUT_P1_DOWN: {

    } break;
    case InputManager::RAW_INPUT_P1_UP:

        break;
    case InputManager::RAW_INPUT_P1_MOVE: {

    } break;
    case InputManager::RAW_INPUT_KEY_UP: {
        if (evt.key == InputManager::RAW_INPUT_BUTTON_SPACE) {
            iteration_ = 0;
        }
    } break;
    case InputManager::RAW_INPUT_MOUSE_WHEEL: {
        mul_ += evt.move.dy * 0.1f;
        LOGI("%f", mul_);
        iteration_ = 0;
    } break;
    case InputManager::RAW_INPUT_RESIZE:
        iteration_ = 0;
        break;
    default:
        break;
    }
}
