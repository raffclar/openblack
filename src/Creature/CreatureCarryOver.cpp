/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureCarryOver.h"

#include <string>

#include <PhysiqueFile.h>

using namespace openblack;
using namespace openblack::creature_carry_over;

map_coords::MapCoords creature_carry_over::ArrivalCoords(glm::vec2 place)
{
	return map_coords::CellCentre(map_coords::CellOf(map_coords::ToFixed(place.x)),
	                              map_coords::CellOf(map_coords::ToFixed(place.y)));
}

creature_carry_over::KeptFiles creature_carry_over::KeptFilesIn(const std::filesystem::path& folder,
                                                                std::string_view profileFile)
{
	return {.mind = folder / std::string(profileFile), .physique = folder / creaturemind::PhysiqueFileName(profileFile)};
}

bool creature_carry_over::SavesAsItLives(uint32_t turnsBefore, uint32_t turnsNow)
{
	if (turnsNow == 0)
	{
		return false;
	}
	const bool atMultiple = turnsNow % k_SaveEveryTurns == 0;
	const bool steppedPast = turnsNow > turnsBefore && turnsNow / k_SaveEveryTurns > turnsBefore / k_SaveEveryTurns;
	return atMultiple || steppedPast;
}

bool creature_carry_over::WritesToPlayersFiles(const WriteFacts& facts)
{
	return facts.networkGame ? facts.broughtHere : facts.localPlayers;
}
