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

#include <functional>
#include <optional>

#include "3D/AllMeshes.h"
#include "Enums.h"

// The land's birds: crows, doves, swallows, pigeons, seagulls and bats, and the doves or bats about a temple. They all
// fly as the flying flock miracle's doves do, a leader wandering legs about its flock's home and the rest following it,
// with the differences here. Pure functions, tested on their own.

namespace openblack::animals::birds
{

/// Every kind that flies: the land's birds, the miracles' doves and bats, and the table's unused rows of them
[[nodiscard]] bool IsBird(AnimalInfo type);
/// The land's own birds, which the land scripts and the temples make: not the miracles' doves and bats
[[nodiscard]] bool IsLandBird(AnimalInfo type);

/// The clip a land bird flies with, chosen afresh each time it changes what it does or how fast it goes: crows,
/// doves, pigeons and seagulls flap or glide by an even chance, a swallow flaps or glides calmly or erratically by a
/// third each, a bat always flaps. `roll(n)` is the game's random whole number below n.
[[nodiscard]] AnimId FlyingClip(AnimalInfo type, const std::function<uint32_t(uint32_t)>& roll);
/// The clip a land bird lies dead in
[[nodiscard]] std::optional<AnimId> DeadClip(AnimalInfo type);

/// How far a bird tilts into a turn, in radians, and the seconds it takes to tilt there
struct Bank
{
	float angle;
	float seconds;
};
/// The land's birds ease into their tilt over two seconds, the miracles' over half a second; both tilt half a radian
[[nodiscard]] Bank BankOf(AnimalInfo type);

/// A flock's leader keeps to its leg this many of its turns at most (its kind's stay time) before it picks a new one,
/// whether it got there or not
[[nodiscard]] bool LeaderPicksNewLeg(uint32_t turnsOnLeg, uint32_t stayTime);

/// How old a bird the land script makes is, in years: the age the script gives, or a random one when it gives none, 5
/// to 24 joining a flock and 5 to 44 on its own
[[nodiscard]] uint32_t ScriptBirdAge(uint32_t scriptAge, bool joinsFlock, const std::function<uint32_t(uint32_t)>& roll);
/// A land script's flock wanders this far from its home when the script gives no reach
inline constexpr float k_DefaultFlockReach = 80.0f;
/// The height a flock's leader picks its legs about: the flock's own when it has one (a temple's), else its kind's
[[nodiscard]] float BaseHeight(float flockHeight, float kindHeight);

// The temple's birds

/// The temple's flock is seen to every hundred turns of the game
inline constexpr uint32_t k_TempleFlockEvery = 100;
/// It wanders up to 30 m about the temple, its followers within 15 m of their leader
inline constexpr float k_TempleFlockReach = 30.0f;
inline constexpr float k_TempleFlockDistance = 15.0f;
/// Doves about a good temple, bats about an evil one: a player of no alignment at all counts as good
[[nodiscard]] AnimalInfo TempleBirdKind(float alignment);
/// How many birds a temple has: its most (the temple's table) by how far its player is from neutral, none while the
/// temple is unbuilt
[[nodiscard]] uint32_t TempleBirdCount(float alignment, uint32_t most, bool built);
/// The flock moves one bird towards its count each time it is seen to
enum class TempleFlockStep : uint8_t
{
	None,
	AddOne,
	RemoveOne,
};
[[nodiscard]] TempleFlockStep StepTowards(uint32_t members, uint32_t count);
/// The height the temple's birds fly about: as high as the temple's model stands, scaled, and 10 m more
[[nodiscard]] float TempleFlockHeight(float scale, float modelHeight);

} // namespace openblack::animals::birds
