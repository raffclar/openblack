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

// The emergencies that a creature looks at each turn before it decides what to do: low life, a fire on its body, and a
// fire in the nearest town

namespace openblack::creature_emergencies
{

/// Below this life, the creature makes the desire to restore its health dominant
inline constexpr float k_HurtLife = 0.4f;
/// The desire to restore its health stays dominant for this many seconds
inline constexpr float k_RestoreHealthSeconds = 10.0f;
/// The creature uses the water miracle only when its sightings of the miracle are more than this part of the sightings
/// that it needs
inline constexpr float k_WaterMiracleShare = 0.6f;
/// The creature looks for a fire only in a town nearer than this distance
inline constexpr float k_TownReach = 150.0f;

/// Whether the creature looks at its emergencies this turn
struct Gates
{
	/// An emergency started and the creature did not stop what it does since then
	bool emergencyRunning {false};
	bool scripted {false};
	/// An order forced the plan that it does
	bool planForced {false};
	bool fighting {false};
};
[[nodiscard]] bool LooksAtEmergencies(const Gates& gates);

/// Whether the creature with this life, and not on fire, makes the desire to restore its health dominant
[[nodiscard]] bool IsHurt(float life);

/// What the creature does when it is on fire
enum class OwnFire : uint8_t
{
	/// It casts the water miracle on itself
	CastWater,
	/// It takes off the leash and goes to drink from the sea
	DrinkFromTheSea,
};
/// The creature casts the water miracle on itself only when its sightings of the miracle are more than a part of those
/// that it needs, and its body can pay for the miracle
[[nodiscard]] OwnFire ChooseOwnFire(float seen, float needed, bool canPay);

/// What the creature knows about the water miracle and its desires, when a thing burns in the nearest town
struct TownFireFacts
{
	bool knowsWater {false};
	float seen {0.0f};
	float needed {0.0f};
	bool canPay {false};
	bool compassionActivated {false};
	bool compassionSuppressed {false};
	float compassion {0.0f};
	float anger {0.0f};
};
/// Whether the creature goes to put out a fire in the nearest town with the water miracle. It must know the miracle and
/// have more than a part of the sightings that it needs, more than all of them counting as all. Its body must pay for
/// the miracle. Compassion must be active, not suppressed, and stronger than anger.
[[nodiscard]] bool PutsOutTownFire(const TownFireFacts& facts);

/// A building of a town, as the creature looks for a fire
struct Building
{
	bool onFire {false};
	/// A storage pit that is built, or the town centre
	bool comesFirst {false};
};
/// The burning building that the creature goes to, from the town's buildings in their order (the newest first). The
/// first burning building that comes first wins. Else the last burning building wins.
[[nodiscard]] std::optional<size_t> PickBurningBuilding(std::span<const Building> buildings);

/// Whether the run speed that the creature takes when it is on fire stays. A creature of a player that is not the
/// computer goes back to its walk speed as it stops what it did, so it walks.
[[nodiscard]] bool KeepsRunSpeed(bool hasPlayer, bool computerPlayer);

} // namespace openblack::creature_emergencies
