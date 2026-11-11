/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Creature/CaveCreature.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureTattoo.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/PosedModel.h"
#include "ECS/Registry.h"

namespace openblack::ecs::components
{

/// A copy of a creature's body drawn somewhere else, posed on its own. Its own CreatureAnimation, CreatureEyes and
/// Transform say how it is posed and where it stands. It is not a creature: nothing that looks for creatures finds it.
///
/// The copy of the Creature Cave and the copy of the falling spell film keep the looks that the creature was to show
/// when the copy was made, and have a CreatureMorph of their own. The copy of the tattoo menu has no CreatureMorph of
/// its own: it takes its shape, skin and tattoos from its creature each frame.
struct CreatureCopy
{
	/// The creature it is a copy of
	entt::entity creature {entt::null};
	/// The size it is drawn at
	float size {1.0f};
	/// The evil to good, thin to fat and weak to strong targets that the creature had when the copy was made. The copy
	/// keeps them and does not follow the creature after that. None for a copy that takes its looks from the creature.
	std::optional<creature_morph::Morph> targets;
	/// The evil to good target that the skin and the hair of the copy were painted with last. None: they were not
	/// painted, so the copy shows the base skin of its species and the hair of a creature that is neither evil nor good.
	std::optional<float> paintedAlignment;
};

/// The shape that a copy is drawn with: its own, else the shape of its creature. `bodies` is the registry of the
/// creature.
[[nodiscard]] inline const CreatureMorph* ShapeOfCopy(const Registry& copies, entt::entity copy, const Registry& bodies)
{
	if (const auto* own = copies.TryGet<const CreatureMorph>(copy))
	{
		return own;
	}
	const auto* self = copies.TryGet<const CreatureCopy>(copy);
	if (self == nullptr || !bodies.Valid(self->creature))
	{
		return nullptr;
	}
	return bodies.TryGet<const CreatureMorph>(self->creature);
}

/// The evil to good value that the hair of a copy takes its look from: the value its skin was painted with last
[[nodiscard]] inline float HairAlignmentOfCopy(const CreatureCopy& copy, const CreatureMorph& shape)
{
	return copy.targets.has_value() ? copy.paintedAlignment.value_or(0.0f) : shape.drawn.evilGood;
}

/// The copy of the player's creature that the Creature Cave shows by its fire
struct CaveCreature
{
	cave_creature::State state;
	/// Where the creature is lit from: the cave's fire
	glm::vec3 light {0.0f};
	/// Where each tattoo place of the creature is on the skin of its body. The tattoo editor finds the places when it
	/// first shows the copy. A place that the species does not have, or that no triangle holds, has none.
	std::optional<std::array<std::optional<posed_model::SkinPoint>, creature_tattoo::k_SlotCount>> tattooSites;
};

} // namespace openblack::ecs::components
