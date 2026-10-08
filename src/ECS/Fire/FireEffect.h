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

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::effects
{
struct EffectValues;
} // namespace openblack::ecs::effects

// The fire system: every object that is hotter than the ambient air
// has a FireEffect with its temperature. Above its combustion temperature Tc it burns: it heats up to 2 Tc, loses life,
// chars, heats what is near (the same model for fireballs, lightning, held and thrown objects) and cools down again when
// nothing heats it. One game turn = 0.1 s. See docs/bw1-notes/magic.md.

namespace openblack::ecs::fire
{
/// The fire of one object
struct FireEffect
{
	// The bits of `flags`
	static constexpr uint8_t k_JustIgnited = 0x01; ///< Tprev < Tc <= T this turn
	static constexpr uint8_t k_VeryHot = 0x02;     ///< T > 3 Tc
	static constexpr uint8_t k_Cooling = 0x04;     ///< cooled (water, rain or T < Tprev) this turn
	static constexpr uint8_t k_JustExtinguished = 0x08;
	static constexpr uint8_t k_Deleted = 0x10;
	static constexpr uint8_t k_SoundPlaying = 0x20;

	uint32_t id {0};                   ///< the port's handle (villagers keep it to find the fire they fight)
	float temperature {0.0f};          ///< T
	float previous {0.0f};             ///< T at the end of the last Process
	entt::entity object {entt::null};  ///< the burning object (it points back to its fire)
	std::optional<PlayerNames> player; ///< the player responsible, if any
	/// the thing that heated it (a spell, an object); never gets heat back from this fire; cleared when gone
	entt::entity source {entt::null};
	uint32_t reaction {0}; ///< REACT_TO_FIRE (10) or REACT_TO_BURNING_OBJECT_IN_HAND (33)
	uint8_t tag {0};       ///< the creation tag at creation; processed while it equals the process tag
	float charring {0.0f}; ///< 0..1
	uint8_t flags {0};
	FireEffect* root {this};    ///< the first fire of its group
	FireEffect* next {nullptr}; ///< the next of the group
	/// the group root only: the villagers fighting it, newest first
	std::vector<entt::entity> firemen;

	/// max(combustion temperature, 40); 0 without an object
	[[nodiscard]] float CombustionThreshold() const;
	/// 2 Tc, the hottest a burning object gets
	[[nodiscard]] float MaxBurnTemperature() const { return 2.0f * CombustionThreshold(); }
	/// The ambient temperature at the object's position
	[[nodiscard]] float Ambient() const;
	/// max(heat capacity, 1)
	[[nodiscard]] float Capacity() const;
	/// T >= Tc
	[[nodiscard]] bool IsOnFire() const;
	/// T >= 100 or T >= Tc
	[[nodiscard]] bool IsAboveReactionTemperature() const;
	/// (T - 0.8 Tc) / (2 Tc - 0.8 Tc), at most 2 x life, in 0..1
	[[nodiscard]] float FireFraction() const;
	/// 1.25 x the object's fire radius x the fraction
	[[nodiscard]] float FireRadius() const;
	/// 1.25 x its fire radius
	[[nodiscard]] float MaxFireRadius() const;
	/// min(fire radius, max) + 1
	[[nodiscard]] float SafeFireRadius() const;
	/// The flame height, 1.25 x height x (T - Tamb) / (2 Tc - Tamb) in 0..1
	[[nodiscard]] float FlameHeight() const;
	/// (T - Tamb) x capacity
	[[nodiscard]] float HeatContent() const;
	/// T += q / cap, but at most dTmax (by magnitude)
	void AddHeat(float heat, float maxChange);
	/// The sum of the 2D radii of the group's burning objects
	[[nodiscard]] float GroupBurningRadius() const;
	/// The group's highest burningPriority
	[[nodiscard]] float GroupBurningPriority() const;
	/// The group's fire nearest to `position` (within its safe radius or object radius) that is above the
	/// reaction temperature; nullptr if none
	[[nodiscard]] FireEffect* NearestFireToFight(const glm::vec3& position) const;
};

/// `other` (with its group) joins `self`'s group, right after it
void AddToFireGroup(FireEffect& self, FireEffect& other);

/// The object's fire, or nullptr
[[nodiscard]] FireEffect* Find(entt::entity object);
/// A fire by its handle while it is in the list; nullptr once deleted
[[nodiscard]] FireEffect* Get(uint32_t id);

/// None if the object refuses a burn (a BURN 100 effect), can't be set on fire, has Tc 0 or is being deleted
/// (ecs::IsAvailable). Its T starts at the object's temperature, it joins the head of the list as the
/// root of its own group, gets its graphic and the object StartOnFire.
FireEffect* Create(entt::entity object, std::optional<PlayerNames> player, entt::entity source);
/// The fire is taken out of its group and the list, and freed at the end of the turn
void ToBeDeleted(FireEffect& fire);

/// The fire's T, else the ambient temperature
[[nodiscard]] float GetTemperature(entt::entity object);
/// Whether the object has a fire that burns
[[nodiscard]] bool IsOnFire(entt::entity object);
/// The ambient temperature: 24.7 everywhere
[[nodiscard]] float AmbientTemperature(const glm::vec3& position);

/// A new fire when hotter than the object, then
/// T = t; an existing fire just takes t (also lower)
void SetTemperature(entt::entity object, float temperature, entt::entity source);
/// T = 2 Tc x speed + Tc
void SetOnFire(entt::entity object, float speed);

/// The fire part of an object's damage effect: a burn
/// drives T towards ambient + burn, T += min(10 dT / cap, dT); a negative one (water, beating) cools it. A villager not
/// yet on fire runs (SetupOnFire).
void ApplyEffectToFireEffectIfNecessary(entt::entity object, const effects::EffectValues& values);

/// For an object in the hand inside the holder's influence: every fire in the map cell of the object's fire centre
/// heats it
void CheckToSeeIfObjectIsNearOnFireObject(entt::entity object);
/// The fire passes to a new object (a rock split in two): same group, T = Tprev = max of both
void CopyFire(entt::entity from, entt::entity to);
/// The fire moves to another object (Tree -> DeadTree), with its reaction
void MoveFire(entt::entity from, entt::entity to);
/// The object was picked up or put into physics: out of its group, no
/// REACT_TO_FIRE; in the hand it is REACT_TO_BURNING_OBJECT_IN_HAND
void StartedMoving(entt::entity object, bool inHand);
/// The object left the hand
void SetOutMagicHand(entt::entity object);

/// Every fire's Process, once per game turn
void ProcessList();
/// Every fire, newest first (the list order)
[[nodiscard]] const std::vector<FireEffect*>& All();
/// A land is loaded: no fires, no sound slots
void Clear();

/// OPENBLACK_FIRE_TRACE=1
[[nodiscard]] bool TraceEnabled();
} // namespace openblack::ecs::fire
