/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <optional>
#include <span>
#include <vector>

#include "Enums.h"
#include "Magic/MagicTables.h"

// The miracles a town offers. A town holds a set of magic types, given by its land's script. Its village centre shows
// each miracle on one of up to six stone hands, a seed spinning above the palm. Its worship site keeps an icon for every
// miracle its towns offer, where the miracle is charged from the site's prayer power. Tapping a stone hand charges its
// miracle at the site; once charged, the miracle jumps into the hand. Pure rules, free of the world.

namespace openblack::magic::spell_icons
{

/// A village centre has six stone hands, standing at the first six points of its model
inline constexpr size_t k_CentreHands = 6;
/// A worship site's icons stand at the eleventh to sixteenth points of its model, then further out
inline constexpr uint32_t k_FirstSitePoint = 10;
inline constexpr uint32_t k_SitePoints = 6;
/// When every place is taken the icons are set this much further out from the site, and again, up to the limit
inline constexpr float k_SitePushStep = 7.5f;
/// Two icons nearer than this are in the same place
inline constexpr float k_SamePlace = 1.0f;
/// A hand's seed spins this far above the first point of the hand's model
inline constexpr float k_SeedAbovePalm = 1.0f;
/// The power-up levels a seed can have past its plain miracle
inline constexpr size_t k_PowerUpLevels = 3;

/// The magic types a town holds, by magic type
using HeldMagic = std::array<bool, k_MagicTypeCount>;

/// What a magic type is to a town's hands: the seed that shows it, and the power-up level it is of that seed
/// (k_BasePowerUpLevel for its plain miracle)
struct SeedLevel
{
	SpellSeedType seed {SpellSeedType::None};
	int powerUp {k_BasePowerUpLevel};
};

/// The town comes to hold a magic type: whether it is new to it. Types beyond the game's, and none, are never held.
bool Hold(HeldMagic& held, MagicType type);
/// The town no longer holds a magic type: whether it held it
bool Release(HeldMagic& held, MagicType type);

/// The hand a new miracle goes on: the first empty one whose place the centre's model has, none when every one is taken
[[nodiscard]] std::optional<size_t> FreeHand(std::span<const SpellSeedType, k_CentreHands> shown, size_t places);

/// The seeds the centre's hands show for what the town holds, in the order of the magic types, each once: a power-up
/// level shows its seed as its plain miracle does. As many as there are places, and no more than six.
template <typename SeedOf>
[[nodiscard]] std::vector<SpellSeedType> HandsFor(const HeldMagic& held, SeedOf&& seedOf, size_t places);

/// The power-up level the seed above a hand shows: the highest enabled, k_BasePowerUpLevel for none
[[nodiscard]] int ShownPowerUp(const std::array<bool, k_PowerUpLevels>& powerUps);

/// The miracles a worship site keeps icons for: every seed any of its towns' hands show, each once, in the order the
/// towns came and their hands stand
[[nodiscard]] std::vector<SpellSeedType> SiteSeeds(std::span<const std::vector<SpellSeedType>> townSeeds);

/// A worship site's icon, as a tap on it sees it
struct IconCharge
{
	/// It is charging a miracle for a hand
	bool charging {false};
	/// The hand charging it is the one tapping
	bool chargedByThisHand {false};
	/// A seed made from it is out, in a hand or cast
	bool seedOut {false};
	/// What it has stored, and what its miracle needs
	float store {0.0f};
	float required {0.0f};
};

/// What a tap does to an icon
enum class TapResult : uint8_t
{
	/// Nothing: the tap does not count
	Nothing,
	/// Its charged miracle goes into the hand
	PutInHand,
	/// Its charging is called off and its prayer power goes back to the site
	Cancelled,
	/// It starts charging its miracle for the hand
	StartedCharge,
};

/// What is still needed to charge it full
[[nodiscard]] float Need(const IconCharge& icon);

/// Whether its miracle may go into a hand: charged full, no seed of it out, and the hand ready for it
[[nodiscard]] bool ReadyForHand(const IconCharge& icon, bool handReady);

/// Whether a tap may start it charging: not charging, no seed of it out, its miracle enabled for the hand's player, and
/// either on a worship site or with something stored
[[nodiscard]] bool MayStartCharge(const IconCharge& icon, bool magicEnabled, bool onSite);

/// A tap on it: its charged miracle into the hand if it can go; else its charging called off if the tapping hand charges
/// it; else, with the hand ready, its charging started
[[nodiscard]] TapResult Tap(const IconCharge& icon, bool handReady, bool magicEnabled, bool onSite);

/// The click a counted tap on a village centre's hand makes is played faster the further along the hand stands: its
/// playback rate in percent, the sixth hand's for any after it
[[nodiscard]] uint32_t ClickPitch(size_t hand);

/// The announcer's sample naming the miracle that has just been charged full at an icon
[[nodiscard]] int FullyChargedVoice(SpellSeedType seed);

template <typename SeedOf>
std::vector<SpellSeedType> HandsFor(const HeldMagic& held, SeedOf&& seedOf, size_t places)
{
	std::vector<SpellSeedType> hands;
	const size_t limit = places < k_CentreHands ? places : k_CentreHands;
	for (size_t i = 0; i < held.size(); ++i)
	{
		if (!held.at(i))
		{
			continue;
		}
		const std::optional<SeedLevel> level = seedOf(static_cast<MagicType>(i));
		if (!level.has_value() || level->seed == SpellSeedType::None)
		{
			continue;
		}
		if (std::ranges::find(hands, level->seed) == hands.end() && hands.size() < limit)
		{
			hands.push_back(level->seed);
		}
	}
	return hands;
}

} // namespace openblack::magic::spell_icons
