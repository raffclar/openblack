/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpellIcons.h"

#include <algorithm>

namespace openblack::magic::spell_icons
{

namespace
{
[[nodiscard]] std::optional<size_t> IndexOf(MagicType type)
{
	const auto index = static_cast<size_t>(type);
	if (type == MagicType::None || index >= k_MagicTypeCount)
	{
		return std::nullopt;
	}
	return index;
}

/// The click's playback rate by hand; the last is never reached, as the hands after the sixth play the sixth's
constexpr std::array<uint32_t, 7> k_ClickPitches {100, 115, 130, 145, 155, 175, 190};
constexpr size_t k_LastClickHand = 5;

/// The announcer's sample for each seed, by seed; the lightning's, and the seeds with none of their own, are 8
constexpr int k_DefaultVoice = 8;
constexpr std::array<int, 30> k_FullyChargedVoices {
    14, // storm
    6,  // nature
    2,  // fire
    5,  // food
    13, // shield
    9,  // physical shield
    8,  // lightning
    7,  // heal
    17, // wood
    16, // water
    3,  // flying flock
    4,  // ground flock
    21, // freeze
    24, // small
    19, // big
    26, // weak
    25, // strong
    8,  // fat
    8,  // thin
    22, // invisible
    20, // compassion
    18, // angry
    8,  // hungry
    8,  // frightened
    8,  // tired
    8,  // ill
    8,  // thirsty
    23, // itchy
    15, // teleport
    1,  // beam explosion
};
} // namespace

bool Hold(HeldMagic& held, MagicType type)
{
	const auto index = IndexOf(type);
	if (!index.has_value() || held.at(*index))
	{
		return false;
	}
	held.at(*index) = true;
	return true;
}

bool Release(HeldMagic& held, MagicType type)
{
	const auto index = IndexOf(type);
	if (!index.has_value() || !held.at(*index))
	{
		return false;
	}
	held.at(*index) = false;
	return true;
}

std::optional<size_t> FreeHand(std::span<const SpellSeedType, k_CentreHands> shown, size_t places)
{
	for (size_t hand = 0; hand < shown.size(); ++hand)
	{
		if (shown[hand] == SpellSeedType::None)
		{
			// The first empty hand is the one; one the model has no place for takes nothing
			return hand < places ? std::optional<size_t>(hand) : std::nullopt;
		}
	}
	return std::nullopt;
}

int ShownPowerUp(const std::array<bool, k_PowerUpLevels>& powerUps)
{
	int shown = k_BasePowerUpLevel;
	for (size_t level = 0; level < powerUps.size(); ++level)
	{
		if (powerUps.at(level))
		{
			shown = static_cast<int>(level);
		}
	}
	return shown;
}

std::vector<SpellSeedType> SiteSeeds(std::span<const std::vector<SpellSeedType>> townSeeds)
{
	std::vector<SpellSeedType> seeds;
	for (const auto& town : townSeeds)
	{
		for (const auto seed : town)
		{
			if (seed != SpellSeedType::None && std::ranges::find(seeds, seed) == seeds.end())
			{
				seeds.push_back(seed);
			}
		}
	}
	return seeds;
}

float Need(const IconCharge& icon)
{
	return icon.required - icon.store;
}

bool ReadyForHand(const IconCharge& icon, bool handReady)
{
	return Need(icon) <= 0.0f && !icon.seedOut && handReady;
}

bool MayStartCharge(const IconCharge& icon, bool magicEnabled, bool onSite)
{
	return !icon.charging && !icon.seedOut && magicEnabled && (onSite || icon.store != 0.0f);
}

TapResult Tap(const IconCharge& icon, bool handReady, bool magicEnabled, bool onSite)
{
	if (ReadyForHand(icon, handReady))
	{
		return TapResult::PutInHand;
	}
	// Only the hand charging it calls it off; once charged and waiting for a hand no hand charges it
	if ((icon.charging || icon.store != 0.0f) && icon.chargedByThisHand)
	{
		return TapResult::Cancelled;
	}
	if (handReady && MayStartCharge(icon, magicEnabled, onSite))
	{
		return TapResult::StartedCharge;
	}
	return TapResult::Nothing;
}

uint32_t ClickPitch(size_t hand)
{
	return k_ClickPitches.at(std::min(hand, k_LastClickHand));
}

int FullyChargedVoice(SpellSeedType seed)
{
	const auto index = static_cast<size_t>(seed);
	return seed == SpellSeedType::None || index >= k_FullyChargedVoices.size() ? k_DefaultVoice
	                                                                           : k_FullyChargedVoices.at(index);
}

} // namespace openblack::magic::spell_icons
