#include "GSHDRTest.h"

#include <fstream>
#include <sstream>

#if defined(USE_SW_RENDER)
#include <Ren/SW/SW.h>
#endif

#include <Eng/GameStateManager.h>
#include <Eng/Random.h>
#include <Ren/Context.h>
#include <Sys/AssetFile.h>
#include <Sys/Json.h>
#include <Sys/Log.h>
#include <Gui/Renderer.h>

#include "../Viewer.h"
#include "../ui/FontStorage.h"

namespace GSHDRTestInternal {
std::vector<uint8_t> LoadHDR(const char *file_name, int &out_w, int &out_h) {
    std::ifstream in_file(file_name, std::ios::binary);

    std::string line;
    if (!std::getline(in_file, line) || line != "#?RADIANCE") {
        throw std::runtime_error("Is not HDR file!");
    }

    float exposure = 1.0f;
    std::string format;

    while (std::getline(in_file, line)) {
        if (line.empty()) break;

        if (!line.compare(0, 6, "FORMAT")) {
            format = line.substr(7);
        } else if (!line.compare(0, 8, "EXPOSURE")) {
            exposure = atof(line.substr(9).c_str());
        }
    }

    if (format != "32-bit_rle_rgbe") {
        throw std::runtime_error("Wrong format!");
    }

    int res_x = 0, res_y = 0;

    std::string resolution;
    if (!std::getline(in_file, resolution)) {
        throw std::runtime_error("Cannot read resolution!");
    }

    {
        std::stringstream ss(resolution);
        std::string tok;

        ss >> tok;
        if (tok != "-Y") {
            throw std::runtime_error("Unsupported format!");
        }

        ss >> tok;
        res_y = atoi(tok.c_str());

        ss >> tok;
        if (tok != "+X") {
            throw std::runtime_error("Unsupported format!");
        }

        ss >> tok;
        res_x = atoi(tok.c_str());
    }

    if (!res_x || !res_y) {
        throw std::runtime_error("Unsupported format!");
    }

    out_w = res_x;
    out_h = res_y;

    std::vector<uint8_t> data(res_x * res_y * 4);
    int data_offset = 0;

    int scanlines_left = res_y;
    std::vector<uint8_t> scanline(res_x * 4);

    while (scanlines_left) {
        {
            uint8_t rgbe[4];

            if (!in_file.read((char *)&rgbe[0], 4)) {
                throw std::runtime_error("Cannot read file!");
            }

            if ((rgbe[0] != 2) || (rgbe[1] != 2) || ((rgbe[2] & 0x80) != 0)) {
                data[0] = rgbe[0];
                data[1] = rgbe[1];
                data[2] = rgbe[2];
                data[3] = rgbe[3];

                if (!in_file.read((char *)&data[4], (res_x * scanlines_left - 1) * 4)) {
                    throw std::runtime_error("Cannot read file!");
                }
                return data;
            }

            if ((((rgbe[2] & 0xFF) << 8) | (rgbe[3] & 0xFF)) != res_x) {
                throw std::runtime_error("Wrong scanline width!");
            }
        }

        int index = 0;
        for (int i = 0; i < 4; i++) {
            int index_end = (i + 1) * res_x;
            while (index < index_end) {
                uint8_t buf[2];
                if (!in_file.read((char *)&buf[0], 2)) {
                    throw std::runtime_error("Cannot read file!");
                }

                if (buf[0] > 128) {
                    int count = buf[0] - 128;
                    if ((count == 0) || (count > index_end - index)) {
                        throw std::runtime_error("Wrong data!");
                    }
                    while (count-- > 0) {
                        scanline[index++] = buf[1];
                    }
                } else {
                    int count = buf[0];
                    if ((count == 0) || (count > index_end - index)) {
                        throw std::runtime_error("Wrong data!");
                    }
                    scanline[index++] = buf[1];
                    if (--count > 0) {
                        if (!in_file.read((char *)&scanline[index], count)) {
                            throw std::runtime_error("Cannot read file!");
                        }
                        index += count;
                    }
                }
            }
        }

        for (int i = 0; i < res_x; i++) {
            data[data_offset + 0] = scanline[i + 0 * res_x];
            data[data_offset + 1] = scanline[i + 1 * res_x];
            data[data_offset + 2] = scanline[i + 2 * res_x];
            data[data_offset + 3] = scanline[i + 3 * res_x];
            data_offset += 4;
        }

        scanlines_left--;
    }

    return data;
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

    img_ = LoadHDR("grace-new.hdr", img_w_, img_h_);
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

        for (int j = 0; j < height; j++) {
            float v = float(j) / height;
            for (int i = 0; i < width; i++) {
                float u = float(i) / width;

                int ii = int(u * img_w_);
                int jj = int(v * img_h_);

                if (ii > img_w_ - 1) ii = img_w_ - 1;
                if (jj > img_h_ - 1) jj = img_h_ - 1;

                float r = img_[4 * (jj * img_w_ + ii) + 0] / 255.0f;
                float g = img_[4 * (jj * img_w_ + ii) + 1] / 255.0f;
                float b = img_[4 * (jj * img_w_ + ii) + 2] / 255.0f;
                float e = (float)img_[4 * (jj * img_w_ + ii) + 3];

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
