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

#include <algorithm>
#include <iterator>
#include <optional>
#include <vector>

#include <PackFile.h>
#include <glm/gtx/vec_swizzle.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "Audio/GameSoundEffects.h"
#include "Audio/HeaderOverrides.h"
#include "CHLApi.h"
#include "Camera/Camera.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "Common/MachineClock.h"
#include "ECS/BuildingSites.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/GuidanceHeard.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/ReportedHand.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownAggression.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HelpMessageSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/ThingOnScreen.h"
#include "ECS/TownDesire.h"
#include "Graphics/Moon.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"

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

/// The player of this computer, who hears the guidance
PlayerNames OwnPlayer()
{
	return Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer() : PlayerNames::PLAYER_ONE;
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
	const auto local = machine_clock::LocalTime();
	return guidance::ClockSaysNight(local.tm_hour, local.tm_min);
}

/// The random numbers of the help, which no frame draws
uint32_t HelpRand(int32_t n)
{
	return Locator::helpMessageSystem::has_value() ? Locator::helpMessageSystem::value().HelpRand(n) : 0;
}

float HelpFloatRand(float x)
{
	return Locator::helpMessageSystem::has_value() ? Locator::helpMessageSystem::value().HelpFloatRand(x) : 0.0f;
}

/// The advisors warn of a loss only near the point under the cursor
constexpr float k_WarnReach = 300.0f;

/// The point under the cursor of this computer's player, as the last reports of the hand and of the help that a turn did
/// give it. None when the cursor is not on a point of the world.
std::optional<glm::vec3> ReportedPointUnderCursor()
{
	if (!Locator::helpProfileSystem::has_value() || !Locator::handSystem::has_value() ||
	    !Locator::helpProfileSystem::value().GetLastReport().cursorOnPoint)
	{
		return std::nullopt;
	}
	const auto hands = Locator::handSystem::value().GetHandsOf(OwnPlayer());
	if (!hands.has_value())
	{
		return std::nullopt;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto hand = (*hands)[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)];
	const auto* reported = registry.Valid(hand) ? registry.TryGet<const ReportedHand>(hand) : nullptr;
	return reported != nullptr ? std::optional(reported->collidePoint) : std::nullopt;
}

/// Whether a thing stands nearer to the point under the cursor than the reach of a warning
bool NearPointUnderCursor(entt::entity thing)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return false;
	}
	const auto point = ReportedPointUnderCursor();
	const auto* at = Locator::entitiesRegistry::value().TryGet<const Transform>(thing);
	return point.has_value() && at != nullptr && gutils::GetDistanceInMetres(at->position, *point) < k_WarnReach;
}

/// The point under the cursor of the player at this computer, as the last report of a turn tells it (zero until the
/// first report)
glm::vec3 ReportedCollidePointOrZero()
{
	if (!Locator::handSystem::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return glm::vec3(0.0f);
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto hand = Locator::handSystem::value().GetOwnHands()[static_cast<size_t>(HandSystemInterface::Side::Left)];
	const auto* reported = registry.Valid(hand) ? registry.TryGet<const ReportedHand>(hand) : nullptr;
	return reported != nullptr ? reported->collidePoint : glm::vec3(0.0f);
}

/// Play a voice of the guidance sound bank at a thing on the land. The player hears it at full volume to a third of its
/// reach.
void PlayVoice(uint32_t sample, glm::vec3 position, float reach)
{
	if (Locator::terrainSystem::has_value())
	{
		position.y = Locator::terrainSystem::value().GetHeightAt(glm::xz(position));
	}
	audio::StartGameSoundEffect(resources::HashIdentifier(fmt::format("Guidance.sad/{}", sample)),
	                            {.position = position,
	                             .pitchPercent = guidance::k_VoicePitch,
	                             .volume = guidance::k_VoiceVolume,
	                             .voicePlayType = pack::AudioBankLoop::Once,
	                             .minDistance = reach * guidance::k_VoiceNearShare,
	                             .maxDistance = reach,
	                             .callerWins = audio::k_CallerDistancesWin});
}

/// The towns of the players (not the neutral player), player by player. The towns of each player come in the order that
/// the player got them.
std::vector<entt::entity> PlayersTowns(const ecs::Registry& registry)
{
	struct Owned
	{
		PlayerNames owner;
		uint32_t gained;
		entt::entity town;
	};
	std::vector<Owned> owned;
	registry.Each<const Town>([&owned](entt::entity entity, const Town& town) {
		if (town.owner < PlayerNames::NEUTRAL)
		{
			owned.push_back({.owner = town.owner, .gained = town.gained, .town = entity});
		}
	});
	std::ranges::sort(
	    owned, [](const Owned& a, const Owned& b) { return a.owner != b.owner ? a.owner < b.owner : a.gained < b.gained; });
	std::vector<entt::entity> towns;
	towns.reserve(owned.size());
	std::ranges::transform(owned, std::back_inserter(towns), &Owned::town);
	return towns;
}
} // namespace

void GuidanceSystem::SaveState(reflection::BinaryWriter& out) const
{
	reflection::Save(out, _stampsDue);
	reflection::Save(out, _moonWait);
	reflection::Save(out, _remarks);
}

bool GuidanceSystem::LoadState(reflection::BinaryReader& in)
{
	reflection::Load(in, _stampsDue);
	reflection::Load(in, _moonWait);
	reflection::Load(in, _remarks);
	return !in.Failed();
}

GuidanceHeard* GuidanceSystem::Heard()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return nullptr;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto here = OwnPlayer();
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
	if (_stampsDue && Locator::helpMessageSystem::has_value())
	{
		if (auto* heard = Heard(); heard != nullptr)
		{
			const auto turn = Turn();
			for (size_t kind = 0; kind < guidance::k_KindCount; ++kind)
			{
				const auto least = guidance::Rule(static_cast<guidance::Kind>(kind)).leastGap;
				heard->lastTurn.at(kind) = guidance::FirstStamp(turn, HelpRand(static_cast<int32_t>(least)));
			}
			heard->oneOffSaid = {};
			heard->alignmentDrift = 0.0f;
			heard->lastDesireVoice = 0;
			heard->seenThings.clear();
			_stampsDue = false;
		}
	}
	LookAtMoon();
	TownDesireVoices();
}

bool GuidanceSystem::PlayNow(guidance::Kind kind)
{
	auto* heard = Heard();
	if (heard == nullptr || !Locator::helpMessageSystem::has_value())
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
	const auto spread = guidance::GapSpread(rule.leastGap, HelpFloatRand(1.0f));
	const auto wait = rule.leastGap + HelpRand(static_cast<int32_t>(spread));
	const auto since = Turn() - heard->lastTurn.at(static_cast<size_t>(kind));
	return guidance::MayPlay(rule, level, firstLand, wait, since);
}

bool GuidanceSystem::RemarkNow(guidance::Kind kind)
{
	return PlayNow(guidance::Kind::AnyRemark) && PlayNow(kind);
}

void GuidanceSystem::RemarkIfNow(guidance::Kind kind)
{
	if (RemarkNow(kind))
	{
		SayRemark(kind);
	}
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
	if (!row.has_value() || !Locator::infoConstants::has_value() || !Locator::helpMessageSystem::has_value())
	{
		return;
	}
	const auto& texts = Locator::infoConstants::value().helpSpritesGuidance.at(*row).texts;
	const auto choices = guidance::RemarkChoices(texts);
	const auto pick = HelpRand(static_cast<int32_t>(choices));
	Say(texts.at(pick), kind);
}

void GuidanceSystem::TownAttacked(entt::entity town, PlayerNames attacker, bool byCreature)
{
	const auto here = OwnPlayer();
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

void GuidanceSystem::TownAttackCry(entt::entity town, const guidance::TownAttackVoice& voice)
{
	if (!Locator::entitiesRegistry::has_value() || !Locator::helpMessageSystem::has_value() ||
	    !PlayNow(guidance::Kind::TownAttackedSound))
	{
		return;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(town))
	{
		return;
	}
	// Only a town with people in it cries out
	const auto* stats = registry.TryGet<const TownStats>(town);
	const auto* at = registry.TryGet<const Transform>(town);
	if (stats == nullptr || stats->adults + stats->children == 0 || at == nullptr)
	{
		return;
	}
	constexpr int32_t k_AttackerCries = 10;
	const uint32_t attackerCry = voice.attacker == guidance::Attacker::Other
	                                 ? 0
	                                 : guidance::AttackerCry(voice.attacker, static_cast<uint32_t>(HelpRand(k_AttackerCries)));
	const auto cries = guidance::AttackCries(voice.fire, attackerCry);
	const auto* aggression = registry.TryGet<const TownAggression>(town);
	const float reach = guidance::AttackCryReach(aggression != nullptr ? aggression->record.protection : 0.0f);
	const auto sample = guidance::PickCry(cries, static_cast<uint32_t>(HelpRand(static_cast<int32_t>(cries.size()))));
	// The guidance counts the cry as heard even when the bank has no sound for it
	if (auto* heard = Heard(); heard != nullptr)
	{
		heard->lastTurn.at(static_cast<size_t>(guidance::Kind::TownAttackedSound)) = Turn();
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("audio"), "Town {} cries out: guidance sample {}, heard to {:.0f} m",
	                    entt::to_integral(town), sample, reach);
	if (sample != 0)
	{
		PlayVoice(sample, at->position, reach);
	}
}

void GuidanceSystem::BuildingBroken(entt::entity building, std::optional<PlayerNames> player, float remaining)
{
	if (!(remaining < k_WreckedShare) || player != OwnPlayer() || !RemarkNow(guidance::Kind::DestroyBuilding))
	{
		return;
	}
	// Only a building the player can see
	const auto view = ecs::thing_on_screen::ScreenView(OwnPlayer());
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
	const float alignment = Locator::alignmentSystem::value().GetPlayerAlignment(OwnPlayer());
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
	if (heard == nullptr || !Locator::helpMessageSystem::has_value())
	{
		return;
	}
	if (heard->oneOffSaid.at(static_cast<size_t>(oneOff)))
	{
		return;
	}
	const auto& rule = guidance::Rule(oneOff);
	if (guidance::OneOffSaid(HelpFloatRand(1.0f), rule.chance))
	{
		Say(rule.text, guidance::Kind::OneOff);
		// Looked up again: saying it may have added the component to the player
		if (auto* again = Heard(); again != nullptr)
		{
			again->oneOffSaid.at(static_cast<size_t>(oneOff)) = true;
		}
	}
}

void GuidanceSystem::KilledVillager(entt::entity villager)
{
	if (!RemarkNow(guidance::Kind::KillingPeople))
	{
		return;
	}
	const auto view = ecs::thing_on_screen::ScreenView(OwnPlayer());
	if (view.has_value() && Locator::entitiesRegistry::value().Valid(villager) &&
	    ecs::thing_on_screen::OnScreen(*view, villager))
	{
		SayRemark(guidance::Kind::KillingPeople);
	}
}

void GuidanceSystem::CreatureWonFight(entt::entity creature)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.Valid(creature) ? registry.TryGet<const Creature>(creature) : nullptr;
	if (body == nullptr || body->owner != OwnPlayer() || !RemarkNow(guidance::Kind::GeneralGood))
	{
		return;
	}
	// Only when the place where the creature stands is on screen
	const auto view = ecs::thing_on_screen::ScreenView(OwnPlayer());
	if (view.has_value() && ecs::thing_on_screen::PlaceOnScreen(*view, creature))
	{
		SayRemark(guidance::Kind::GeneralGood);
	}
}

void GuidanceSystem::WorshipperDied()
{
	RemarkIfNow(guidance::Kind::WorshippersDying);
}

void GuidanceSystem::BeliefVoice(glm::vec3 at, float share, GuidanceAlignment alignment)
{
	if (!Locator::gameRandom::has_value() || !PlayNow(guidance::Kind::BeliefSound))
	{
		return;
	}
	auto* heard = Heard();
	if (heard == nullptr)
	{
		return;
	}
	auto& random = Locator::gameRandom::value();
	const float left = share - random.LocalFloatRand(guidance::BeliefVoiceDrawLimit(share));
	if (alignment != GuidanceAlignment::Good && alignment != GuidanceAlignment::Evil)
	{
		alignment = random.LocalRand(2) != 0 ? GuidanceAlignment::Evil : GuidanceAlignment::Good;
	}
	const auto sample = guidance::BeliefVoice(alignment, left);
	if (!sample.has_value())
	{
		return;
	}
	const auto turn = Turn();
	const float distance = gutils::GetDistanceInMetres(ReportedCollidePointOrZero(), at);
	const auto sinceDesireVoice = turn - heard->lastTurn.at(static_cast<size_t>(guidance::Kind::TownDesireSound));
	// The game compares the voice with a last voice that it clears on each land and never sets, so the two are never
	// the same
	constexpr bool k_SameAsLast = false;
	if (!(guidance::BeliefVoiceWanted(sinceDesireVoice, distance, k_SameAsLast) > guidance::k_BeliefVoiceLeast))
	{
		return;
	}
	heard->lastTurn.at(static_cast<size_t>(guidance::Kind::BeliefSound)) = turn;
	SPDLOG_LOGGER_DEBUG(spdlog::get("audio"), "Voice of awe: guidance sample {}, share {:.3f}, {:.0f} m from the cursor",
	                    *sample, left, distance);
	PlayVoice(*sample, at, guidance::k_BeliefVoiceReach);
}

void GuidanceSystem::LosingVillager(entt::entity villager)
{
	if (RemarkNow(guidance::Kind::LosingVillagers) && NearPointUnderCursor(villager))
	{
		SayRemark(guidance::Kind::LosingVillagers);
	}
}

void GuidanceSystem::LowOnPeople(entt::entity town)
{
	if (!RemarkNow(guidance::Kind::LowOnPeople) || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	const auto* stats = Locator::entitiesRegistry::value().TryGet<const TownStats>(town);
	if (stats != nullptr && stats->adults + stats->children != 0 && NearPointUnderCursor(town))
	{
		SayRemark(guidance::Kind::LowOnPeople);
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
	const auto look = guidance::LookAtMoon(graphics::moon::Phase(machine_clock::UnixTime()), ComputerNight());
	if (look.remark)
	{
		SayOneOff(guidance::OneOff::FullMoon);
	}
	_moonWait = look.wait;
}

void GuidanceSystem::TownDesireVoices()
{
	if (Turn() % guidance::k_DesireVoiceTurns != 0 || !Locator::entitiesRegistry::has_value() ||
	    !Locator::helpMessageSystem::has_value() || !Locator::handSystem::has_value() ||
	    !PlayNow(guidance::Kind::TownDesireSound))
	{
		return;
	}
	auto* heard = Heard();
	if (heard == nullptr)
	{
		return;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto turn = Turn();
	// The camera of the player as the last report of the hand that a turn did gives it
	const auto reportedCamera = Locator::handSystem::value().GetReportedCamera(OwnPlayer());
	if (!reportedCamera.has_value())
	{
		return;
	}
	const glm::vec3 camera = reportedCamera->origin;
	const auto since = turn - heard->lastTurn.at(static_cast<size_t>(guidance::Kind::TownDesireSound));
	const auto score = [&](entt::entity thing, float distance, float desire, uint32_t voice) {
		return guidance::DesireVoiceScore({.turnsSinceHeard = since,
		                                   .turnsSinceSeen = guidance::TurnsSinceSeen(heard->seenThings, thing, turn),
		                                   .distance = distance,
		                                   .desire = desire,
		                                   .sameAsLast = voice == heard->lastDesireVoice});
	};
	guidance::DesireVoicePick best;
	entt::entity thing = entt::null;

	// Find the town with people that is nearest to the camera, by its storage pit
	float nearest = guidance::k_DesireVoiceRange;
	entt::entity town = entt::null;
	for (const auto candidate : PlayersTowns(registry))
	{
		const auto pit = ecs::construction::StoragePitOf(candidate);
		const auto* stats = registry.TryGet<const TownStats>(candidate);
		if (!pit.has_value() || stats == nullptr || stats->adults + stats->children == 0 || !registry.AllOf<Transform>(*pit))
		{
			continue;
		}
		const float distance = gutils::GetDistanceInMetres(camera, registry.Get<const Transform>(*pit).position);
		if (distance < nearest)
		{
			nearest = distance;
			town = candidate;
		}
	}
	// Each desire of the town picks a voice and scores it, the strongest desire first
	if (const auto* desires = town != entt::null ? registry.TryGet<const TownDesire>(town) : nullptr; desires != nullptr)
	{
		thing = town;
		for (const auto& sorted : desires->sortedRaw)
		{
			const float raw = ecs::town_desire::GetRawDesire(*desires, sorted.index);
			const float draw = HelpFloatRand(guidance::DesireVoiceDrawLimit(sorted.value));
			const auto voice = guidance::DesireVoice(sorted.index, sorted.value, draw);
			if (voice == 0)
			{
				continue;
			}
			if (const float value = score(town, nearest, raw, voice); value > best.score)
			{
				best = {.voice = voice, .score = value};
			}
		}
	}

	// Find the worship site of the player at this machine that is nearest to the camera, with people that dance at it
	const auto temple =
	    Locator::worshipSiteSystem::has_value() ? Locator::worshipSiteSystem::value().TempleOf(OwnPlayer()) : entt::null;
	if (temple != entt::null)
	{
		const auto& worship = registry.Get<const CitadelWorship>(temple);
		float nearestSite = guidance::k_DesireVoiceRange;
		entt::entity site = entt::null;
		for (const auto candidate : worship.sites)
		{
			if (candidate == entt::null || !registry.Valid(candidate) || !registry.AllOf<Transform>(candidate))
			{
				continue;
			}
			const auto dance = registry.Get<const WorshipSite>(candidate).dance;
			const auto* dancing = registry.Valid(dance) ? registry.TryGet<const Dance>(dance) : nullptr;
			if (dancing == nullptr || dancing->dancers == 0)
			{
				continue;
			}
			const float distance = gutils::GetDistanceInMetres(camera, registry.Get<const Transform>(candidate).position);
			if (distance < nearestSite)
			{
				nearestSite = distance;
				site = candidate;
			}
		}
		if (site != entt::null)
		{
			// The want of food of the site picks both voices. The food voice scores by this want, and the worshippers
			// voice scores by the strain of the temple's sites.
			const float food = Locator::worshipSiteSystem::value().DesireForFood(site);
			const float strain = worship.strainSound < 1.0f ? worship.strainSound : 1.0f;
			const float foodDraw = HelpFloatRand(guidance::DesireVoiceDrawLimit(food));
			const auto foodVoice = guidance::DesireVoice(guidance::k_WorshipFoodRow, food, foodDraw);
			const float worshippersDraw = HelpFloatRand(guidance::DesireVoiceDrawLimit(food));
			const auto worshippersVoice = guidance::DesireVoice(guidance::k_WorshippersRow, food, worshippersDraw);
			const guidance::DesireVoicePick foodPick {
			    .voice = foodVoice, .score = foodVoice != 0 ? score(site, nearestSite, food, foodVoice) : 0.0f};
			const guidance::DesireVoicePick worshippersPick {
			    .voice = worshippersVoice,
			    .score = worshippersVoice != 0 ? score(site, nearestSite, strain, worshippersVoice) : 0.0f};
			if (const auto pick = guidance::WorshipSiteVoice(foodPick, worshippersPick); pick.has_value())
			{
				best = *pick;
				// The voice of a worship site plays at the temple
				thing = temple;
			}
		}
	}

	if (!(best.score > guidance::k_DesireVoiceLeast))
	{
		return;
	}
	const float draw = HelpFloatRand(guidance::DesireVoiceReachDrawLimit(best.score));
	const auto* at = thing != entt::null ? registry.TryGet<const Transform>(thing) : nullptr;
	if (at == nullptr)
	{
		return;
	}
	const float reach = guidance::DesireVoiceReach(best.score, draw);
	heard->lastTurn.at(static_cast<size_t>(guidance::Kind::TownDesireSound)) = turn;
	heard->lastDesireVoice = best.voice;
	SPDLOG_LOGGER_DEBUG(spdlog::get("audio"), "Desire voice at {}: guidance sample {}, score {:.3f}, heard to {:.0f} m",
	                    entt::to_integral(thing), best.voice, best.score, reach);
	PlayVoice(best.voice, at->position, reach);
}
