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
#include <vector>

#include <entt/entity/fwd.hpp>

#include "Creature/CreatureMarks.h"
#include "Creature/CreatureTattoo.h"

namespace openblack::ecs::systems
{

/// Paints the creatures' skins (see components::CreatureSkin): blended towards evil or good as each body is drawn,
/// with its tattoos, wounds and blood over them, painted again whenever one of those changes. Heals the marks as the
/// game's turns go by.
class CreatureSkinSystemInterface
{
public:
	virtual ~CreatureSkinSystemInterface() = default;

	/// Once a frame, after the bodies are brought up to date: paints the skins that have changed, a new creature's
	/// straight away
	virtual void Update() = 0;
	/// Once a game turn: the marks heal a little
	virtual void ProcessTurn() = 0;
	/// In the creature's own turn, as its body lives the turn: its marks heal a step
	virtual void CreatureTurn(entt::entity /*creature*/) {}

	/// The creature's slots change. The skin keeps the old tattoos until something paints them again (see PaintTattoos).
	virtual void SetTattoos(entt::entity creature, const creature_tattoo::Slots& slots) = 0;
	/// The tattoos that the creature's skin shows in place of its slots, as the tattoo editor shows its edits. None shows
	/// the slots again. A copy of a creature shows its tattoos this way only when it has a skin of its own.
	virtual void ShowTattoos(entt::entity creature, std::optional<creature_tattoo::Slots> slots) = 0;
	/// The creature keeps a block of the player's own pictures for its tattoos (see creature_tattoo::PicturesBlock)
	virtual void SetTattooPictures(entt::entity creature, std::vector<uint8_t> pictures) = 0;
	/// Makes the masks of the creature's player again from the creature's tattoos (see creature_tattoo::RemakeMasks).
	/// The skins do not change until something paints them again.
	virtual void RemakeTattooMasks(entt::entity creature, creature_tattoo::Sheet sheet) = 0;
	/// Paints the creature's tattoos again, as they are now, with the masks of its player
	virtual void PaintTattoos(entt::entity creature) = 0;
	/// Puts tattoos on a creature as the end of a drag in the tattoo editor does. The creature gets the slots and the
	/// block of pictures again. Then the masks of its player change to the player's own pictures, and the skin changes.
	virtual void PutOnTattoos(entt::entity creature, const creature_tattoo::Slots& slots,
	                          const creature_tattoo::Pictures& pictures) = 0;
	/// A wound or burn, or a drop of blood, on a creature's skin
	virtual void AddWound(entt::entity creature, const creature_marks::Mark& wound) = 0;
	virtual void AddBlood(entt::entity creature, const creature_marks::Mark& blood) = 0;
	/// A creature's marks healed by some counts at once, as a heal effect does
	virtual void Heal(entt::entity creature, uint32_t counts) = 0;
};

} // namespace openblack::ecs::systems
