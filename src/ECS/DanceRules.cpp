/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DanceRules.h"

#include <algorithm>
#include <ranges>

#include <DanceFile.h>

using namespace openblack;
using namespace openblack::ecs;
using dance_rules::Groups;

namespace
{
/// The dancers a dance expects before it starts; the game never sets it, so one dancer will do
constexpr uint32_t k_ExpectedDancers = 0;
/// A dance that has waited this many seconds for its dancers starts anyway
constexpr uint32_t k_LongestWait = 90;
/// Beats in a loop of the dance for each half second of turns
constexpr uint32_t k_BeatsPerHalfSecond = 120;

/// The game's float to whole number, truncated
int32_t Truncate(float value)
{
	return static_cast<int32_t>(value);
}

void AddOnce(std::vector<std::size_t>& list, std::size_t group)
{
	if (std::ranges::find(list, group) == list.end())
	{
		list.push_back(group);
	}
}

void SetMembership(Groups& groups, std::size_t index, uint32_t limit, uint32_t quota)
{
	auto& group = groups.all[index];
	// A group in a shape keeps its membership
	if (group.formation != 0)
	{
		return;
	}
	group.limited = limit != 0;
	group.quota = quota;
	if (group.limited)
	{
		std::erase(groups.shared, index);
		AddOnce(groups.limited, index);
		return;
	}
	std::erase(groups.limited, index);
	AddOnce(groups.shared, index);
	dance_rules::SetWeights(groups);
}

void ApplyAction(Groups& groups, std::size_t index, const dance::DanceAction& action)
{
	auto& group = groups.all[index];
	switch (static_cast<dance_rules::ActionType>(action.type))
	{
	case dance_rules::ActionType::Membership:
		SetMembership(groups, index, action.arguments[0], action.arguments[1]);
		break;
	case dance_rules::ActionType::Formation:
		group.formation = action.arguments[0];
		break;
	case dance_rules::ActionType::DanceType:
		group.danceType = action.arguments[0];
		break;
	case dance_rules::ActionType::Sexes:
		group.sexes = action.arguments[0];
		break;
	default:
		// TODO(opening): the actions that move the dancers about: their shape, turning, steps and clips
		break;
	}
}
} // namespace

void dance_rules::SetWeights(Groups& groups)
{
	uint32_t largest = 0;
	for (const auto index : groups.shared)
	{
		largest = std::max(largest, groups.all[index].quota);
	}
	if (largest < 1)
	{
		return;
	}
	auto divisor = largest;
	const auto dividesAll = [&groups](uint32_t d) {
		return std::ranges::all_of(groups.shared, [&groups, d](std::size_t index) { return groups.all[index].quota % d == 0; });
	};
	while (divisor > 1 && !dividesAll(divisor))
	{
		--divisor;
	}
	for (const auto index : groups.shared)
	{
		groups.all[index].weight = groups.all[index].quota / divisor;
	}
	groups.roundLength = 100 / divisor;
}

void dance_rules::ApplyKeyFrame(Groups& groups, const dance::DanceKeyFrame& keyFrame)
{
	for (const auto& action : keyFrame.actions)
	{
		for (const auto index : action.groups | std::views::reverse)
		{
			while (groups.all.size() <= index)
			{
				groups.all.emplace_back();
			}
			ApplyAction(groups, index, action);
		}
	}
}

void dance_rules::ApplyKeyFramesUpTo(Groups& groups, const dance::DanceFile& file, float beat)
{
	for (const auto& keyFrame : file.keyFrames)
	{
		if (!(keyFrame.time <= beat))
		{
			break;
		}
		ApplyKeyFrame(groups, keyFrame);
	}
	SetWeights(groups);
}

std::optional<std::size_t> dance_rules::AddDancer(Groups& groups, entt::entity dancer, uint32_t danceType, uint32_t sex)
{
	const auto join = [&groups, dancer](std::size_t index) {
		groups.all[index].dancers.push_back(dancer);
		++groups.dancers;
		return index;
	};
	for (const auto index : groups.limited)
	{
		auto& group = groups.all[index];
		if (group.danceType == danceType && group.limitedDancers < group.quota && (group.sexes & sex) != 0)
		{
			++group.limitedDancers;
			return join(index);
		}
	}
	uint32_t reach = 0;
	for (const auto index : groups.shared)
	{
		const auto& group = groups.all[index];
		reach += group.weight;
		if (group.danceType == danceType && groups.round < reach && (group.sexes & sex) != 0)
		{
			++groups.round;
			if (groups.round >= groups.roundLength)
			{
				groups.round = 0;
			}
			return join(index);
		}
	}
	return std::nullopt;
}

void dance_rules::RemoveDancer(Groups& groups, std::size_t index, entt::entity dancer)
{
	if (index >= groups.all.size())
	{
		return;
	}
	auto& group = groups.all[index];
	--groups.dancers;
	if (group.limited)
	{
		--group.limitedDancers;
	}
	std::erase(group.dancers, dancer);
}

entt::entity dance_rules::FirstDancer(const Groups& groups, entt::entity exclude)
{
	for (const auto& group : groups.all)
	{
		const auto found = std::ranges::find_if(group.dancers, [exclude](entt::entity d) { return d != exclude; });
		if (found != group.dancers.end())
		{
			return *found;
		}
	}
	return entt::null;
}

bool dance_rules::KeyFrameDue(float keyFrameTime, float beat, uint32_t turnsPerSecond)
{
	const auto half = static_cast<int32_t>(turnsPerSecond / 2);
	return Truncate(keyFrameTime) / half == Truncate(beat) / half;
}

const dance::DanceKeyFrame* dance_rules::DueKeyFrame(const dance::DanceFile& file, float beat, uint32_t turnsPerSecond)
{
	const auto found = std::ranges::find_if(file.keyFrames, [beat, turnsPerSecond](const dance::DanceKeyFrame& keyFrame) {
		return KeyFrameDue(keyFrame.time, beat, turnsPerSecond);
	});
	return found != file.keyFrames.end() ? &*found : nullptr;
}

float dance_rules::NextBeat(float beat, uint32_t loops, uint32_t turnsPerSecond)
{
	const float next = beat + 1.0f;
	const auto loopBeats = static_cast<uint32_t>(turnsPerSecond / 2 * loops * k_BeatsPerHalfSecond);
	return static_cast<uint32_t>(Truncate(next)) >= loopBeats ? 0.0f : next;
}

bool dance_rules::ReadyToStart(uint32_t dancers, bool& waiting, uint32_t& waitStartTurn, uint32_t turn, uint32_t turnsPerSecond)
{
	if (dancers == 0)
	{
		return false;
	}
	if (!waiting)
	{
		waiting = true;
		waitStartTurn = turn;
	}
	return dancers > k_ExpectedDancers / 2 || (turn - waitStartTurn) / turnsPerSecond >= k_LongestWait;
}
