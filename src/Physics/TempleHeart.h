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
#include <span>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

/// What a temple's heart does when something thrown strikes it: it passes the blow on to one of its player's buildings
/// or homeless people, or takes the harm itself
namespace openblack::physics::temple_heart
{

/// A building the blow might pass to
struct Building
{
	entt::entity entity {entt::null};
	/// Still in the world to be struck
	bool available {false};
	float life {0.0f};
	float built {0.0f};
	bool field {false};
	bool footballPitch {false};
};
/// A homeless villager the blow might pass to
struct Homeless
{
	entt::entity entity {entt::null};
	/// Still in the world and not on its way to dying
	bool available {false};
};
/// One of the heart's player's towns: its buildings, the newest first, and its homeless people, the newest first
struct Town
{
	std::vector<Building> buildings;
	std::vector<Homeless> homeless;
};

/// A building with more life than this takes the blow at once
inline constexpr float k_SoundLife = 0.25f;

enum class TargetKind : uint8_t
{
	/// Nothing to pass it to: the heart itself
	Heart,
	Building,
	Villager,
};
struct Target
{
	TargetKind kind {TargetKind::Heart};
	entt::entity entity {entt::null};
};
/// Over the player's towns in the order the player gained them: the first building standing, built and alive that isn't a
/// field or a football pitch, at once when its life is over a quarter; else, after every town, the first such building
/// found; else the first available homeless villager of any town; else the heart
[[nodiscard]] Target Choose(std::span<const Town> towns);

/// The harm a striking body does the heart: its speed times its mass times a fraction, at most a fifth
inline constexpr float k_HarmPerMomentum = 0.000005f;
inline constexpr float k_MostHarm = 0.2f;
[[nodiscard]] float Harm(glm::vec3 velocity, float mass);

/// A villager the blow passes to is flung straight up, spinning, by no one
inline constexpr glm::vec3 k_VillagerLaunch {0.0f, 30.0f, 0.0f};
inline constexpr glm::vec3 k_VillagerLaunchSpin {1.0f, 0.0f, 1.0f};

/// The heart's beam at what it passes blows on to: the thing it last beamed at and the turn it last beamed
struct Beam
{
	entt::entity target {entt::null};
	uint32_t turn {0};
};

/// The turns between the heart's beams: two seconds' worth
[[nodiscard]] uint32_t BeamInterval(uint32_t millisecondsPerTurn);

/// A blow that breaks buildings passed on to a target: a new target starts the wait afresh, and a beam is due once more
/// than the interval has gone by since the last; due, it is the beam's turn now
[[nodiscard]] bool BeamAtTargetDue(Beam& beam, entt::entity target, uint32_t turn, uint32_t interval);
/// A blow that breaks buildings the heart takes itself: it forgets what it last beamed at, starting the wait afresh, and
/// beams at a point on itself on the same wait
[[nodiscard]] bool BeamAtItselfDue(Beam& beam, uint32_t turn, uint32_t interval);

/// The heart's beam sound is one of five in turn
inline constexpr uint32_t k_BeamSounds = 5;

/// A heart that is hurt, with less than all its life, beams every turn it is due: the less life, the oftener. The wait
/// between beams is 3 seconds at nearly all its life, down to 0.4 seconds at none, in whole turns.
inline constexpr float k_DamageWaitFull = 3.0f;
inline constexpr float k_DamageWaitNone = 0.4f;
/// It sounds a beam sound only once it has this much life or less
inline constexpr float k_DamageSoundLife = 0.6f;

/// The life the damage effect works from, none when the heart is whole and the effect is not made at all
[[nodiscard]] std::optional<float> DamageEffectLife(float life);
/// The turns between a hurt heart's beams, by its life from the damage effect's: whole turns a second, times the wait
[[nodiscard]] uint32_t DamageInterval(uint32_t millisecondsPerTurn, float life);
/// The turn after the interval, once the last beam is more than that many turns gone. Due, it is the beam's turn now.
[[nodiscard]] bool DamageBeamDue(uint32_t& lastTurn, uint32_t turn, uint32_t interval);
/// Whether a hurt heart's beam sounds, by its life from the damage effect's
[[nodiscard]] bool DamageBeamSounds(float life);
/// The sample of the next damage beam sound, 206 to 210 in turn, and the counter moved on. The counter is the damage
/// effect's own, apart from the one the beams for blows that break buildings go round on.
[[nodiscard]] uint32_t NextDamageSample(uint32_t& counter);

} // namespace openblack::physics::temple_heart
