/*
** Command & Conquer Generals Evolution
** Adapter between runtime GameMessage objects and fixed-width Evolution commands.
*/

#pragma once

#include "Common/EvolutionCommandCodec.h"

class GameMessage;

namespace evolution
{

bool gameMessageToEvolutionCommand(const GameMessage &message, Command &command);
bool appendEvolutionCommandToGameMessage(const Command &command, GameMessage &message);

} // namespace evolution
