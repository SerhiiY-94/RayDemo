#include "GSSamplingTest.h"

#include <fstream>

#if defined(USE_SW_RENDER)
#include <ren/SW/SW.h>
#endif

#include <engine/GameStateManager.h>
#include <ren/Context.h>
#include <sys/Json.h>
#include <sys/Log.h>
#include <ui/Renderer.h>

#include <ray/internal/Core.h>
#include <ray/internal/Halton.h>

#include "../Viewer.h"

namespace GSSamplingTestInternal {
float EvalFunc(const float x, const float y, const float xmax, const float ymax) {
    //return 1. / 2. + 1. / 2. * powf(1. - y / ymax, 3.) * sin(2. * math::pi<float>() * (x / xmax) * exp(8. * (x / xmax)));
    return 0.5f + 0.5f * sin(2.0f * math::pi<float>() * (x / xmax) * exp(8.0f * (x / xmax)));
    //float d = sqrt(xmax * xmax + ymax * ymax);
    //float l = sqrt(x * x + y * y);
    //float t = (x * 0.995 + y * 0.1) / xmax;
    //double f = 16 * 128.0 * x / xmax;
    //return std::cos(t * t * 1024) * 0.5f + 0.5f;
    /*if (math::fract(t * (128 + t * 128)) < 0.5f) {
    	return 1.0f;
    } else {
    	return 0.0f;
    }*/
}

double drand48() {
    return double(rand()) / RAND_MAX;
}

std::vector<uint16_t> radical_inv_perms;
}

GSSamplingTest::GSSamplingTest(GameBase *game) : game_(game) {
    state_manager_	= game->GetComponent<GameStateManager>(STATE_MANAGER_KEY);
    ctx_			= game->GetComponent<ren::Context>(REN_CONTEXT_KEY);
    renderer_		= game->GetComponent<Renderer>(RENDERER_KEY);

    ui_renderer_ = game->GetComponent<ui::Renderer>(UI_RENDERER_KEY);
    ui_root_ = game->GetComponent<ui::BaseElement>(UI_ROOT_KEY);

    const auto fonts = game->GetComponent<FontStorage>(UI_FONTS_KEY);
    font_ = fonts->FindFont("main_font");
}

GSSamplingTest::~GSSamplingTest() {

}

void GSSamplingTest::Enter() {
#if defined(USE_SW_RENDER)
    swEnable(SW_FAST_PERSPECTIVE_CORRECTION);
    swEnable(SW_DEPTH_TEST);
#endif
    using namespace GSSamplingTestInternal;

    radical_inv_perms = ray::ComputeRadicalInversePermutations(ray::g_primes, ray::PrimesCount, ::rand);
}

void GSSamplingTest::Exit() {

}

void GSSamplingTest::Draw(float dt_s) {
    using namespace math;
    using namespace GSSamplingTestInternal;

    //renderer_->set_current_cam(&cam_);
    //renderer_->ClearColorAndDepth(0, 0, 0, 1);

    uint32_t width = game_->width, height = game_->height;

    int sample_limit = 8;
    if (++iteration_ > sample_limit) {
        return;
    }

    pixels_.resize(width * height * 4);

    if (iteration_ == 1) {
        std::fill(pixels_.begin(), pixels_.end(), 0.0f);
    }

    LOGI("%i", int(iteration_));

    uint32_t nsamplesx = 4, nsamplesy = 1;

#if 1
    for (uint32_t y = 0; y < height; ++y) {
        if (y < height / 4) {
            for (uint32_t x = 0; x < width; ++x) {
                float sum = 0;

                for (uint32_t ny = 0; ny < nsamplesy; ++ny) {
                    for (uint32_t nx = 0; nx < nsamplesx; ++nx) {
                        sum += EvalFunc(x + (iteration_ - 1)/sample_limit + (nx + 0.5f) / (nsamplesx * sample_limit), y + (ny + 0.5f) / nsamplesy, width, height);
                    }
                }

                float val = sum / (nsamplesx * nsamplesy);
                float prev = pixels_[4 * (y * width + x) + 0];
                pixels_[4 * (y * width + x) + 0] += (val - prev) / iteration_;
                pixels_[4 * (y * width + x) + 1] = pixels_[4 * (y * width + x) + 2] = pixels_[4 * (y * width + x) + 0];
                pixels_[4 * (y * width + x) + 3] = 1.0f;
            }
        } else if (y < 2 * (height / 4)) {
            for (uint32_t x = 0; x < width; ++x) {
                float sum = 0;

                for (uint32_t ny = 0; ny < nsamplesy; ++ny) {
                    for (uint32_t nx = 0; nx < nsamplesx; ++nx) {
                        sum += EvalFunc(x + drand48(), y + drand48(), width, height);
                    }
                }

                float val = sum / (nsamplesx * nsamplesy);
                float prev = pixels_[4 * (y * width + x) + 0];
                pixels_[4 * (y * width + x) + 0] += (val - prev) / iteration_;
                pixels_[4 * (y * width + x) + 1] = pixels_[4 * (y * width + x) + 2] = pixels_[4 * (y * width + x) + 0];
                pixels_[4 * (y * width + x) + 3] = 1.0f;
            }
        } else if (y < 3 * (height / 4)) {
            for (uint32_t x = 0; x < width; ++x) {
                float sum = 0;

                for (uint32_t ny = 0; ny < nsamplesy; ++ny) {
                    for (uint32_t nx = 0; nx < nsamplesx; ++nx) {
                        sum += EvalFunc(x + (nx + drand48()) / nsamplesx, y + (ny + drand48()) / nsamplesy, width, height);
                    }
                }

                float val = sum / (nsamplesx * nsamplesy);
                float prev = pixels_[4 * (y * width + x) + 0];
                pixels_[4 * (y * width + x) + 0] += (val - prev) / iteration_;
                pixels_[4 * (y * width + x) + 1] = pixels_[4 * (y * width + x) + 2] = pixels_[4 * (y * width + x) + 0];
                pixels_[4 * (y * width + x) + 3] = 1.0f;
            }
        } else {
            uint32_t ndx = 0;
            for (uint32_t x = 0; x < width; ++x) {
                float sum = 0;

                int i = (int)iteration_ - 0;


                for (uint32_t ny = 0; ny < nsamplesy; ++ny) {
                    for (uint32_t nx = 0; nx < nsamplesx; ++nx) {
                        int last_ndx = ndx;
                        //ndx = ((y - 3 * (height / 4)) * width + x) * nsamplesx * sample_limit * 31 + i * nsamplesx + nx;
                        ndx = ((y - 3 * (height / 4)) * width + x) * 31 + i * nsamplesx + nx;
                        //ndx = (i * (width + height) + x) * nsamplesx + nx;
                        //if (!(last_ndx == 0 || last_ndx + 1 == ndx)) {
                        //__debugbreak();
                        //}

                        if (x == 0 && (y - 3 * (height / 4)) == 0) {
                            volatile int ii = 0;
                        }

                        //float rx = RadicalInverse<3>(ndx);
                        float rx = ray::ScrambledRadicalInverse<29>(&radical_inv_perms[100], ndx);
                        float ry = 0;//RadicalInverse<2>(i * nsamplesx + nx);

                        //sum += EvalFunc(x + (nx + rx) / nsamplesx, y + (ny + ry) / nsamplesy, width, height);
                        sum += EvalFunc(x + rx, y + ry, width, height);
                    }
                }

                float val = sum / (nsamplesx * nsamplesy);
                float prev = pixels_[4 * (y * width + x) + 0];
                pixels_[4 * (y * width + x) + 0] += (val - prev) / iteration_;
                pixels_[4 * (y * width + x) + 1] = pixels_[4 * (y * width + x) + 2] = pixels_[4 * (y * width + x) + 0];
                pixels_[4 * (y * width + x) + 3] = 1.0f;
            }
            volatile int ii = 0;
        }
    }
#endif

#if 0
    int i = (int)iteration_ * nsamplesx;
    for (int j = 0; j < nsamplesx; j++) {
        //float rx = RadicalInverse<3>(i + j);
        float rx = ray::ScrambledRadicalInverse<3>(&radical_inv_perms[2], (3000 + i + j) % 4096);
        //float ry = RadicalInverse<5>(i + j);
        float ry = ray::ScrambledRadicalInverse<5>(&radical_inv_perms[5], (3000 + i + j) % 4096);

        int x = rx * (width - 0);
        int y = height - 1 - ry * (height - 0);

        pixels_[4 * (y * width + x) + 0] = 1.0f;
        pixels_[4 * (y * width + x) + 1] = pixels_[4 * (y * width + x) + 2] = pixels_[4 * (y * width + x) + 0];
        pixels_[4 * (y * width + x) + 3] = 1.0f;

        /*for (int y = 0; y < height; y++) {
        	pixels_[4 * (y * width + x) + 0] = 1.0f;
        	pixels_[4 * (y * width + x) + 1] = pixels_[4 * (y * width + x) + 2] = pixels_[4 * (y * width + x) + 0];
        	pixels_[4 * (y * width + x) + 3] = 1.0f;
        }*/
    }
#endif

#if defined(USE_SW_RENDER)
    swBlitPixels(0, 0, SW_FLOAT, SW_FRGBA, width, height, &pixels_[0], 1);
#endif

#if 1
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

void GSSamplingTest::Update(int dt_ms) {

}

void GSSamplingTest::HandleInput(InputManager::Event evt) {
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
    }
    break;
    case InputManager::RAW_INPUT_RESIZE:
        iteration_ = 0;
        break;
    default:
        break;
    }
}
