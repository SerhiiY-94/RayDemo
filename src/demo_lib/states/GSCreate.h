#pragma once

#include <memory>

enum eGameState { GS_CLIP_TEST, GS_DRAW_TEST, GS_RAY_TEST, GS_CURVE_TEST, GS_PACK_TEST, GS_COLORS_TEST, GS_SAMPLING_TEST };

class GameBase;
class GameState;

std::shared_ptr<GameState> GSCreate(eGameState state, GameBase *game);