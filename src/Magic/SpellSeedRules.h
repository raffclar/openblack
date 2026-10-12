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

#include <glm/vec2.hpp>

#include "Enums.h"

namespace openblack
{
struct GSpellSeedInfo;
}

// A miracle held in the hand as a seed: how the hand casts it, how charged it is and when it is ready. Pure functions of
// a seed's record and state.

namespace openblack::magic
{

/// How the hand casts a seed's miracle
enum class CastStyle : uint8_t
{
	/// Held: the miracle runs from the hand while the button is down (lightning, water, food, wood); let go, the seed
	/// keeps what prayer power is left for another go
	Held,
	/// Thrown: pressing readies it, letting go casts it at the hand's point with the hand's movement (fireball, storm,
	/// shield, the flocks)
	OnRelease,
	/// Placed: pressing casts it at the hand's point at once (heal, forest, teleport, the beam explosion)
	OnPress,
};
[[nodiscard]] CastStyle CastStyleOf(SpellCastType castType);
[[nodiscard]] CastStyle CastStyleOf(const GSpellSeedInfo& seed);

/// The prayer power a seed still needs to cast a miracle that costs so much to create
[[nodiscard]] float ChantNeeded(float costToCreate, float store);
/// How charged a seed is: its store over the cost, at most 1; fully charged for a miracle that costs nothing
[[nodiscard]] float SeedPower(float store, float costToCreate);
/// Whether a seed held for some turns is ready to cast: a seed from a worship site waits a delay, one that is ready at
/// once (from a dispenser) is always ready
[[nodiscard]] bool SeedReadyAfter(uint32_t turnsInHand, float turnSeconds, float delaySeconds);

/// A seed out of the hand that stays with the miracle it cast (the storms', the shields' and the forest's)
struct SeedWithSpell
{
	/// Its miracle runs from the hand, or the seed stays in the hand once cast
	bool castInHand {false};
	bool keptInHand {false};
	/// It is bound to the miracle it cast
	bool followsSpell {false};
	/// It was made at an icon of the player's worship
	bool hasIcon {false};
	/// Its miracle has closed down
	bool spellClosedDown {false};
};
/// Whether such a seed is shown over its miracle: one bound to its miracle and made at an icon of the player's worship,
/// until the miracle closes down. A seed whose miracle runs from the hand, or which stays in the hand, never is.
[[nodiscard]] bool ShownWithSpell(const SeedWithSpell& seed);
/// A miracle whose seed is shown closes down once the seed stands where its player has no influence
[[nodiscard]] bool ClosesOutOfInfluence(bool shown, bool inInfluence, bool closedDown);

/// Something standing in the shown seed's cell of the map, which the seed may sit on top of
struct ThingUnderSeed
{
	/// How far it stands from the seed across the land
	glm::vec2 offset {0.0f};
	/// Its radius across the land
	float radius {0.0f};
	/// How high its top stands over the land
	float top {0.0f};
	/// Living things, and things moving, are never sat on
	bool living {false};
	bool moving {false};
};
/// How high over the land the shown seed sits: on the top of the tallest of the still, non-living things whose circles
/// meet its own (counted when the square of the distance between them is less than the sum of the squares of their
/// radii), on the land with none
[[nodiscard]] float TopUnderSeed(float seedRadius, std::span<const ThingUnderSeed> things);
/// Before the forest miracle has planted its forest its seed is hidden this far under the land
inline constexpr float k_UnplantedForestSeedHeight = -5.0f;
/// The forest miracle's seed: over the tallest of its forest's trees once it has planted it, or over whatever it sits
/// on if that is taller; hidden under the land until then (no forest)
[[nodiscard]] float ForestSeedHeight(float topUnder, std::optional<float> tallestTree);

} // namespace openblack::magic
