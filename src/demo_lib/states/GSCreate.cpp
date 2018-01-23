#include "GSCreate.h"

#include <stdexcept>

#include "GSRayTest.h"
#include "GSSamplingTest.h"

std::shared_ptr<GameState> GSCreate(eGameState state, GameBase *game) {
    if (state == GS_RAY_TEST) {
        return std::make_shared<GSRayTest>(game);
    } else if (state == GS_SAMPLING_TEST) {
        return std::make_shared<GSSamplingTest>(game);
    }
    throw std::invalid_argument("Unknown game state!");
}