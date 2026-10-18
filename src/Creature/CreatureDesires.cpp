/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureDesires.h"

#include <cmath>

#include <algorithm>

#include "Common/GUtilsDistance.h"
#include "Creature/CreatureDesireSources.h"
#include "Creature/CreatureLayers.h"

using namespace openblack;
using namespace openblack::creature_desires;
namespace animations = openblack::creature_layers::animations;

namespace
{
constexpr std::array<std::string_view, k_DesireCount> k_Names {
    "Impress",
    "Compassion",
    "Anger",
    "Play",
    "Hunger",
    "Fear",
    "Curiosity",
    "Poo",
    "Tiredness",
    "Idle with player",
    "Wanderlust",
    "Puke",
    "Build home",
    "Bring home",
    "Water",
    "Restore health",
    "Be friends",
    "Attract attention",
    "Manifest state",
    "Get warmer",
    "Get colder",
    "Scratch",
    "Run from player",
    "Rest",
    "Obey player",
    "Illness",
    "Obey creature",
    "Sadness",
    "Stay near home",
    "Tell player",
    "Play with player",
    "Tell creature",
    "Educate friend",
    "Follow player",
    "Get high",
    "Hang around home",
    "Mental illness",
    "Miss friend",
    "Look around",
    "Steal",
};

/// The action that shows each desire the creature can show
struct Emote
{
	Desire desire;
	size_t animation;
};
constexpr std::array<Emote, 20> k_Emotes {{
    {Desire::Impress, animations::k_Summon},
    {Desire::Compassion, animations::k_FeelingNice},
    {Desire::Anger, animations::k_Taunt},
    {Desire::Play, animations::k_FeelPlayful},
    {Desire::Hunger, animations::k_Hungry},
    {Desire::Fear, animations::k_Frightened},
    {Desire::Curiosity, animations::k_Confused},
    {Desire::Poo, animations::k_NeedAPoo},
    {Desire::Tiredness, animations::k_Tired},
    {Desire::IdleWithPlayer, animations::k_FeelPlayful},
    {Desire::Water, animations::k_Hot},
    {Desire::BeFriends, animations::k_Happy},
    {Desire::AttractAttention, animations::k_FriendlyWave},
    {Desire::GetWarmer, animations::k_Cold},
    {Desire::GetColder, animations::k_Hot},
    {Desire::Rest, animations::k_Tired},
    {Desire::Sadness, animations::k_Sad},
    {Desire::PlayWithPlayer, animations::k_FeelPlayful},
    {Desire::MissFriend, animations::k_Sad},
    {Desire::Steal, animations::k_FeelPlayful},
}};
} // namespace

std::string_view creature_desires::Name(Desire desire)
{
	const auto index = static_cast<size_t>(desire);
	return index < k_Names.size() ? k_Names.at(index) : "Unknown";
}

float creature_desires::Sigmoid(float threshold, float value)
{
	return gutils::CreatureSigmoidThreshold(threshold, value);
}

float creature_desires::SigmoidStep(float threshold, float value)
{
	return gutils::SigmoidThreshold(threshold, value);
}

Desires creature_desires::Create(const std::array<DesireSetup, k_DesireCount>& setup, const std::function<float(float)>& upTo)
{
	Desires desires;
	// The game draws the decay and the growing time of each desire first, then the sources of all the desires
	for (size_t i = 0; i < k_DesireCount; ++i)
	{
		const auto& from = setup.at(i);
		auto& desire = desires.desires.at(i);
		desire.max = from.max;
		desire.decay = upTo(from.decayMax - from.decayMin) + from.decayMin;
		desire.increaseSeconds = creature_desire_sources::SpreadIncreaseSeconds(from.increaseSeconds, upTo);
		desire.weight = from.weight;
	}
	for (size_t i = 0; i < k_DesireCount; ++i)
	{
		const auto& from = setup.at(i);
		auto& desire = desires.desires.at(i);
		for (size_t s = 0; s < from.sources.size(); ++s)
		{
			const auto& source = from.sources.at(s);
			if (source.type >= k_NoSource || desire.sources.size() >= k_MaxSources)
			{
				continue;
			}
			const auto spread = s < from.spreads.size() ? from.spreads.at(s) : DesireSetup::Spread {};
			auto& added = desire.sources.emplace_back(source);
			added.value = creature_desire_sources::SpreadSource(source.value, spread.value, upTo);
			added.threshold = creature_desire_sources::SpreadSource(source.threshold, spread.threshold, upTo);
		}
	}
	return desires;
}

void creature_desires::ActivateForPhase(Desires& desires, std::span<const PhaseDesires> phases, size_t phase)
{
	for (auto& desire : desires.desires)
	{
		desire.suppressedTurns = 0;
		desire.activated = false;
	}
	for (size_t i = 0; i <= phase && i < phases.size(); ++i)
	{
		for (const auto desire : phases[i].add)
		{
			desires[desire].activated = true;
		}
		for (const auto desire : phases[i].remove)
		{
			desires[desire].activated = false;
		}
	}
}

void creature_desires::UpdateSources(Desires& desires, const SourceReader& read)
{
	for (auto& desire : desires.desires)
	{
		for (auto& source : desire.sources)
		{
			if (const auto value = read ? read(source.type, desires) : std::nullopt)
			{
				source.value = *value;
			}
			source.value *= source.multiplier;
		}
	}
}

void creature_desires::UpdateDesires(Desires& desires, float turnsPerSecond)
{
	desires.sum = 0.0f;
	for (auto& desire : desires.desires)
	{
		if (!desire.activated)
		{
			continue;
		}
		float drive = 0.0f;
		for (auto& source : desire.sources)
		{
			const auto sourceDrive = Sigmoid(source.threshold, source.value);
			source.drive += sourceDrive;
			drive += sourceDrive;
		}
		const auto increase =
		    desire.increaseSeconds > 0.0f && turnsPerSecond > 0.0f ? drive / (turnsPerSecond * desire.increaseSeconds) : 0.0f;
		if (desire.suppressedTurns > 0)
		{
			--desire.suppressedTurns;
		}
		desire.value = increase > 0.0f && desire.suppressedTurns == 0 ? desire.value + increase : desire.value * desire.decay;
		desire.value = std::clamp(desire.value, 0.0f, std::max(desire.max, 0.0f));
		desires.sum += desire.value;
	}
}

void creature_desires::Suppress(Desires& desires, Desire desire, float seconds, float turnsPerSecond)
{
	auto& state = desires[desire];
	const auto turns = static_cast<uint32_t>(std::max(seconds * turnsPerSecond, 0.0f));
	state.suppressedTurns = std::max(state.suppressedTurns, turns);
}

void creature_desires::SetSource(Desires& desires, uint32_t type, float value)
{
	for (auto& desire : desires.desires)
	{
		for (auto& source : desire.sources)
		{
			if (source.type == type)
			{
				source.value = std::clamp(value, 0.0f, 1.0f);
			}
		}
	}
	desires.written.emplace_back(type, value);
}

void creature_desires::MakeSourcesFull(Desires& desires, Desire desire)
{
	// Setting a kind of source changes it in every desire, this one's own list included, so the list is copied first
	for (const auto& source : std::vector(desires[desire].sources))
	{
		if (std::ranges::find(k_SourcesNeverMadeFull, source.type) == k_SourcesNeverMadeFull.end())
		{
			SetSource(desires, source.type, 1.0f);
		}
	}
}

void creature_desires::SetValueFromScript(Desires& desires, Desire desire, float value)
{
	desires[desire].value = std::clamp(value, 0.0f, 1.0f);
	if (value == 1.0f)
	{
		MakeSourcesFull(desires, desire);
	}
}

void creature_desires::ChangeSource(Desires& desires, uint32_t type, float amount)
{
	for (auto& desire : desires.desires)
	{
		for (auto& source : desire.sources)
		{
			if (source.type == type)
			{
				source.value = std::clamp(source.value + amount, 0.0f, 1.0f);
			}
		}
	}
}

std::optional<Desire> creature_desires::DesireOfSource(const Desires& desires, uint32_t type)
{
	for (size_t i = 0; i < desires.desires.size(); ++i)
	{
		if (std::ranges::find(desires.desires.at(i).sources, type, &Source::type) != desires.desires.at(i).sources.end())
		{
			return static_cast<Desire>(i);
		}
	}
	return std::nullopt;
}

std::optional<float> creature_desires::SourceValue(const DesireState& desire, uint32_t type)
{
	const auto found = std::ranges::find(desire.sources, type, &Source::type);
	if (found == desire.sources.end())
	{
		return std::nullopt;
	}
	return found->value;
}

Desire creature_desires::Dominant(const Desires& desires)
{
	auto dominant = Desire::Impress;
	float most = 0.0f;
	for (size_t i = 0; i < k_DesireCount; ++i)
	{
		const auto& state = desires.desires.at(i);
		if (state.activated && state.value > most)
		{
			most = state.value;
			dominant = static_cast<Desire>(i);
		}
	}
	return dominant;
}

std::optional<size_t> creature_desires::EmoteFor(Desire desire)
{
	const auto found = std::ranges::find(k_Emotes, desire, &Emote::desire);
	if (found == k_Emotes.end())
	{
		return std::nullopt;
	}
	return found->animation;
}

std::optional<Desire> creature_desires::StrongestShowable(const Desires& desires)
{
	std::optional<Desire> strongest;
	float strongestValue = 0.0f;
	for (const auto& emote : k_Emotes)
	{
		const auto& state = desires[emote.desire];
		if (state.activated && state.value > strongestValue)
		{
			strongest = emote.desire;
			strongestValue = state.value;
		}
	}
	return strongest;
}

Communication creature_desires::Communicate(const Desires& desires, std::optional<uint32_t> turnsSinceFeedback,
                                            uint32_t turnsPerSecond, float attitudeToPlayer)
{
	const bool recentFeedback =
	    turnsSinceFeedback.has_value() && *turnsSinceFeedback < k_CommunicateFeedbackSeconds * turnsPerSecond;
	if (recentFeedback && desires[Desire::ManifestState].carriedOut == 0)
	{
		return {.animation = attitudeToPlayer < 0.0f ? animations::k_Sad : animations::k_Happy, .desire = std::nullopt};
	}
	const auto strongest = StrongestShowable(desires);
	if (!strongest.has_value())
	{
		return {};
	}
	return {.animation = EmoteFor(*strongest), .desire = strongest};
}

bool creature_desires::CountTowardsUrge(DesireState& desire)
{
	++desire.carriedOut;
	// The weight is cut down to a whole number of plans, read as unsigned as the game reads it
	const auto needed = static_cast<uint32_t>(static_cast<int64_t>(desire.weight));
	if (desire.carriedOut < needed)
	{
		return false;
	}
	desire.carriedOut = 0;
	return true;
}

void creature_desires::ClearSourcesAfterSatisfying(DesireState& desire)
{
	for (auto& source : desire.sources)
	{
		if (source.clearedWhenSatisfied)
		{
			source.value = 0.0f;
		}
	}
}

void creature_desires::LessenAfterAction(DesireState& desire, float multiplier, float floor)
{
	if (!desire.activated)
	{
		return;
	}
	desire.value *= multiplier;
	if (desire.value < floor)
	{
		desire.value = floor;
	}
	else if (desire.max < desire.value)
	{
		desire.value = desire.max;
	}
}
