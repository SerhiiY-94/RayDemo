#pragma once

#include <Eng/GameBase.h>

const char UI_FONTS_KEY[]       = "ui_fonts";

const char RENDERER_KEY[]       = "renderer";
const char RAY_RENDERER_KEY[]   = "ray_renderer";     

const char TEST_RESULT_KEY[]    = "test_result";

const char SCENE_NAME_KEY[]     = "scene_name";

class Viewer : public GameBase {
public:
    Viewer(int w, int h, const char *local_dir, const char *scene_name, bool nogpu);
};

