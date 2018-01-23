#pragma once

#include <memory>

enum eGameState { GS_RAY_TEST, GS_SAMPLING_TEST };

class GameBase;
class GameState;

std::shared_ptr<GameState> GSCreate(eGameState state, GameBase *game);