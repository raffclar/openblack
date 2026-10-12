/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureHelp.h"

#include <cmath>

#include <algorithm>
#include <limits>

#include <glm/geometric.hpp>

using namespace openblack;
using namespace openblack::creature_help;

namespace
{
using enum Telling;

// level, how it is said, first text, needs the creature in view, stacks, only one waits
constexpr std::array<KindInfo, k_KindCount> k_Kinds {{
    {3, PointAtCreature, 462, false, true, false},  // LessonIncreaseSource
    {3, PointAtCreature, 523, false, true, false},  // LessonDecreaseSource
    {3, PointAtCreature, 664, false, true, false},  // LessonIncreaseDesire
    {3, PointAtCreature, 704, false, true, false},  // LessonDecreaseDesire
    {3, PointAtCreature, 744, false, true, false},  // LessonIncreaseOpinion
    {3, PointAtCreature, 784, false, true, false},  // LessonDecreaseOpinion
    {2, PointAtCreature, 876, false, true, false},  // LessonNormalAction
    {2, PointAtCreature, 0, false, true, false},    // LessonMagicAction: the miracle's own text
    {2, PointAtCreature, 882, false, true, false},  // NearlyLearntNormalAction
    {2, PointAtCreature, 0, false, true, false},    // NearlyLearntMagicAction: the miracle's own text
    {4, JustText, 894, true, false, false},         // CantLearnNormalActionYet
    {4, JustText, 900, true, false, false},         // CantLearnMagicActionYet
    {1, Nothing, 0, false, true, false},            // StageInitial
    {1, StageScript, 0, false, true, false},        // StageLearnToEat
    {1, StageScript, 0, false, true, false},        // StagePunishment
    {1, StageScript, 0, false, true, false},        // StageLeashPullAndPickUp
    {1, StageScript, 0, false, true, false},        // StageLeashAttachToHouse
    {1, StageScript, 0, false, true, false},        // StageMeetGuide
    {1, StageScript, 0, false, true, false},        // StageFriendsWithGuide
    {1, StageScript, 0, false, true, false},        // StageGuideExplainsHistory
    {1, StageScript, 0, false, true, false},        // StageLearnSpell
    {1, StageScript, 0, false, true, false},        // StageLearnToImpressTown
    {1, StageScript, 0, false, true, false},        // StageLearnToFight
    {1, StageScript, 0, false, true, false},        // StageLearnToHelpTown
    {1, StageScript, 0, false, true, false},        // StageLeashGoodAndEvil
    {1, StageScript, 0, false, true, false},        // StageFullyMature
    {1, PointAtCreature, 1046, false, true, false}, // ReceivedSpell
    {1, PointAtCreature, 1062, false, true, false}, // FinishedReceivingSpell
    {1, JustText, 1084, true, true, false},         // Tutorial
    {4, JustText, 401, false, false, false},        // CurrentSource
    {2, JustText, 868, false, true, false},         // PhysicalProblem
    {2, Unsaid, 1078, false, true, false},          // MentalProblem
    {2, JustText, 888, true, true, false},          // PromptHasntLearntNormalAction
    {2, JustText, 1079, true, false, false},        // TownAttitudeToCreature
    {2, JustText, 989, true, true, false},          // CannotLearnActionInThisStage
    {2, JustText, 1009, true, true, false},         // ShowStageCriteria
    {1, JustText, 624, false, false, true},         // SomethingInterestingOffScreen: said as what it wants is
    {1, JustText, 1105, false, true, false},        // MiscellaneousStacked
    {1, JustText, 1112, false, false, false},       // MiscellaneousUnstacked
    {1, PointAtCreature, 1113, false, true, false}, // MessedUpAction
    {3, JustText, 1096, true, false, false},        // FailureExplanation
    {2, JustText, 624, false, false, true},         // CurrentDesire
}};

constexpr std::array<std::string_view, 13> k_StageScripts {
    "CreatureDevLearnToEat",
    "CreatureDevPunishment",
    "CreatureDevLeashIntro",
    "CreatureDevLeashAttachToHouse",
    "MeetTheGuide",
    "CreatureDevFriendsWithGuide",
    "CreatureDevExplainsPast",
    "CreatureDevTeachesSpells",
    "CreatureDevImpressTown",
    "CreatureDevGuideTeachesFight",
    "CreatureDevHelpTown",
    "CreatureDevLeashGoodAndEvil",
    "TheStorm",
};

/// A message is told again only after a minute; what the creature wants, after five seconds
constexpr uint32_t k_RepeatSeconds = 60;
constexpr uint32_t k_DesireRepeatSeconds = 5;

/// Beyond this a camera doesn't see the creature well enough for the help to talk of it
constexpr float k_ViewDistance = 400.0f;
/// The help's cone: two thirds of the game's field of view (70 degrees)
constexpr float k_ViewCone = 1.2217305f * (2.0f / 3.0f);

/// The hand-over reading keeps nine tenths of itself each turn
constexpr float k_HandOverKeep = 0.9f;
constexpr float k_HandOverGain = 0.1f;

std::vector<Entry>& EntriesOf(Memory& memory, Kind kind)
{
	return memory.entries.at(static_cast<size_t>(kind));
}

const std::vector<Entry>& EntriesOf(const Memory& memory, Kind kind)
{
	return memory.entries.at(static_cast<size_t>(kind));
}
} // namespace

const KindInfo& creature_help::Info(Kind kind)
{
	return k_Kinds.at(static_cast<size_t>(kind));
}

std::optional<std::string_view> creature_help::StageScript(Kind kind, uint32_t land)
{
	if (!IsStageMessage(kind) || kind == Kind::StageInitial)
	{
		return std::nullopt;
	}
	if (kind == Kind::StageFullyMature && land != 1)
	{
		return std::nullopt;
	}
	return k_StageScripts.at(static_cast<size_t>(kind) - static_cast<size_t>(Kind::StageLearnToEat));
}

std::optional<uint32_t> creature_help::Text(Kind kind, uint32_t parameter, std::optional<uint32_t> miracleText)
{
	const auto& info = Info(kind);
	switch (kind)
	{
	case Kind::LessonMagicAction:
	case Kind::NearlyLearntMagicAction:
		return miracleText.has_value() && *miracleText != 0 ? miracleText : std::nullopt;
	case Kind::LessonNormalAction:
	case Kind::NearlyLearntNormalAction:
	case Kind::PromptHasntLearntNormalAction:
		// Skill 0 isn't told of
		if (parameter == 0)
		{
			return std::nullopt;
		}
		break;
	case Kind::MiscellaneousStacked:
		if (parameter == k_TooBigToHandle)
		{
			return k_TooBigToHandleText;
		}
		break;
	default:
		break;
	}
	if (info.telling == Telling::Nothing || info.telling == Telling::StageScript)
	{
		return std::nullopt;
	}
	return info.firstText + parameter;
}

Outcome creature_help::Decide(const Message& message, const Situation& situation, bool toldLately)
{
	const auto& info = Info(message.kind);
	const bool stage = IsStageMessage(message.kind);
	if (situation.insideTemple || !situation.playersCreature)
	{
		return Outcome::Drop;
	}
	if (!stage)
	{
		if (situation.stage < k_HelpStage || !situation.helpOn)
		{
			return Outcome::Drop;
		}
		if (!message.forced && situation.outOrAsleep)
		{
			return Outcome::Drop;
		}
	}
	const auto held = situation.dialogueTaken || (situation.handOnCreature && !message.forced) ||
	                  (situation.raisedThisTurn && !stage) || situation.scriptControlled ||
	                  (situation.inDevelopmentScript && !stage);
	const auto waitOrDrop = info.stacks ? Outcome::Stack : Outcome::Drop;
	if (held)
	{
		return waitOrDrop;
	}
	if (info.needsCreatureInView && !situation.inView)
	{
		return Outcome::Drop;
	}
	if (toldLately && !message.forced)
	{
		return waitOrDrop;
	}
	return Outcome::Tell;
}

uint32_t creature_help::WholeSeconds(uint32_t now, uint32_t then, uint32_t turnsPerSecond)
{
	return turnsPerSecond == 0 ? 0 : (now - then) / turnsPerSecond;
}

bool creature_help::ToldLately(const Memory& memory, Kind kind, uint32_t parameter, uint32_t turn, uint32_t turnsPerSecond)
{
	const auto& entries = EntriesOf(memory, kind);
	const auto found = std::ranges::find(entries, parameter, &Entry::parameter);
	if (found == entries.end() || found->toldTurn == 0)
	{
		return false;
	}
	const auto limit = kind == Kind::CurrentDesire ? k_DesireRepeatSeconds : k_RepeatSeconds;
	return WholeSeconds(turn, found->toldTurn, turnsPerSecond) < limit;
}

void creature_help::MarkTold(Memory& memory, Kind kind, uint32_t parameter, uint32_t turn)
{
	auto& entries = EntriesOf(memory, kind);
	if (auto found = std::ranges::find(entries, parameter, &Entry::parameter); found != entries.end())
	{
		found->toldTurn = turn;
		found->waiting = false;
		return;
	}
	entries.push_back({.parameter = parameter, .toldTurn = turn});
}

void creature_help::Stack(Memory& memory, const Message& message, uint32_t turn, uint32_t turnsPerSecond)
{
	auto& entries = EntriesOf(memory, message.kind);
	const bool oneWaiting = Info(message.kind).oneWaiting;
	for (auto& entry : entries)
	{
		if (entry.parameter != message.parameter && !oneWaiting)
		{
			continue;
		}
		// Told within the last minute, the newer one is let go
		if (entry.toldTurn != 0 && WholeSeconds(turn, entry.toldTurn, turnsPerSecond) <= k_RepeatSeconds)
		{
			return;
		}
		entry.parameter = message.parameter;
		entry.thing = message.thing;
		entry.position = message.position;
		entry.waiting = true;
		return;
	}
	entries.push_back(
	    {.parameter = message.parameter, .thing = message.thing, .position = message.position, .toldTurn = 0, .waiting = true});
}

std::optional<Waiting> creature_help::FirstWaiting(const Memory& memory)
{
	for (size_t kind = 0; kind < k_KindCount; ++kind)
	{
		const auto& entries = memory.entries.at(kind);
		if (const auto found = std::ranges::find_if(entries, &Entry::waiting); found != entries.end())
		{
			return Waiting {static_cast<Kind>(kind), static_cast<size_t>(found - entries.begin())};
		}
	}
	return std::nullopt;
}

void creature_help::Settle(Memory& memory, Waiting waiting, uint32_t turn)
{
	auto& entries = EntriesOf(memory, waiting.kind);
	if (waiting.index < entries.size())
	{
		entries[waiting.index].toldTurn = turn;
		entries[waiting.index].waiting = false;
	}
}

std::optional<BodyProblem> creature_help::BodyProblemNow(const Body& body, const BodyThresholds& thresholds,
                                                         uint32_t secondsSinceLast)
{
	if (!(static_cast<float>(secondsSinceLast) > thresholds.seconds))
	{
		return std::nullopt;
	}
	if (body.energy < thresholds.lowEnergy)
	{
		return BodyProblem::LowEnergy;
	}
	if (body.energy < thresholds.veryLowEnergy)
	{
		return BodyProblem::VeryLowEnergy;
	}
	if (body.life < thresholds.lowLife)
	{
		return BodyProblem::Hurt;
	}
	if (body.life < thresholds.veryLowLife)
	{
		return BodyProblem::BadlyHurt;
	}
	if (body.exhaustion > thresholds.tired)
	{
		return BodyProblem::Tired;
	}
	if (body.exhaustion > thresholds.veryTired)
	{
		return BodyProblem::VeryTired;
	}
	if (body.dehydration > thresholds.thirsty)
	{
		return BodyProblem::Thirsty;
	}
	if (body.dehydration > thresholds.veryThirsty)
	{
		return BodyProblem::VeryThirsty;
	}
	return std::nullopt;
}

std::optional<BodyProblem> creature_help::FaintingProblem(const Body& body)
{
	if (body.life <= 0.0f)
	{
		return BodyProblem::BadlyHurt;
	}
	if (body.energy <= 0.0f)
	{
		return BodyProblem::VeryLowEnergy;
	}
	if (body.exhaustion >= 1.0f)
	{
		return BodyProblem::VeryTired;
	}
	return std::nullopt;
}

float creature_help::NextHandOver(float handOver, bool over)
{
	return (over ? 1.0f : 0.0f) * k_HandOverGain + handOver * k_HandOverKeep;
}

std::array<bool, k_TutorialCount> creature_help::TutorialApplies(const TutorialConditions& conditions)
{
	constexpr uint32_t k_RunStage = 3;
	return {
	    true,                           // Leash
	    conditions.stage >= k_RunStage, // Run
	    true,                           // HandOver
	    false,                          // Unleash: never
	    conditions.wantsToBuildHome,    // MoreRocks
	    true,                           // PickUp
	    conditions.wantsToBuildHome,    // DropRocksAtHome
	    conditions.carrying,            // DropAtFeet
	    conditions.carrying,            // StrokeToEat
	    true,                           // SlapOrStroke
	    conditions.wantsWater,          // Drink
	    conditions.wantsToPoo,          // Poo
	};
}

std::optional<size_t> creature_help::TutorialNow(const Memory& memory, std::span<const bool, k_TutorialCount> applies,
                                                 uint32_t turn, uint32_t turnsPerSecond, float seconds)
{
	if (!(static_cast<float>(WholeSeconds(turn, memory.tutorialTurn, turnsPerSecond)) > seconds))
	{
		return std::nullopt;
	}
	auto oldest = std::numeric_limits<uint32_t>::max();
	std::optional<size_t> chosen;
	for (size_t i = 0; i < k_TutorialCount; ++i)
	{
		if (!memory.tutorialDone.at(i) && applies[i] && memory.tutorialToldTurn.at(i) < oldest)
		{
			oldest = memory.tutorialToldTurn.at(i);
			chosen = i;
		}
	}
	if (!chosen.has_value())
	{
		return std::nullopt;
	}
	const auto since = oldest == 0 && *chosen == 0 ? std::numeric_limits<float>::max()
	                                               : static_cast<float>(WholeSeconds(turn, oldest, turnsPerSecond));
	return since > seconds ? chosen : std::nullopt;
}

bool creature_help::LooksAt(glm::vec3 cameraOrigin, glm::vec3 cameraFocus, glm::vec3 point)
{
	auto view = cameraFocus - cameraOrigin;
	if (view != glm::vec3(0.0f))
	{
		view *= 1.0f / std::sqrt(glm::dot(view, view));
	}
	auto toPoint = point - cameraOrigin;
	if (toPoint != glm::vec3(0.0f))
	{
		const auto distance = std::sqrt(glm::dot(toPoint, toPoint));
		if (!(distance < k_ViewDistance))
		{
			return false;
		}
		toPoint *= 1.0f / distance;
	}
	return std::cos(k_ViewCone) < glm::dot(toPoint, view);
}

size_t creature_help::DominantDesire(std::span<const float> values, std::span<const bool> active)
{
	size_t dominant = 0;
	float greatest = 0.0f;
	for (size_t i = 0; i < std::min(values.size(), active.size()); ++i)
	{
		if (active[i] && values[i] > greatest)
		{
			greatest = values[i];
			dominant = i;
		}
	}
	return dominant;
}

std::optional<creature_desires::Desire> creature_help::DesireShownOnHold(std::optional<creature_desires::Desire> credited,
                                                                         std::optional<creature_desires::Desire> planned)
{
	const auto desire = credited.has_value() ? credited : planned;
	if (!desire.has_value() || !creature_desires::EmoteFor(*desire).has_value())
	{
		return std::nullopt;
	}
	return desire;
}

Message creature_help::TooYoungToLearn(float feedback)
{
	return {.kind = Kind::MiscellaneousStacked, .parameter = feedback >= 0.0f ? k_StrokedTooYoung : k_SlappedTooYoung};
}

Message creature_help::FeedbackLesson(float feedback, bool opinionChanged, creature_desires::Desire desire, uint32_t source)
{
	const bool better = feedback > 0.0f;
	if (opinionChanged)
	{
		return {.kind = better ? Kind::LessonIncreaseOpinion : Kind::LessonDecreaseOpinion,
		        .parameter = static_cast<uint32_t>(desire)};
	}
	return {.kind = better ? Kind::LessonIncreaseSource : Kind::LessonDecreaseSource, .parameter = source};
}

Message creature_help::SpellMessage(bool tookHold, uint32_t spell)
{
	return {.kind = tookHold ? Kind::ReceivedSpell : Kind::FinishedReceivingSpell, .parameter = spell};
}
