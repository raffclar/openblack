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

} // namespace openblack::creature_file_load
