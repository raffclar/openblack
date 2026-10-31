/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureFileLoad.h"

namespace openblack::creature_file_load
{

Refusal WhyNot(const Request& request)
{
	if (!request.playerInGame)
	{
		return Refusal::NoPlayer;
	}
	if (!request.fileOpens)
	{
		return Refusal::NoFile;
	}
	// The game does not examine these two and fails. openblack makes no creature.
	if (!request.onMap)
	{
		return Refusal::OffMap;
	}
	if (!request.species.has_value())
	{
		return Refusal::UnknownSpecies;
	}
	return Refusal::None;
}

Refusal WhyNotBrought(const BroughtRequest& request)
{
	// Only a god with a citadel asks the other computers to make its creature
	if (!request.citadel)
	{
		return Refusal::NoCitadel;
	}
	if (!request.file || !request.readable)
	{
		return Refusal::NoFile;
	}
	if (!request.species.has_value())
	{
		return Refusal::UnknownSpecies;
	}
	// A god has one creature. The creature that the god has stays, and the game makes no new creature.
	if (request.hasCreature)
	{
		return Refusal::HasCreature;
	}
	// The game does not examine the place. openblack makes no creature off the map.
	if (!request.onMap)
	{
		return Refusal::OffMap;
	}
	return Refusal::None;
}

bool KeptByThisComputer(const BroughtCreature& creature)
{
	return creature.ownGod && !creature.clanOrder && !creature.ownGodPlaysForClan;
}

} // namespace openblack::creature_file_load
