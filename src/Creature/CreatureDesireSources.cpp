/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureDesireSources.h"

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>

#include "Common/X87Vector.h"

using namespace openblack;
using namespace openblack::creature_desire_sources;

namespace
{
/// The game multiplies by these, not divides
constexpr float k_PerMostHeldRadius = 1.0f / k_MostHeldRadius;
constexpr float k_PerMostHandMovement = 1.0f / k_MostHandMovement;
/// Hang around at home falls to nothing a hundred units from home
constexpr float k_HangAroundFalloff = 0.01f;
/// The sources to get high and of mental illness count whole hundreds of magic mushrooms eaten
constexpr uint32_t k_MushroomsPerStep = 100;
/// A young creature finds the player more interesting: four times before this age, twice before the second
constexpr uint32_t k_YoungAge = 10;
constexpr uint32_t k_GrowingAge = 40;
constexpr float k_YoungInterest = 4.0f;
constexpr float k_GrowingInterest = 2.0f;

[[nodiscard]] float Clamp01(float value)
{
	return std::clamp(value, 0.0f, 1.0f);
}
} // namespace

float creature_desire_sources::SpreadTrait(float base, float spread, const UpTo& upTo)
{
	const auto draw = [&]() { return base + upTo(2.0f * spread) - spread; };
	// The upper limit is tested only when a first draw is above 0
	if (draw() > 0.0f && draw() >= 1.0f)
	{
		return 1.0f;
	}
	if (draw() <= 0.0f)
	{
		return 0.0f;
	}
	return draw();
}

InnateTraits creature_desire_sources::SpreadTraits(const InnateTraits& species, const UpTo& upTo)
{
	// In the order the game draws them
	InnateTraits traits;
	traits.kindness = SpreadTrait(species.kindness, k_TraitSpread, upTo);
	traits.aggression = SpreadTrait(species.aggression, k_TraitSpread, upTo);
	traits.lethargy = SpreadTrait(species.lethargy, k_TraitSpread, upTo);
	traits.friendliness = SpreadTrait(species.friendliness, k_TraitSpread, upTo);
	traits.communicativeness = SpreadTrait(species.communicativeness, k_TraitSpread, upTo);
	traits.sixth = SpreadTrait(species.sixth, k_SixthTraitSpread, upTo);
	traits.objectsLookWeight = SpreadTrait(species.objectsLookWeight, k_LookWeightSpread, upTo);
	traits.mountainsLookWeight = SpreadTrait(species.mountainsLookWeight, k_LookWeightSpread, upTo);
	traits.waterLookWeight = SpreadTrait(species.waterLookWeight, k_LookWeightSpread, upTo);
	return traits;
}

float creature_desire_sources::SpreadSource(float value, float spread, const UpTo& upTo)
{
	// The game takes the spread off the draw first, then adds the value
	return Clamp01((upTo(2.0f * spread) - spread) + value);
}

float creature_desire_sources::SpreadIncreaseSeconds(float seconds, const UpTo& upTo)
{
	const auto spread = seconds * k_IncreaseSpread;
	return seconds + (upTo(2.0f * spread) - spread);
}

void creature_desire_sources::CountTurn(Attention& attention, bool lookedAt)
{
	if (lookedAt)
	{
		++attention.turnsWatched;
		attention.turnsIgnored = 0;
	}
	else
	{
		++attention.turnsIgnored;
		attention.turnsWatched = 0;
	}
	++attention.turnsSinceInteraction;
}

float creature_desire_sources::CountSource(uint32_t turns, uint32_t seconds, uint32_t turnsPerSecond)
{
	if (turnsPerSecond == 0)
	{
		return 0.0f;
	}
	const auto wholeSeconds = static_cast<float>(turns / turnsPerSecond);
	return Clamp01(wholeSeconds * (1.0f / static_cast<float>(seconds)));
}

uint32_t creature_desire_sources::TurnsForSource(float value, uint32_t seconds, uint32_t turnsPerSecond)
{
	const auto turns = static_cast<float>(turnsPerSecond) * value * static_cast<float>(seconds);
	return turns > 0.0f ? static_cast<uint32_t>(turns) : 0u;
}

float creature_desire_sources::InterestNow(PlayerInterest& interest, const std::optional<glm::vec3>& hand,
                                           std::optional<float> heldRadius)
{
	if (!hand.has_value())
	{
		return 0.0f;
	}
	if (!interest.lastHand.has_value())
	{
		interest.lastHand = hand;
	}
	interest.handMovement = std::clamp(interest.handMovement + x87::Distance<x87::SumOrder::Zyx>(*hand, *interest.lastHand),
	                                   0.0f, k_MostHandMovement);
	interest.handMovement *= k_HandMovementFade;
	interest.lastHand = hand;
	const auto held = heldRadius.has_value() ? std::clamp(*heldRadius, 0.0f, k_MostHeldRadius) * k_PerMostHeldRadius : 0.0f;
	return Clamp01(interest.handMovement * k_PerMostHandMovement + held);
}

void creature_desire_sources::UpdateInterest(PlayerInterest& interest, float now)
{
	interest.value = now > interest.value ? now : interest.value + (now - interest.value) * 0.5f;
}

std::optional<float> creature_desire_sources::Read(uint32_t type, const State& state)
{
	switch (type)
	{
	case types::k_ImpressFromSeeingThings:
		return state.impressFromSeeing;
	case types::k_InnateKindness:
		return state.innate != nullptr ? std::optional(state.innate->kindness) : std::nullopt;
	case types::k_InnateAggression:
		return state.innate != nullptr ? std::optional(state.innate->aggression) : std::nullopt;
	case types::k_InnateLethargy:
		return state.innate != nullptr ? std::optional(state.innate->lethargy) : std::nullopt;
	case types::k_InnateFriendliness:
		return state.innate != nullptr ? std::optional(state.innate->friendliness) : std::nullopt;
	case types::k_InnateCommunicativeness:
		return state.innate != nullptr ? std::optional(state.innate->communicativeness) : std::nullopt;
	case types::k_IdleWithPlayer:
	{
		if (state.computerPlayer)
		{
			return 0.0f;
		}
		const auto young = state.age < k_YoungAge ? k_YoungInterest : (state.age < k_GrowingAge ? k_GrowingInterest : 1.0f);
		return Clamp01(young * state.interest);
	}
	case types::k_Loneliness:
		return state.attention != nullptr
		           ? std::optional(CountSource(state.attention->turnsIgnored, k_LonelySeconds, state.turnsPerSecond))
		           : std::nullopt;
	case types::k_LackOfInteraction:
		return state.attention != nullptr ? std::optional(CountSource(state.attention->turnsSinceInteraction,
		                                                              k_UninterestedSeconds, state.turnsPerSecond))
		                                  : std::nullopt;
	case types::k_GoHome:
	{
		if (!state.homeDistance.has_value())
		{
			return std::nullopt;
		}
		// With no distance for its stage, it wants to go home fully when away from home, and not at all at home
		if (state.goHomeDistance == 0.0f)
		{
			return *state.homeDistance > 0.0f ? 1.0f : 0.0f;
		}
		return Clamp01(*state.homeDistance / state.goHomeDistance);
	}
	case types::k_HangAroundAtHome:
		return state.homeDistance.has_value() ? std::optional(1.0f - Clamp01(*state.homeDistance * k_HangAroundFalloff))
		                                      : std::nullopt;
	case types::k_GetHigh:
	case types::k_MentalIllness:
		return static_cast<float>(state.mushroomsEaten / k_MushroomsPerStep);
	default:
		return std::nullopt;
	}
}

void creature_desire_sources::Write(uint32_t type, float value, const Written& written)
{
	auto* innate = written.innate;
	switch (type)
	{
	case types::k_InnateKindness:
		if (innate != nullptr)
		{
			innate->kindness = value;
		}
		break;
	case types::k_InnateAggression:
		if (innate != nullptr)
		{
			innate->aggression = value;
		}
		break;
	case types::k_InnateLethargy:
		if (innate != nullptr)
		{
			innate->lethargy = value;
		}
		break;
	case types::k_InnateFriendliness:
		if (innate != nullptr)
		{
			innate->friendliness = value;
		}
		break;
	case types::k_InnateCommunicativeness:
		if (innate != nullptr)
		{
			innate->communicativeness = value;
		}
		break;
	case types::k_IdleWithPlayer:
		if (written.interest != nullptr)
		{
			written.interest->value = 0.0f;
		}
		break;
	case types::k_Loneliness:
		if (written.attention != nullptr)
		{
			written.attention->turnsIgnored = TurnsForSource(value, k_LonelySeconds, written.turnsPerSecond);
		}
		break;
	case types::k_LackOfInteraction:
		if (written.attention != nullptr)
		{
			written.attention->turnsSinceInteraction = TurnsForSource(value, k_UninterestedSeconds, written.turnsPerSecond);
		}
		break;
	case types::k_GetHigh:
		if (written.mushroomsEaten != nullptr)
		{
			*written.mushroomsEaten = k_MushroomsPerStep;
		}
		break;
	default:
		break;
	}
}

bool creature_desire_sources::SightingStirsSources(uint32_t turn, uint32_t lastTurn)
{
	constexpr uint32_t k_SightingSourceTurns = 200;
	return turn - lastTurn > k_SightingSourceTurns;
}

float creature_desire_sources::TownSightingAmount(uint32_t turnsPerSecond)
{
	// The game cuts the number of turns down to a whole number
	const auto turns = static_cast<uint32_t>(static_cast<float>(turnsPerSecond) * k_TownSightingSeconds);
	return turns > 0 ? 1.0f / static_cast<float>(turns) : 0.0f;
}

bool creature_desire_sources::TownSightingStirs(bool scriptControlled, bool townWantsSomething)
{
	return !scriptControlled && townWantsSomething;
}

float creature_desire_sources::ImpressAfterSighting(float impress)
{
	return Clamp01(impress + k_ImpressPerSighting);
}
