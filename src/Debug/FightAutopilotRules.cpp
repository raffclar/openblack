/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FightAutopilotRules.h"

#include <charconv>

#include <algorithm>
#include <array>
#include <format>
#include <ranges>
#include <utility>
#include <variant>

using namespace openblack;
using namespace openblack::fight_autopilot;
namespace fight = openblack::creature_fight;

namespace
{
constexpr std::array<std::string_view, 4> k_PolicyNames {"lesson", "aggressive", "random", "sequence"};
constexpr std::array<std::string_view, 5> k_StageNames {"hit", "hits", "block", "step", "bout"};

/// The presses of the lesson: a single blow waits for the guide to reel; the quick blows follow each other faster than
/// one blow plays, so that more than two wait in the queue
constexpr float k_SingleBlowHoldMs = 200.0f;
constexpr float k_SingleBlowWaitMs = 1500.0f;
constexpr float k_QuickBlowHoldMs = 60.0f;
constexpr float k_QuickBlowWaitMs = 150.0f;
constexpr float k_BlockHoldMs = 150.0f;
constexpr float k_StepHoldMs = 100.0f;
constexpr float k_MoveWaitMs = 1500.0f;
/// The aggressive policy and the bout hit a little slower than one blow plays
constexpr float k_AggressiveWaitMs = 700.0f;
/// The random policy holds and waits within these times
constexpr float k_RandomHoldMinMs = 60.0f;
constexpr float k_RandomHoldMaxMs = 800.0f;
constexpr float k_RandomWaitMinMs = 400.0f;
constexpr float k_RandomWaitMaxMs = 1500.0f;

constexpr std::array<std::pair<std::string_view, Intent>, 8> k_MoveNames {{
    {"high", {.kind = Intent::Kind::Hit, .band = fight::Band::High}},
    {"middle", {.kind = Intent::Kind::Hit, .band = fight::Band::Mid}},
    {"low", {.kind = Intent::Kind::Hit, .band = fight::Band::Low}},
    {"block", {.kind = Intent::Kind::Block}},
    {"step_forward", {.kind = Intent::Kind::Step, .step = fight::Step::Forward}},
    {"step_back", {.kind = Intent::Kind::Step, .step = fight::Step::Back}},
    {"step_left", {.kind = Intent::Kind::Step, .step = fight::Step::Left}},
    {"step_right", {.kind = Intent::Kind::Step, .step = fight::Step::Right}},
}};

bool Reeling(fight::FightAction action)
{
	return action == fight::FightAction::ReelingHigh || action == fight::FightAction::ReelingMiddle ||
	       action == fight::FightAction::ReelingLow;
}

Intent Hit(fight::Band band, float holdMs)
{
	return {.kind = Intent::Kind::Hit, .band = band, .holdMs = holdMs};
}

const fight_transcript::Field* FieldOf(const fight_transcript::Entry& entry, std::string_view name)
{
	const auto found = std::ranges::find(entry.fields, name, &fight_transcript::Field::name);
	return found != entry.fields.end() ? &*found : nullptr;
}

std::optional<int64_t> NumberOf(const fight_transcript::Entry& entry, std::string_view name)
{
	const auto* field = FieldOf(entry, name);
	if (field == nullptr)
	{
		return std::nullopt;
	}
	if (const auto* number = std::get_if<int64_t>(&field->value))
	{
		return *number;
	}
	return std::nullopt;
}

std::string_view TextOf(const fight_transcript::Entry& entry, std::string_view name)
{
	const auto* field = FieldOf(entry, name);
	const auto* text = field != nullptr ? std::get_if<std::string>(&field->value) : nullptr;
	return text != nullptr ? std::string_view(*text) : std::string_view();
}
} // namespace

std::string_view fight_autopilot::Name(Policy policy)
{
	const auto index = static_cast<size_t>(policy);
	return index < k_PolicyNames.size() ? k_PolicyNames.at(index) : "unknown";
}

std::optional<Policy> fight_autopilot::PolicyFromName(std::string_view name)
{
	const auto found = std::ranges::find(k_PolicyNames, name);
	return found != k_PolicyNames.end() ? std::optional(static_cast<Policy>(found - k_PolicyNames.begin())) : std::nullopt;
}

std::string_view fight_autopilot::Name(LessonStage stage)
{
	const auto index = static_cast<size_t>(stage);
	return index < k_StageNames.size() ? k_StageNames.at(index) : "unknown";
}

std::optional<LessonStage> fight_autopilot::LessonStageFromName(std::string_view name)
{
	const auto found = std::ranges::find(k_StageNames, name);
	return found != k_StageNames.end() ? std::optional(static_cast<LessonStage>(found - k_StageNames.begin())) : std::nullopt;
}

std::string fight_autopilot::Describe(const Intent& intent)
{
	switch (intent.kind)
	{
	case Intent::Kind::None:
		return "none";
	case Intent::Kind::Hit:
		return std::format("hit {} for {:.0f} ms", fight::Name(intent.band), intent.holdMs);
	case Intent::Kind::Block:
		return std::format("block for {:.0f} ms", intent.holdMs);
	case Intent::Kind::Step:
		return std::format("step {}", fight::Name(intent.step));
	}
	return "unknown";
}

LessonStage fight_autopilot::Advance(LessonStage stage, const Observation& seen)
{
	switch (stage)
	{
	case LessonStage::Hit:
		return Reeling(seen.opponent) ? LessonStage::Hits : stage;
	case LessonStage::Hits:
		return seen.queuedBlows > 2 ? LessonStage::Block : stage;
	case LessonStage::Block:
		return seen.own == fight::FightAction::Block ? LessonStage::Step : stage;
	case LessonStage::Step:
		return seen.own == fight::FightAction::Step ? LessonStage::Bout : stage;
	case LessonStage::Bout:
		return stage;
	}
	return stage;
}

std::optional<std::vector<SequenceMove>> fight_autopilot::ParseSequence(std::string_view text)
{
	std::vector<SequenceMove> moves;
	for (const auto part : text | std::views::split(','))
	{
		const std::string_view item(part.begin(), part.end());
		if (item.empty())
		{
			continue;
		}
		const auto colon = item.find(':');
		if (colon == std::string_view::npos)
		{
			return std::nullopt;
		}
		const auto time = item.substr(0, colon);
		const auto name = item.substr(colon + 1);
		float seconds = 0.0f;
		const auto [end, error] = std::from_chars(time.data(), time.data() + time.size(), seconds);
		if (error != std::errc() || end != time.data() + time.size() || seconds < 0.0f)
		{
			return std::nullopt;
		}
		const auto found = std::ranges::find(k_MoveNames, name, &std::pair<std::string_view, Intent>::first);
		if (found == k_MoveNames.end())
		{
			return std::nullopt;
		}
		moves.push_back({.atMs = seconds * 1000.0f, .intent = found->second});
	}
	// The moves are made in the order of their times
	std::ranges::stable_sort(moves, {}, &SequenceMove::atMs);
	return moves;
}

Choice fight_autopilot::Choose(Policy policy, LessonStage stage, const Observation& seen, std::mt19937& random)
{
	// The block and the step of the lesson wait until the moves in the queue are done, so that each shows by itself
	if (policy == Policy::Lesson && (stage == LessonStage::Block || stage == LessonStage::Step) && seen.queued > 0)
	{
		return {};
	}
	const auto band = [&random] { return static_cast<fight::Band>(std::uniform_int_distribution<int>(0, 2)(random)); };
	switch (policy)
	{
	case Policy::Lesson:
		switch (stage)
		{
		case LessonStage::Hit:
			return {.intent = Hit(fight::Band::Mid, k_SingleBlowHoldMs), .waitMs = k_SingleBlowWaitMs};
		case LessonStage::Hits:
			return {.intent = Hit(fight::Band::Mid, k_QuickBlowHoldMs), .waitMs = k_QuickBlowWaitMs};
		case LessonStage::Block:
			return {.intent = {.kind = Intent::Kind::Block, .holdMs = k_BlockHoldMs}, .waitMs = k_MoveWaitMs};
		case LessonStage::Step:
			return {.intent = {.kind = Intent::Kind::Step, .step = fight::Step::Forward, .holdMs = k_StepHoldMs},
			        .waitMs = k_MoveWaitMs};
		case LessonStage::Bout:
			return {.intent = Hit(band(), k_SingleBlowHoldMs), .waitMs = k_AggressiveWaitMs};
		}
		break;
	case Policy::Aggressive:
		return {.intent = Hit(band(), k_SingleBlowHoldMs), .waitMs = k_AggressiveWaitMs};
	case Policy::Random:
	{
		const auto pick = std::uniform_int_distribution<size_t>(0, k_MoveNames.size() - 1)(random);
		auto intent = k_MoveNames.at(pick).second;
		intent.holdMs = std::uniform_real_distribution<float>(k_RandomHoldMinMs, k_RandomHoldMaxMs)(random);
		const auto wait = std::uniform_real_distribution<float>(k_RandomWaitMinMs, k_RandomWaitMaxMs)(random);
		return {.intent = intent, .waitMs = wait};
	}
	case Policy::Sequence:
		break;
	}
	return {};
}

std::optional<size_t> fight_autopilot::DueMove(std::span<const SequenceMove> sequence, size_t next, float elapsedMs)
{
	if (next < sequence.size() && sequence[next].atMs <= elapsedMs)
	{
		return next;
	}
	return std::nullopt;
}

std::optional<LessonStage> fight_autopilot::ScriptStage(const fight_transcript::Fight& fight, uint32_t playersCreature)
{
	const auto own = static_cast<int64_t>(playersCreature);
	const auto opponent = static_cast<int64_t>(fight.first == playersCreature ? fight.second : fight.first);
	std::optional<LessonStage> stage;
	const auto atLeast = [&stage](LessonStage next) {
		if (!stage.has_value() || *stage < next)
		{
			stage = next;
		}
	};
	for (const auto& entry : fight.entries)
	{
		const auto creature = NumberOf(entry, "creature");
		if (entry.kind == "script_read")
		{
			const auto native = TextOf(entry, "native");
			if (native == "GET_CREATURE_FIGHT_ACTION" && creature == opponent)
			{
				atLeast(LessonStage::Hit);
			}
			else if (native == "CREATURE_FIGHT_QUEUE_HITS" && creature == own)
			{
				atLeast(LessonStage::Hits);
			}
			else if (native == "GET_CREATURE_FIGHT_ACTION" && creature == own)
			{
				const auto* answer = FieldOf(entry, "answer");
				const auto* value = answer != nullptr ? std::get_if<double>(&answer->value) : nullptr;
				const bool block = value != nullptr && *value == static_cast<double>(fight::FightAction::Block);
				atLeast(block ? LessonStage::Step : LessonStage::Block);
			}
		}
		else if (entry.kind == "control" && creature == own && TextOf(entry, "why") == "script" &&
		         TextOf(entry, "to") == "computer")
		{
			atLeast(LessonStage::Bout);
		}
	}
	return stage;
}

bool LessonCheck::InOrder() const
{
	return hit.has_value() && hits.has_value() && block.has_value() && step.has_value() && *hit < *hits && *hits < *block &&
	       *block < *step;
}

LessonCheck fight_autopilot::CheckLesson(const fight_transcript::Fight& fight, uint32_t playersCreature)
{
	const auto own = static_cast<int64_t>(playersCreature);
	const auto opponent = static_cast<int64_t>(fight.first == playersCreature ? fight.second : fight.first);
	LessonCheck check;
	// Each stage counts only after the stage before it, as the lesson waits for them in turn
	for (const auto& entry : fight.entries)
	{
		const auto creature = NumberOf(entry, "creature");
		if (!check.hit.has_value())
		{
			if (entry.kind == "action" && creature == opponent && TextOf(entry, "fight_action").starts_with("reeling_"))
			{
				check.hit = entry.index;
			}
		}
		else if (!check.hits.has_value())
		{
			if (entry.kind == "move_queued" && creature == own && NumberOf(entry, "blows_queued").value_or(0) > 2)
			{
				check.hits = entry.index;
			}
		}
		else if (!check.block.has_value())
		{
			if (entry.kind == "action" && creature == own && TextOf(entry, "fight_action") == "block")
			{
				check.block = entry.index;
			}
		}
		else if (!check.step.has_value())
		{
			if (entry.kind == "action" && creature == own && TextOf(entry, "fight_action") == "step")
			{
				check.step = entry.index;
			}
		}
	}
	return check;
}
