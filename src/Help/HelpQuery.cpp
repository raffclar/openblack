/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HelpQuery.h"

using namespace openblack;
using namespace openblack::help_query;
namespace messages = openblack::help_messages;

Answer help_query::About(const Thing& thing)
{
	if (thing.person.has_value())
	{
		const auto& person = *thing.person;
		if (person.scriptControlled)
		{
			return SayTexts {.texts = {.first = k_ScriptControlledText, .last = k_ScriptControlledText}};
		}
		if (!person.workingDisciple)
		{
			if (person.dead)
			{
				return SayTexts {.texts = {.first = k_DeadText, .last = k_DeadText}};
			}
			if (person.child)
			{
				return SaySet {messages::k_Child};
			}
			if (person.vagrant)
			{
				return SaySet {messages::k_Vagrant};
			}
			if (!person.players)
			{
				return SaySet {messages::k_NonBeliever};
			}
		}
	}
	else if (thing.othersWonder)
	{
		return SaySet {messages::k_OthersWonder};
	}

	if (thing.texts.first == 0)
	{
		return SaySet {messages::k_NotInterested};
	}
	switch (thing.role)
	{
	case Role::Town:
		return SayTexts {.texts = thing.texts, .script = messages::Script::Town};
	case Role::TownNeed:
		if (!thing.rawDesire.has_value())
		{
			return SayTexts {.texts = {.first = thing.needText, .last = thing.needText}};
		}
		return SayTexts {.texts = thing.texts,
		                 .script = messages::Script::TownNeeds,
		                 .townNeed = std::pair {thing.needText, messages::TownNeed(*thing.rawDesire)}};
	case Role::Plain:
	default:
		return SayTexts {.texts = thing.texts};
	}
}

Answer help_query::AboutGround(const Ground& ground)
{
	if (!ground.hasPoint)
	{
		return SaySet {messages::k_NotInterested};
	}
	if (ground.fishFarm)
	{
		return SayTexts {.texts = {.first = k_FishFarmText, .last = k_FishFarmText}};
	}
	if (static_cast<double>(ground.altitude) <= k_ShallowWaterHeight && ground.onLand)
	{
		return SayTexts {.texts = {.first = k_ShallowWaterText, .last = k_ShallowWaterText}};
	}
	if (ground.material.first != 0)
	{
		return SayTexts {.texts = ground.material};
	}
	return SaySet {messages::k_NotInterested};
}

Texts help_query::VillagerTexts(uint32_t activityText, std::optional<VillagerDisciple> disciple, Texts kind)
{
	if (activityText != 0)
	{
		return {.first = activityText, .last = activityText};
	}
	if (disciple.has_value())
	{
		// The disciples' texts follow one another in the order of their kinds, the first standing for none; a craftsman
		// has the first and a trader the trader's own; the rest have their kind's
		constexpr uint32_t k_FirstDiscipleText = 3129;
		constexpr uint32_t k_TraderText = 2770;
		const auto type = static_cast<uint32_t>(*disciple);
		if (type < static_cast<uint32_t>(VillagerDisciple::Craftsman))
		{
			return {.first = k_FirstDiscipleText + type, .last = k_FirstDiscipleText + type};
		}
		if (*disciple == VillagerDisciple::Trader)
		{
			return {.first = k_TraderText, .last = k_TraderText};
		}
		if (*disciple == VillagerDisciple::Craftsman)
		{
			return {.first = k_FirstDiscipleText, .last = k_FirstDiscipleText};
		}
	}
	return kind;
}

Texts help_query::CreatureTexts(Texts species, uint32_t speciesRow, bool players)
{
	constexpr uint32_t k_ChimpRow = 12;
	constexpr uint32_t k_OgreRow = 13;
	auto text = species.first;
	if (speciesRow != k_ChimpRow && speciesRow != k_OgreRow && !players)
	{
		++text;
	}
	return {.first = text, .last = text};
}

Texts help_query::FeatureTexts(FeatureInfo type, float built, Texts kind)
{
	constexpr uint32_t k_ArkText = 2865;
	if (type == FeatureInfo::ArkDryDock && built >= 1.0f)
	{
		return {.first = k_ArkText, .last = k_ArkText};
	}
	return kind;
}

Texts help_query::FoodPileTexts(bool inFinishedStore, Texts kind)
{
	return inFinishedStore ? kind : Texts {.first = k_FoodPileText, .last = k_FoodPileText};
}

Texts help_query::SeedTexts(Texts miracle, Texts object)
{
	return {.first = miracle.first != 0 ? miracle.first : object.first, .last = miracle.last != 0 ? miracle.last : object.last};
}
