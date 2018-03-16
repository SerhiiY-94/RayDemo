#pragma once

#include <engine/GameBase.h>

const char UI_FONTS_KEY[]               = "ui_fonts";

const char RENDERER_KEY[]               = "renderer";
const char RAY_RENDERER_KEY[]			= "ray_renderer";

class Viewer : public GameBase {
public:
    Viewer(int w, int h, const char *local_dir);
};

