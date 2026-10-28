/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureTattooChange.h"

#include <utility>

using namespace openblack;
using namespace openblack::ecs;

void creature_tattoo_change::Apply(components::CreatureTattoos& tattoos, player_commands::CreatureTattoo change,
                                   bool networkGame, const Submit& submit)
{
	if (!submit)
	{
		tattoos.slots = change.slots;
		if (change.hasPictures)
		{
			tattoos.pictures = std::move(change.pictures);
		}
		return;
	}
	// The skin shows the change. In a game alone it shows it until the turn puts it on the creature.
	tattoos.shown = change.slots;
	if (change.hasPictures)
	{
		tattoos.picturesShown = true;
		tattoos.shownPictures = change.pictures;
	}
	// No packet of the game sends tattoos. In a network game, the change does not go to the other computers.
	if (!networkGame)
	{
		submit(std::move(change));
	}
}

const creature_tattoo::Slots& creature_tattoo_change::ShownSlots(const components::CreatureTattoos& tattoos)
{
	return tattoos.shown.has_value() ? *tattoos.shown : tattoos.slots;
}

const std::vector<uint8_t>& creature_tattoo_change::ShownPictures(const components::CreatureTattoos& tattoos)
{
	return tattoos.picturesShown ? tattoos.shownPictures : tattoos.pictures;
}
