/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

// What a spell and its particle effect exchange (creating the effect links it to its spell): the per-step input
// (ProcessInfo), the events the rules send back (SpellSink::SpellEvent) and the power-up level. Wiki:
// docs/bw1-notes/magic.md.

namespace openblack::psys
{

/// Copied into the spell when it is placed and refreshed every turn by the creator; every PSys step of the spell's
/// effect reads it
struct ProcessInfo
{
	glm::vec3 interfacePos {0.0f}; ///< (UNVERIFIED)
	/// the current hand / gesture position, metres; the script cast puts its "from" point here
	glm::vec3 handPos {0.0f};
	glm::vec3 cameraForward {0.0f}; ///< hand: the interface's camera forward; script: target - from
	glm::vec3 direction {0.0f};     ///< the hand velocity / cast direction (the spell's at init)
	float power {1.0f};             ///< the spell's strength: StrengthFloatProvider
	float curl {0.0f};              ///< the interface's or the script's curl (UNVERIFIED use)
	bool enabled {true};            ///< EventConditionTrueWhenEnabled: the creator still casts it
};

/// An event built by the rules and sent to the spell
struct SpellEventInfo
{
	enum class Type : int
	{
		Started = 1,          ///< when the effect starts
		Point = 2,            ///< EventAlways, UR_Explosion, the tornado base, SpellWater drops
		Landed = 3,           ///< LandscapeCollide SendEvent, lightning fork tips
		HitSpell = 4,         ///< a shield or another spell (target = that spell)
		Object = 5,           ///< UR_HealSpellChakra: the target object only
		CanDestroy = 7,       ///< a query: CanBeDestroyedBySpell
		InitWithoutPSys = 11, ///< the placing of a spell with no particle type
	};
	Type type {Type::Point};
	glm::vec3 position {0.0f}; ///< metres
	glm::vec3 velocity {0.0f}; ///< the movement (the spell's after an applied event)
	float strength {1.0f};     ///< multiplies the effect
	bool checkShields {false};
	entt::entity target {entt::null};
};

/// The spell behind an effect
class SpellSink
{
public:
	SpellSink() = default;
	SpellSink(const SpellSink&) = default;
	SpellSink(SpellSink&&) = default;
	SpellSink& operator=(const SpellSink&) = default;
	SpellSink& operator=(SpellSink&&) = default;
	virtual ~SpellSink() = default;

	/// The event sent to the spell: the rule's result (1 applied, 0 not)
	virtual int SpellEvent(const SpellEventInfo& event) = 0;
	/// -1 base, 0, 1
	[[nodiscard]] virtual int PowerUpLevel() const = 0;
	/// This computer's interface casts it
	[[nodiscard]] virtual bool IsMyInterfaceCasting() const { return false; }
	/// A human player casts it
	[[nodiscard]] virtual bool IsHumanPlayerCasting() const { return false; }
	/// Its creator is the script's player (the neutral one)
	[[nodiscard]] virtual bool IsScriptCasting() const { return false; }
	/// The spell's player; false without one
	[[nodiscard]] virtual bool Player([[maybe_unused]] int& player) const { return false; }
	/// The spell itself (a shield gives it as a shield hit's target)
	[[nodiscard]] virtual entt::entity SpellEntity() const { return entt::null; }
};

} // namespace openblack::psys
