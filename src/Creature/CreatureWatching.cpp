/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureWatching.h"

#include <cmath>

#include <algorithm>

#include "Common/X87.h"

using namespace openblack;
using namespace openblack::creature_watching;

namespace
{
/// Short of learning a skill, the meter shows it half learnt
constexpr float k_SkillMeterShare = 0.5f;
/// A miracle seen this many times or more beyond those it needed no longer fills the learning meter
constexpr float k_LearntMeterSightings = 3.0f;
/// Seen this share of the times it needs, a miracle is nearly learnt
constexpr float k_NearlyLearntShare = 0.75f;

/// The number of actions of a stage. The game draws a number from 0 to 0 for it, so the draw changes nothing, but it
/// uses the random numbers.
uint32_t StageLength(const MimicRule& rule, const std::function<uint32_t(uint32_t)>& random)
{
	return rule.stageSteps + random(1);
}
} // namespace

Knowledge creature_watching::StartKnowledge(std::span<const SkillRule> skills, std::span<const MiracleRule> miracles)
{
	Knowledge knowledge {
	    .skillsSeen = std::vector<Sighting>(skills.size()),
	    .miraclesSeen = std::vector<Sighting>(miracles.size()),
	    .skillsKnown = std::vector<bool>(skills.size(), false),
	    .miraclesKnown = std::vector<bool>(miracles.size(), false),
	};
	return knowledge;
}

Progress creature_watching::SeeSkill(Knowledge& knowledge, size_t skill, std::span<const SkillRule> rules, uint32_t phase,
                                     uint32_t turn, float turnsPerSecond)
{
	if (skill >= rules.size() || skill >= knowledge.skillsSeen.size() || skill >= knowledge.skillsKnown.size())
	{
		return {.ignored = true};
	}
	const auto& rule = rules[skill];
	// Too young, it is told so even when it knows the skill
	if (phase < rule.minPhase)
	{
		return {.ignored = true, .event = LearningEvent::TooYoung};
	}
	auto& seen = knowledge.skillsSeen[skill];
	// A first sighting on turn 0 counts as none, so the next sighting starts the clock again
	if (!seen.turn.has_value() || *seen.turn == 0)
	{
		seen.turn = turn;
	}
	++seen.count;
	const bool knew = knowledge.skillsKnown[skill];
	// Whole seconds, from whole turns a second
	const auto turnsASecond = std::max(static_cast<uint32_t>(turnsPerSecond), 1U);
	const auto seconds = (turn - *seen.turn) / turnsASecond;
	if (static_cast<float>(seconds) >= rule.watchSeconds)
	{
		knowledge.skillsKnown[skill] = true;
		if (knew)
		{
			return {.learnt = true, .share = 1.0f, .ignored = true};
		}
		return {.learnt = true, .share = 1.0f, .event = LearningEvent::Learnt, .meter = 1.0f};
	}
	if (knew)
	{
		return {.learnt = true, .share = 1.0f, .ignored = true};
	}
	return {.share = k_SkillMeterShare, .event = LearningEvent::NearlyLearnt, .meter = k_SkillMeterShare};
}

bool creature_watching::MeterShows(uint32_t lastShown, uint32_t turn)
{
	return turn - lastShown > k_MeterTurns;
}

uint32_t creature_watching::StateToLearnFrom(const VillagerAtWork& villager)
{
	const auto state = villager.state == k_WaitForAnimationState ? villager.finalState : villager.state;
	if (state == k_GoToStoragePitForFoodState && villager.distanceToGoal > k_FarFromPitMetres)
	{
		return k_DecideWhatToDoState;
	}
	return state;
}

bool creature_watching::WatchesVillagers(const Watcher& watcher)
{
	using creature_desires::Desire;
	if (watcher.phase < k_MinWatchPhase || watcher.scriptControlled)
	{
		return false;
	}
	if (!watcher.planDesire.has_value())
	{
		return true;
	}
	switch (*watcher.planDesire)
	{
	case Desire::Impress:
	case Desire::Compassion:
	case Desire::Anger:
	case Desire::Play:
	case Desire::Hunger:
	case Desire::Fear:
	case Desire::Poo:
	case Desire::Tiredness:
	case Desire::Puke:
	case Desire::Water:
	case Desire::RestoreHealth:
	case Desire::BeFriends:
	case Desire::GetWarmer:
	case Desire::GetColder:
	case Desire::Scratch:
	case Desire::RunAwayFromPlayer:
	case Desire::Rest:
	case Desire::Illness:
	case Desire::ObeyCreature:
	case Desire::Steal:
		return false;
	default:
		return true;
	}
}

float creature_watching::TimesNeeded(uint32_t timesToSee, float speciesMultiplier)
{
	// The count is exact in the multiply, and the product rounds to a float
	return x87::Mul(static_cast<double>(timesToSee), speciesMultiplier);
}

uint32_t creature_watching::TaughtSightings(uint32_t timesToSee, float speciesMultiplier)
{
	// The float product is cut towards zero. 10 times 0.7 rounds to 7 before the cut
	const float times = TimesNeeded(timesToSee, speciesMultiplier);
	return times > 0.0f ? static_cast<uint32_t>(times) : 0;
}

bool creature_watching::SetKnown(Knowledge& knowledge, KnownList list, size_t index, bool knows, uint32_t taughtSightings)
{
	auto& known = list == KnownList::Skill ? knowledge.skillsKnown : knowledge.miraclesKnown;
	if (index >= known.size())
	{
		return false;
	}
	const bool newlyKnown = knows && !known[index];
	known[index] = knows;
	if (knows && list == KnownList::Miracle && index < knowledge.miraclesSeen.size())
	{
		knowledge.miraclesSeen[index].count = taughtSightings;
	}
	return newlyKnown;
}

bool creature_watching::KnowsWhatItNeeds(const Knowledge& knowledge, const ActionNeeds& needs)
{
	const auto knowsOf = [](const std::vector<bool>& known, std::optional<size_t> index) {
		return !index.has_value() || (*index < known.size() && known[*index]);
	};
	return std::ranges::all_of(needs.skills,
	                           [&](std::optional<size_t> skill) { return knowsOf(knowledge.skillsKnown, skill); }) &&
	       knowsOf(knowledge.miraclesKnown, needs.miracle);
}

std::optional<Prerequisite> creature_watching::MiraclePrerequisite(size_t miracle)
{
	using Kind = Prerequisite::Kind;
	switch (miracle)
	{
	case 2: // the fireball's power-ups
	case 3:
		return Prerequisite {.kind = Kind::Miracle, .index = 1};
	case 5: // the lightning bolt's
	case 6:
		return Prerequisite {.kind = Kind::Miracle, .index = 4};
	case 8: // the explosion's first power-up, then its second
		return Prerequisite {.kind = Kind::Miracle, .index = 7};
	case 9:
		return Prerequisite {.kind = Kind::Miracle, .index = 8};
	case 11: // the heal's
		return Prerequisite {.kind = Kind::Miracle, .index = 10};
	case 17: // the storm with lightning, then the tornado
		return Prerequisite {.kind = Kind::Miracle, .index = 16};
	case 18:
		return Prerequisite {.kind = Kind::Miracle, .index = 17};
	case 40: // the thirst and itch spells need the skill of building
	case 41:
		return Prerequisite {.kind = Kind::Skill, .index = 0};
	default:
		return std::nullopt;
	}
}

Progress creature_watching::SeeMiracle(Knowledge& knowledge, size_t miracle, std::span<const MiracleRule> rules, uint32_t phase,
                                       uint32_t turn, uint32_t weight, float speciesMultiplier)
{
	if (miracle >= rules.size() || miracle >= knowledge.miraclesSeen.size() || miracle >= knowledge.miraclesKnown.size())
	{
		return {.ignored = true};
	}
	if (const auto needs = MiraclePrerequisite(miracle))
	{
		const auto& known = needs->kind == Prerequisite::Kind::Skill ? knowledge.skillsKnown : knowledge.miraclesKnown;
		if (needs->index >= known.size() || !known[needs->index])
		{
			return {.ignored = true};
		}
	}
	const auto& rule = rules[miracle];
	if (phase < rule.minPhase)
	{
		return {.ignored = true, .event = LearningEvent::TooYoung};
	}
	auto& seen = knowledge.miraclesSeen[miracle];
	// A sighting soon after the last doesn't count, but is remembered as the last
	if (!seen.turn.has_value() || turn - *seen.turn > k_MiracleSightingTurns)
	{
		seen.count += weight;
	}
	seen.turn = turn;
	knowledge.miraclesKnown[miracle] = true;
	const auto needed = TimesNeeded(rule.timesToSee, speciesMultiplier);
	const auto count = static_cast<float>(seen.count);
	if (needed <= count)
	{
		// The meter shows it full for the first few sightings beyond those needed
		Progress progress {.learnt = true, .share = 1.0f, .event = LearningEvent::Learnt};
		if (count - needed < k_LearntMeterSightings)
		{
			progress.meter = 1.0f;
		}
		return progress;
	}
	const float share = count / needed;
	return {.share = share,
	        .event = share >= k_NearlyLearntShare ? std::optional(LearningEvent::NearlyLearnt) : std::nullopt,
	        .meter = share};
}

const char* creature_watching::Name(MimicStage stage)
{
	switch (stage)
	{
	case MimicStage::Notice:
		return "noticing";
	case MimicStage::CopyAction:
		return "copying the action";
	case MimicStage::CopyDesire:
		return "copying the desire";
	}
	return "?";
}

bool creature_watching::WithinCopyReach(float distance, uint32_t sweepCells)
{
	return distance < static_cast<float>(sweepCells);
}

bool creature_watching::StartMimicry(std::optional<Mimicry>& mimicry, size_t rule, std::span<const MimicRule> rules,
                                     const MimicConditions& conditions, std::optional<uint32_t> object, uint32_t turn,
                                     const std::function<uint32_t(uint32_t)>& random)
{
	if (!MayStartMimicry(mimicry, rule, rules, conditions))
	{
		return false;
	}
	BeginMimicry(mimicry, rule, rules, object, turn, random);
	return true;
}

void creature_watching::BeginMimicry(std::optional<Mimicry>& mimicry, size_t rule, std::span<const MimicRule> rules,
                                     std::optional<uint32_t> object, uint32_t turn,
                                     const std::function<uint32_t(uint32_t)>& random)
{
	if (rule >= rules.size())
	{
		return;
	}
	mimicry = Mimicry {.rule = rule,
	                   .stage = MimicStage::Notice,
	                   .limit = StageLength(rules[rule], random),
	                   .stageStart = turn,
	                   .object = object};
}

bool creature_watching::MayStartMimicry(const std::optional<Mimicry>& mimicry, size_t rule, std::span<const MimicRule> rules,
                                        const MimicConditions& conditions)
{
	if (rule >= rules.size())
	{
		return false;
	}
	const auto& wanted = rules[rule];
	if (conditions.phase < k_MinMimicPhase || (wanted.needsLearningLeash && !conditions.learningLeashInHand) ||
	    conditions.reactionPriority >= k_MaxReactionPriorityToMimic || !conditions.canSee)
	{
		return false;
	}
	if (!mimicry.has_value())
	{
		return wanted.chance > 0.0f;
	}
	return mimicry->stage == MimicStage::CopyDesire || mimicry->rule >= rules.size() ||
	       (mimicry->rule != rule && wanted.chance >= rules[mimicry->rule].chance);
}

CastDeedStep creature_watching::CastDeedGate(uint32_t sightings, uint32_t timesToSee, float speciesMultiplier)
{
	// The times needed less the sightings, both floats, then cut towards zero
	const float needed = TimesNeeded(timesToSee, speciesMultiplier);
	const auto stillNeeded = static_cast<int64_t>(needed - static_cast<float>(sightings));
	if (stillNeeded > 1)
	{
		return CastDeedStep::CountAndStop;
	}
	if (stillNeeded == 1)
	{
		return CastDeedStep::CountThenRoll;
	}
	return CastDeedStep::Copy;
}

std::vector<uint32_t> creature_watching::CopyActionsOf(const MimicRule& rule, size_t deed, uint32_t magic,
                                                       std::span<const MiracleRule> miracles)
{
	if (deed != k_DamageWithMagicDeed)
	{
		return rule.copyActions;
	}
	if (magic >= miracles.size() || miracles[magic].action == 0)
	{
		return {};
	}
	return std::vector<uint32_t>(k_CopySlots, miracles[magic].action);
}

std::optional<Mimicry> creature_watching::CopyObservedDeedAgain(const ObservedDeed& observed, std::span<const MimicRule> rules,
                                                                uint32_t turn, const std::function<uint32_t(uint32_t)>& random)
{
	if (observed.rule >= rules.size())
	{
		return std::nullopt;
	}
	const auto limit = StageLength(rules[observed.rule], random);
	return Mimicry {
	    .rule = observed.rule,
	    .stage = MimicStage::CopyAction,
	    .done = limit / 2,
	    .limit = limit,
	    .stageStart = turn,
	    .object = observed.object,
	    .magic = observed.magic,
	};
}

bool creature_watching::StageContinues(const Mimicry& mimicry, uint32_t turn, float turnsPerSecond)
{
	const auto stageTurns = static_cast<uint32_t>(turnsPerSecond * k_MimicStageSeconds);
	return mimicry.done < mimicry.limit && (mimicry.stage != MimicStage::Notice || mimicry.done < 1) &&
	       turn - mimicry.stageStart <= stageTurns;
}

void creature_watching::NextMimicStage(std::optional<Mimicry>& mimicry, std::span<const MimicRule> rules, uint32_t turn,
                                       const std::function<uint32_t(uint32_t)>& random)
{
	if (!mimicry.has_value() || mimicry->rule >= rules.size())
	{
		mimicry.reset();
		return;
	}
	const auto& rule = rules[mimicry->rule];
	if (mimicry->stage == MimicStage::Notice)
	{
		mimicry->stage = MimicStage::CopyAction;
	}
	else if (mimicry->stage == MimicStage::CopyAction && rule.copiesDesire)
	{
		mimicry->stage = MimicStage::CopyDesire;
	}
	else
	{
		mimicry.reset();
		return;
	}
	mimicry->done = 0;
	mimicry->limit = StageLength(rule, random);
	mimicry->stageStart = turn;
}

void creature_watching::MimicActionDone(std::optional<Mimicry>& mimicry)
{
	if (mimicry.has_value())
	{
		++mimicry->done;
	}
}

void creature_watching::StrokedWhileMimicking(std::optional<Mimicry>& mimicry, std::span<const MimicRule> rules,
                                              const std::function<uint32_t(uint32_t)>& random)
{
	if (!mimicry.has_value() || mimicry->rule >= rules.size())
	{
		return;
	}
	mimicry->stage = MimicStage::CopyAction;
	mimicry->done = 0;
	mimicry->limit = StageLength(rules[mimicry->rule], random);
}

creature_watching::MimicKind creature_watching::KindToCopy(const MimicFacts& facts)
{
	using Class = MimicFacts::Class;
	switch (facts.kind)
	{
	case Class::Villager:
		if (facts.child)
		{
			return MimicKind::Child;
		}
		return facts.female ? MimicKind::Woman : MimicKind::Man;
	case Class::Animal:
		return facts.nasty ? MimicKind::NastyAnimal : MimicKind::NiceAnimal;
	case Class::Building:
		return MimicKind::Building;
	case Class::Tree:
		return MimicKind::Tree;
	case Class::Rock:
		return MimicKind::Rock;
	case Class::LooseObject:
		return MimicKind::LooseObject;
	case Class::Creature:
		return MimicKind::Creature;
	default:
		return MimicKind::Other;
	}
}

std::optional<size_t> creature_watching::NearestOfKind(std::span<const MimicCandidate> candidates, MimicKind kind)
{
	std::optional<size_t> nearest;
	float best = 0.0f;
	for (size_t i = 0; i < candidates.size(); ++i)
	{
		if (candidates[i].kind == kind && candidates[i].nearness > best)
		{
			best = candidates[i].nearness;
			nearest = i;
		}
	}
	return nearest;
}
