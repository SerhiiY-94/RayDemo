#include "GSCreate.h"

#include <stdexcept>

#include "GSCPUTest.h"
#include "GSHDRTest.h"
#include "GSHybTest.h"
#include "GSRayTest.h"
#include "GSRayBucketTest.h"
#include "GSSamplingTest.h"

std::shared_ptr<GameState> GSCreate(eGameState state, GameBase *game) {
    if (state == GS_RAY_TEST) {
        return std::make_shared<GSRayTest>(game);
    } else if (state == GS_RAY_BUCK_TEST) {
        return std::make_shared<GSRayBucketTest>(game);
    } else if (state == GS_SAMPLING_TEST) {
        return std::make_shared<GSSamplingTest>(game);
    } else if (state == GS_CPU_TEST) {
        return std::make_shared<GSCPUTest>(game);
    } else if (state == GS_HYB_TEST) {
        return std::make_shared<GSHybTest>(game);
    } else if (state == GS_HDR_TEST) {
        return std::make_shared<GSHDRTest>(game);
    }
    throw std::invalid_argument("Unknown game state!");
}