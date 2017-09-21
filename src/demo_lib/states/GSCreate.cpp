#include "GSCreate.h"

#include <stdexcept>

#include "GSClipTest.h"
#include "GSColorsTest.h"
#include "GSCurveTest.h"
#include "GSDrawTest.h"
#include "GSPackTest.h"
#include "GSRayTest.h"
#include "GSSamplingTest.h"

std::shared_ptr<GameState> GSCreate(eGameState state, GameBase *game) {
    if (state == GS_CLIP_TEST) {
        return std::make_shared<GSClipTest>(game);
    } else if (state == GS_DRAW_TEST) {
        return std::make_shared<GSDrawTest>(game);
    } else if (state == GS_RAY_TEST) {
        return std::make_shared<GSRayTest>(game);
    } else if (state == GS_CURVE_TEST) {
        return std::make_shared<GSCurveTest>(game);
    } else if (state == GS_PACK_TEST) {
        return std::make_shared<GSPackTest>(game);
    } else if (state == GS_COLORS_TEST) {
        return std::make_shared<GSColorsTest>(game);
    } else if (state == GS_SAMPLING_TEST) {
        return std::make_shared<GSSamplingTest>(game);
    }
    throw std::invalid_argument("Unknown game state!");
}