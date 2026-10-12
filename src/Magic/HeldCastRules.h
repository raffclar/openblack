/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>

#include "Enums.h"

// Where the miracle held in the hand may be cast, as the hand's tooltip tells the player while it is held (whether or
// not the action button is down). Over a thing the pointer is on, only that thing counts; over the bare land, the land.
// Pure rules, tested without the game.

namespace openblack::magic
{

/// The thing under the pointer, as far as casting the held miracle on it goes
struct PointedThing
{
	/// A script has highlighted it: the hand never casts on such a thing
	bool scriptHighlight {false};
	/// A fireball, anyone's
	bool fireBall {false};
	/// The miracle may be cast on it by its own rules: the seed is one cast on things (the creature spells), the thing
	/// stands where the miracle's table lets it be cast, and the miracle's kind takes it (a creature with room for
	/// another spell)
	bool castOnByKind {false};
};

/// Whether a held seed may be cast on the thing under the pointer: never on a highlighted thing; a fireball takes only
/// the fire seed (which grows stronger from it); anything else by the miracle's own rules
[[nodiscard]] bool MayCastOn(SpellSeedType seed, const PointedThing& thing);

/// The miracle held in the hand, and where the pointer is
struct HeldCastAtPointer
{
	/// The hand is in the player's influence
	bool handInInfluence {false};
	/// Held long enough to cast
	bool ready {false};
	/// The miracle it casts now, by its power-up
	MagicType type {MagicType::None};
	/// Over a thing: whether the miracle may be cast on it (MayCastOn). None over the bare land.
	std::optional<bool> onPointed;
	/// Whether the miracle may be cast at the land under the pointer
	bool atPoint {false};
};

/// What the hand offers to do with the miracle it holds
enum class HeldCastOffer : uint8_t
{
	/// Nothing: the hand shows what it would over nothing
	None,
	/// Cast it, by the action button
	Cast,
	/// Cast it at a circle drawn: the storms, the tornado and the shields over the land
	CastCircle,
};

/// The offer: only with the hand in the player's influence and the seed ready. Over a thing, "Cast" where it may be
/// cast on the thing; over the land, "Cast" where it may be cast there ("Cast Circle" for the miracles sized by a
/// circle). Nowhere else.
[[nodiscard]] HeldCastOffer OfferFor(const HeldCastAtPointer& held);

} // namespace openblack::magic
