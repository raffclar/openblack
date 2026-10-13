/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "GuidanceSystem.h"

#include <ctime>

#include <spdlog/spdlog.h>

#include "CHLApi.h"
#include "Common/GameRandom.h"
#include "ECS/Components/GuidanceHeard.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/HelpMessageSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/ThingOnScreen.h"
#include "Graphics/Moon.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// The help's category the advisors' remarks count under
constexpr uint32_t k_RemarkCategory = 8;
/// A blow leaving less of a building than this is remarked on
constexpr float k_WreckedShare = 0.4f;
constexpr size_t k_RemarksKept = 16;

uint32_t Turn()
{
	return Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
}

PlayerNames LocalPlayer()
{
	return Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
}

/// The help level the player chose, none while the scripts keep the help system off
uint32_t HelpLevel()
{
	constexpr uint32_t k_DefaultHelpLevel = 3;
	if (!Locator::chlapi::has_value())
	{
		return k_DefaultHelpLevel;
	}
	const auto& chlapi = Locator::chlapi::value();
	return chlapi.IsHelpSystemOn() ? chlapi.GetPlayerHelpLevel() : 0;
}

/// The first land of the story, where most guidance is kept quiet
bool FirstLandOfStory()
{
	const auto& context = Locator::entitiesRegistry::value().Context();
	return context.mapScriptGlobals.landNumber == 1 && !context.skirmish;
}

/// Whether the computer's clock says it is night now
bool ComputerNight()
{
	const std::time_t now = std::time(nullptr);
	std::tm local {};
#if defined(_WIN32)
	localtime_s(&local, &now);
#else
	localtime_r(&now, &local);
#endif
	return guidance::ClockSaysNight(local.tm_hour, local.tm_min);
}
} // namespace

GuidanceHeard* GuidanceSystem::Heard()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return nullptr;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto here = LocalPlayer();
	std::optional<entt::entity> player;
	registry.Each<const Player>([&](entt::entity entity, const Player& candidate) {
		if (candidate.name == here)
		{
			player = entity;
		}
	});
	if (!player.has_value())
	{
		return nullptr;
	}
	if (auto* heard = registry.TryGet<GuidanceHeard>(*player); heard != nullptr)
	{
		return heard;
	}
	return &registry.Assign<GuidanceHeard>(*player);
}

void GuidanceSystem::Reset()
{
	_stampsDue = true;
	_remarks.clear();
}

void GuidanceSystem::ProcessTurn()
{
	// A new land's guidance counts each kind as last heard a random while before now, and nothing said once yet
	if (_stampsDue && Locator::gameRandom::has_value())
	{
		if (auto* heard = Heard(); heard != nullptr)
		{
			auto& random = Locator::gameRandom::value();
			const auto turn = Turn();
			for (size_t kind = 0; kind < guidance::k_KindCount; ++kind)
			{
				const auto least = guidance::Rule(static_cast<guidance::Kind>(kind)).leastGap;
				heard->lastTurn.at(kind) = guidance::FirstStamp(turn, random.LocalRand(static_cast<int32_t>(least)));
			}
			heard->oneOffSaid = {};
			heard->alignmentDrift = 0.0f;
			_stampsDue = false;
		}
	}
	LookAtMoon();
}

bool GuidanceSystem::PlayNow(guidance::Kind kind)
{
	auto* heard = Heard();
	if (heard == nullptr || !Locator::gameRandom::has_value())
	{
		return false;
	}
	const auto& rule = guidance::Rule(kind);
	const bool firstLand = FirstLandOfStory();
	const auto level = HelpLevel();
	// Kept quiet, or not enough help: no wait is drawn
	if ((firstLand && !rule.onFirstLand) || level < rule.helpLevel)
	{
		return false;
	}
	auto& random = Locator::gameRandom::value();
	const auto spread = guidance::GapSpread(rule.leastGap, random.LocalFloatRand(1.0f));
	const auto wait = rule.leastGap + random.LocalRand(static_cast<int32_t>(spread));
	const auto since = Turn() - heard->lastTurn.at(static_cast<size_t>(kind));
	return guidance::MayPlay(rule, level, firstLand, wait, since);
}

bool GuidanceSystem::RemarkNow(guidance::Kind kind)
{
	return PlayNow(guidance::Kind::AnyRemark) && PlayNow(kind);
}

void GuidanceSystem::Say(uint32_t text, guidance::Kind kind)
{
	bool started = false;
	if (Locator::helpMessageSystem::has_value())
	{
		auto& messages = Locator::helpMessageSystem::value();
		const auto script = kind == guidance::Kind::NoText ? help_messages::Script::JustTalkWithNoText
		                                                   : help_messages::Script::JustTalkWithText;
		started = messages.RunMessage(text, text, script);
		messages.TriggerCategory(k_RemarkCategory);
	}
	// Counted as heard whether or not the advisors got to say it
	const auto turn = Turn();
	if (auto* heard = Heard(); heard != nullptr)
	{
		heard->lastTurn.at(static_cast<size_t>(guidance::Kind::AnyRemark)) = turn;
		heard->lastTurn.at(static_cast<size_t>(kind)) = turn;
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Guidance remark {} (kind {}){}", text, static_cast<int>(kind),
	                    started ? "" : ", not started");
	_remarks.push_back({.kind = kind, .text = text, .turn = turn, .started = started});
	if (_remarks.size() > k_RemarksKept)
	{
		_remarks.erase(_remarks.begin());
	}
}

void GuidanceSystem::SayRemark(guidance::Kind kind)
{
	const auto row = guidance::RemarkRow(kind);
	if (!row.has_value() || !Locator::infoConstants::has_value() || !Locator::gameRandom::has_value())
	{
		return;
	}
	const auto& texts = Locator::infoConstants::value().helpSpritesGuidance.at(*row).texts;
	const auto choices = guidance::RemarkChoices(texts);
	const auto pick = Locator::gameRandom::value().LocalRand(static_cast<int32_t>(choices));
	Say(texts.at(pick), kind);
}

void GuidanceSystem::TownAttacked(entt::entity town, PlayerNames attacker, bool byCreature)
{
	const auto here = LocalPlayer();
	// TODO(guidance-alerts): harm done by the player's creature is the creature's own remark, kept quiet on the first
	// land
	if (byCreature || attacker != here || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* owned = registry.Valid(town) ? registry.TryGet<const Town>(town) : nullptr;
	if (owned == nullptr || owned->owner == here || !RemarkNow(guidance::Kind::AttackingTown))
	{
		return;
	}
	// Only a town with people in it
	const auto* stats = registry.TryGet<const TownStats>(town);
	if (stats == nullptr || stats->adults + stats->children == 0)
	{
		return;
	}
	SayRemark(guidance::Kind::AttackingTown);
}

void GuidanceSystem::BuildingBroken(entt::entity building, std::optional<PlayerNames> player, float remaining)
{
	if (!(remaining < k_WreckedShare) || player != LocalPlayer() || !RemarkNow(guidance::Kind::DestroyBuilding))
	{
		return;
	}
	// Only a building the player can see
	const auto view = ecs::thing_on_screen::ScreenView();
	if (!view.has_value() || !Locator::entitiesRegistry::value().Valid(building) ||
	    !ecs::thing_on_screen::OnScreen(*view, building))
	{
		return;
	}
	SayRemark(guidance::Kind::DestroyBuilding);
}

void GuidanceSystem::AlignmentTurn(float change)
{
	auto* heard = Heard();
	if (heard == nullptr || !Locator::infoConstants::has_value() || !Locator::alignmentSystem::has_value())
	{
		return;
	}
	heard->alignmentDrift = guidance::Drift(heard->alignmentDrift, change);
	const float alignment = Locator::alignmentSystem::value().GetPlayerAlignment(LocalPlayer());
	const auto remark = guidance::AlignmentRemark(heard->alignmentDrift, alignment,
	                                              Locator::infoConstants::value().player.maxAlignmentChangePerGameTurn);
	if (remark.has_value() && RemarkNow(*remark))
	{
		SayRemark(*remark);
	}
}

void GuidanceSystem::SayOneOff(guidance::OneOff oneOff)
{
	auto* heard = Heard();
	if (heard == nullptr || !Locator::gameRandom::has_value())
	{
		return;
	}
	if (heard->oneOffSaid.at(static_cast<size_t>(oneOff)))
	{
		return;
	}
	const auto& rule = guidance::Rule(oneOff);
	if (guidance::OneOffSaid(Locator::gameRandom::value().LocalFloatRand(1.0f), rule.chance))
	{
		Say(rule.text, guidance::Kind::OneOff);
		// Looked up again: saying it may have added the component to the player
		if (auto* again = Heard(); again != nullptr)
		{
			again->oneOffSaid.at(static_cast<size_t>(oneOff)) = true;
		}
	}
}

void GuidanceSystem::EndWaits()
{
	// The first stamps would undo it
	_stampsDue = false;
	if (auto* heard = Heard(); heard != nullptr)
	{
		// Half the turns there can be away, so that the turns since it count from half the most there can be on
		constexpr uint32_t k_HalfOfAllTurns = 0x80000000u;
		heard->lastTurn.fill(Turn() + k_HalfOfAllTurns);
	}
}

void GuidanceSystem::LookAtMoon()
{
	// The moon is looked at once its wait is over and the game's sky is at night
	--_moonWait;
	if (_moonWait >= 1 || !Locator::skySystem::has_value() || !Locator::skySystem::value().GetClock().IsVisualNight())
	{
		return;
	}
	const auto look = guidance::LookAtMoon(graphics::moon::Phase(static_cast<int64_t>(std::time(nullptr))), ComputerNight());
	if (look.remark)
	{
		SayOneOff(guidance::OneOff::FullMoon);
	}
	_moonWait = look.wait;
}
