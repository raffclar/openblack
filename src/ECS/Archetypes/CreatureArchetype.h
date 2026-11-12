/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/fwd.hpp>
#include <glm/fwd.hpp>

#include "Creature/CreatureMorph.h"
#include "Enums.h"

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::components
{
struct Creature;
struct CreatureMorph;
} // namespace openblack::ecs::components

namespace openblack::ecs::archetypes
{
class CreatureArchetype
{
public:
	/// What a creature has become: alignment from -1 (evil) to 1 (good), fatness and strength from 0 to 1. Its body is
	/// drawn from these, its species' own strength counting a little towards how strong it looks.
	struct Body
	{
		float alignment {0.0f};
		float fatness {0.5f};
		float strength {0.5f};
	};

	/// How a new creature of the species starts: neutral, as big, fat and strong as the species starts
	[[nodiscard]] static float StartScale(CreatureType species);
	[[nodiscard]] static Body StartBody(CreatureType species);
	/// The scale a creature of the species is drawn at for its size, by its base mesh's rest pose
	[[nodiscard]] static float DrawnScale(CreatureType species, float size);
	/// The species' own strength, 0 to 1, which counts a little towards how strong its creatures look
	[[nodiscard]] static float SpeciesStrength(CreatureType species);
	/// The creature's body takes the weak to strong look of its strength now, as when its strength is set or changed by an
	/// amount
	static void ShowStrength(entt::entity creature);
	/// The creature's body takes its looks: its shown fatness moves one step, and it shows its alignment and strength as
	/// they are now. This occurs at the end of each turn that the hand does not hold it, when a mind file loads into
	/// it, and when a swap of minds changes it.
	static void TakeLooks(Registry& registry, entt::entity creature);
	/// The evil to good, thin to fat and weak to strong targets that the creature's body moves towards: the looks it took
	/// last, or its alignment and strength as they are now before it takes looks
	[[nodiscard]] static creature_morph::Morph Targets(const components::Creature& creature,
	                                                   const components::CreatureMorph& morph);
	/// The skin and the hair of the creature get the evil to good target that the creature has now, as each paint of
	/// the skin does (see components::CreatureMorph::paintedAlignment)
	static void PaintWithTarget(Registry& registry, entt::entity creature);
	/// One morph step of the creature's body: each drawn axis that is far enough from its target moves to it (see
	/// creature_morph::RefreshDrawn). A new alignment also paints the skin and the hair with the evil to good target.
	/// The step occurs once in each creature turn, and once at the end of a swap of minds. It gives false when the
	/// creature has no body to morph.
	static bool MorphStep(Registry& registry, entt::entity creature);

	static entt::entity Create(const glm::vec3& position, PlayerNames playerName, CreatureType creatureType,
	                           entt::id_type creatureMindId, float yAngleRadians, float scale, const Body& body);
	static entt::entity Create(const glm::vec3& position, PlayerNames playerName, CreatureType creatureType,
	                           entt::id_type creatureMindId, float yAngleRadians, float scale)
	{
		return Create(position, playerName, creatureType, creatureMindId, yAngleRadians, scale, Body {});
	}
	CreatureArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
