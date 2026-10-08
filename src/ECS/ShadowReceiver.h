/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include "Enums.h"

namespace openblack::ecs::shadow_receiver
{

/// What decides whether an object's 3D object receives the projected shadows that fall on objects
struct Object
{
	bool powerUpBand = false;   ///< the Power_Up_Band mesh, which no game object uses
	bool tree = false;          ///< a tree, a dead tree or a big forest
	bool forest = false;        ///< a forest
	bool hand = false;          ///< the hand
	bool creature = false;      ///< a creature
	bool orb = false;           ///< a one-shot spell orb
	bool shield = false;        ///< a magic or physical shield
	bool templePart = false;    ///< a part of the temple interior
	bool handFxPart = false;    ///< a part of the hand's effects
	std::optional<PotInfo> pot; ///< the pot's type, for a pot
};

/// The receivers of the projected shadows: a flag of the 3D object, set only when shadows are on and the object asks
/// for them. A new 3D object has it clear, so only what turns it on receives:
/// - every game object when its 3D object is made; trees, forests, flowers, magic food, the food in the hand and a few
///   others turn it off again. So do the one-shot orb, the dispensers' bubble, and the two shields (magic and
///   physical), the same call as the trees';
/// - the spell seed graphic's mesh;
/// and not the objects made with the bare 3D object, which never ask: the hand FX's power-up bands and the seed
/// graphic's power-up band (the same mesh), nor the PSys mesh atoms (their creator's setting, 0 by default and no
/// property). Nor a creature: the body it is drawn with is its own animated 3D object, made bare when the creature is,
/// and the game object's 3D object is replaced by it, so the creature's flag stays clear and no shadow falls on it
[[nodiscard]] constexpr bool Receives(const Object& object)
{
	if (object.powerUpBand || object.tree || object.forest || object.hand || object.creature || object.orb || object.shield ||
	    object.templePart || object.handFxPart)
	{
		return false;
	}
	return !object.pot.has_value() || (*object.pot != PotInfo::HandFood && *object.pot != PotInfo::MagicFood);
}

} // namespace openblack::ecs::shadow_receiver
