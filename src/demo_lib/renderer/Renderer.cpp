#include "Renderer.h"

#include <cassert>

Renderer::Renderer(ren::Context &ctx, const JsObject &config) : ctx_(ctx), current_cam_(nullptr) {
    Init();
    //Temp();
}