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
#include <vector>

#include <DanceFile.h>

#include "ECS/DanceMoves.h"

using namespace openblack;
using namespace openblack::ecs;
using openblack::ecs::components::Dance;
using openblack::ecs::components::DanceGroups;

namespace
{
/// The fastest rate, at a speed of 1
constexpr float k_MostRate = 4.0f;
/// A loop length is this many half seconds long
constexpr uint32_t k_HalfSecondsPerLoopLength = 120;

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

void SetMembership(DanceGroups& groups, std::size_t index, uint32_t limit, uint32_t quota)
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

/// One of a key frame's actions done to a group: membership by the rules here, the rest by the dance's moves. True when
/// the group starts a move.
bool ApplyAction(Dance& dance, std::size_t index, const dance::DanceAction& action, int32_t now)
{
	auto& group = dance.groups.all[index];
	switch (static_cast<dance_rules::ActionType>(action.type))
	{
	case dance_rules::ActionType::Membership:
		SetMembership(dance.groups, index, action.arguments[0], action.arguments[1]);
		return false;
	case dance_rules::ActionType::Formation:
		group.formation = action.arguments[0];
		return false;
	case dance_rules::ActionType::DanceType:
		group.danceType = action.arguments[0];
		return false;
	case dance_rules::ActionType::Sexes:
		group.sexes = action.arguments[0];
		return false;
	default:
		return dance_moves::ApplyAction(dance, index, action.type, action.arguments, now);
	}
}

} // namespace

float dance_rules::RateForSpeed(float speed)
{
	// Rounded towards zero to tenths
	const auto tenths = static_cast<int32_t>(speed * 10.0f);
	return static_cast<float>(tenths) * 0.1f * k_MostRate;
}

void dance_rules::SetSpeed(Dance& dance, float speed)
{
	dance.rate = RateForSpeed(speed);
	if (dance.state == Dance::State::Dancing && dance.rate != dance.dancingRate)
	{
		dance.dancingRate = dance.rate;
		dance.clock = 0.0f;
	}
	dance.speed = speed;
}

void dance_rules::SetWorshipSpeed(Dance& dance, float intensity)
{
	if (intensity > 0.0f && dance.state == Dance::State::Stopped)
	{
		dance.state = Dance::State::Dancing;
	}
	else if (intensity <= 0.0f && dance.state == Dance::State::Dancing)
	{
		dance.state = Dance::State::Stopped;
	}
	SetSpeed(dance, intensity);
}

bool dance_rules::HasProperlyStarted(Dance& dance, uint32_t turn)
{
	if (dance.dancers == 0)
	{
		return false;
	}
	if (!dance.firstDancerTurn.has_value())
	{
		dance.firstDancerTurn = turn;
	}
	return dance.dancers > dance.onTheirWay / 2 || (turn - *dance.firstDancerTurn) / k_TurnsPerSecond >= k_LongestWaitSeconds;
}

uint32_t dance_rules::LoopTurns(const Dance& dance)
{
	return k_TurnsPerSecond / 2 * dance.loopLength * k_HalfSecondsPerLoopLength;
}

void dance_rules::SetWeights(DanceGroups& groups)
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

std::vector<std::size_t> dance_rules::ApplyKeyFrame(Dance& dance, const dance::DanceKeyFrame& keyFrame)
{
	std::vector<std::size_t> moving;
	const auto now = Truncate(dance.clock);
	for (const auto& action : keyFrame.actions)
	{
		for (const auto index : action.groups | std::views::reverse)
		{
			while (dance.groups.all.size() <= index)
			{
				dance.groups.all.emplace_back();
			}
			if (ApplyAction(dance, index, action, now) && std::ranges::find(moving, index) == moving.end())
			{
				moving.push_back(index);
			}
		}
	}
	return moving;
}

void dance_rules::ApplyKeyFramesUpTo(Dance& dance, const dance::DanceFile& file, float clock)
{
	for (const auto& keyFrame : file.keyFrames)
	{
		if (!(keyFrame.time <= clock))
		{
			break;
		}
		static_cast<void>(ApplyKeyFrame(dance, keyFrame));
	}
	SetWeights(dance.groups);
}

std::optional<std::size_t> dance_rules::AddDancer(Dance& dance, entt::entity dancer, uint32_t danceType, uint32_t sex)
{
	auto& groups = dance.groups;
	const auto join = [&dance, dancer](std::size_t index) {
		dance.groups.all[index].dancers.push_back(dancer);
		++dance.dancers;
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

void dance_rules::RemoveDancer(Dance& dance, std::size_t index, entt::entity dancer)
{
	if (index >= dance.groups.all.size())
	{
		return;
	}
	auto& group = dance.groups.all[index];
	if (dance.dancers > 0)
	{
		--dance.dancers;
	}
	if (group.limited)
	{
		--group.limitedDancers;
	}
	std::erase(group.dancers, dancer);
}

entt::entity dance_rules::FirstDancer(const DanceGroups& groups, entt::entity exclude)
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

bool dance_rules::KeyFrameDue(float keyFrameTime, float clock)
{
	constexpr auto k_Half = static_cast<int32_t>(k_TurnsPerSecond / 2);
	return Truncate(keyFrameTime) / k_Half == Truncate(clock) / k_Half;
}

const dance::DanceKeyFrame* dance_rules::DueKeyFrame(const dance::DanceFile& file, float clock)
{
	const auto found = std::ranges::find_if(
	    file.keyFrames, [clock](const dance::DanceKeyFrame& keyFrame) { return KeyFrameDue(keyFrame.time, clock); });
	return found != file.keyFrames.end() ? &*found : nullptr;
}

float dance_rules::NextClock(float clock, uint32_t loopLength)
{
	const float next = clock + 1.0f;
	const auto loopTurns = k_TurnsPerSecond / 2 * loopLength * k_HalfSecondsPerLoopLength;
	return static_cast<uint32_t>(Truncate(next)) >= loopTurns ? 0.0f : next;
}

std::vector<std::size_t> dance_rules::ProcessTurn(Dance& dance, uint32_t turn)
{
	if (dance.state == Dance::State::Stopped && dance.autostart && HasProperlyStarted(dance, turn))
	{
		dance.state = Dance::State::Dancing;
		dance.startTurn = turn;
	}
	else if (dance.duration > 0 && turn - dance.startTurn > dance.duration)
	{
		// TODO(opening): its lights go out
		dance.state = Dance::State::Stopped;
	}
	if (dance.state != Dance::State::Dancing || dance.dancers == 0)
	{
		return {};
	}
	// TODO(opening): a town's dance counts the turns the camera is near it
	std::vector<std::size_t> moving;
	if (dance.file != nullptr)
	{
		if (const auto* keyFrame = DueKeyFrame(*dance.file, dance.clock))
		{
			moving = ApplyKeyFrame(dance, *keyFrame);
		}
	}
	// Each group's move goes on, in the order the groups were made
	const auto now = Truncate(dance.clock);
	for (auto& group : dance.groups.all)
	{
		dance_moves::ProcessGroup(group, now);
	}
	// TODO(opening): the dance's lights
	dance.clock = NextClock(dance.clock, dance.loopLength);
	return moving;
}
