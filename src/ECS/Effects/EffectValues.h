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

#include "Enums.h"

namespace openblack
{
struct GEffectInfo;
} // namespace openblack

// The generic effect system the miracles hurt, heal and sway alignment with. Wiki: docs/bw1-notes/magic.md.

namespace openblack::ecs::effects
{

/// The amounts an effect applies, its radius and who applies it
struct EffectValues
{
	/// The indices of `numbers`, in the order of the effect info's amounts
	enum class Number : size_t
	{
		Burn,
		Crush,
		Hit,
		Heal,
		FlyAway,
		Alignment,
		Belief,

		_COUNT
	};
	/// The effect info's amounts
	std::array<float, static_cast<size_t>(Number::_COUNT)> numbers {};
	float radius {0.0f}; ///< Metres
	/// The object that applies it (a spell's creator); its player is the effect's player
	entt::entity appliedBy {entt::null};
	bool appliedByCreature {false};
	/// The applier's player
	std::optional<PlayerNames> player;
	/// The player blamed when it differs from the applier's: the caused player is taken first (else the applier's),
	/// the player the other way round. Unset: player (the spells, where both are the caster). Set when an
	/// abode is destroyed physically (the hand's player; the player is the hitter's). Read by the town's aggressor update
	std::optional<PlayerNames> causedPlayer;

	/// The 7 numbers and the radius of an effect info (the magic effect info's base)
	static EffectValues FromEffectInfo(const GEffectInfo& info);

	/// Multiplies the 7 numbers, not the radius (skipped for 1)
	void Scale(float factor);

	/// Burn, crush, hit or fly away above 0
	[[nodiscard]] bool IsDestructive() const;

	/// Every available effect receiver of the 10 m map cells over pos +- R whose fire centre is within R + its fire
	/// radius and whose altitude is within its height + R gets ApplyEffect. No falloff. pos: x, z metres, y the altitude
	/// above the land. Returns the last object hit (entt::null: none).
	entt::entity ApplyEffectToMapPos(const glm::vec3& position);
};

/// Whether the object takes effects: any object does; a dead villager refuses a heal
[[nodiscard]] bool IsEffectReceiver(entt::entity object, const EffectValues& values);

/// Crush and hit (x the info's defence multipliers) reduce the life, heal increases it, a kill is DestroyedByEffect, a
/// crush creates REACT_TO_OBJECT_CRUSHED, and the caster's alignment moves. Returns the original's "effectiveness":
/// (1 - life0) / heal + life0 / damage.
float ApplyEffect(entt::entity object, EffectValues& values);
/// The seven defence multipliers of the object's info, 1 each without an info (the same table ApplyEffect multiplies
/// by). An abode's physical destruction divides by it
[[nodiscard]] std::array<float, static_cast<size_t>(EffectValues::Number::_COUNT)> GetDefenseMultiplier(entt::entity object);

/// 0 below the combustion temperature Tc, else (T - Tc) / Tc x defenceMultiplierBurn x 0.1
[[nodiscard]] float ConvertTemperatureToDamage(entt::entity object, float temperature);

} // namespace openblack::ecs::effects
