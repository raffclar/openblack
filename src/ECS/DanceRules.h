/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>
#include <string>
#include <vector>

#include <entt/entity/entity.hpp>

namespace openblack::dance
{
struct DanceFile;
struct DanceKeyFrame;
} // namespace openblack::dance

/// How a dance shares its dancers between its groups and steps through its beats
namespace openblack::ecs::dance_rules
{

/// Which dancers a group takes: men, women, or both (anything not a villager counts as both)
inline constexpr uint32_t k_Men = 1;
inline constexpr uint32_t k_Women = 2;
inline constexpr uint32_t k_AnySex = k_Men | k_Women;

/// What a key frame's action does to a group (the rest move the dancers about and are not ported yet)
enum class ActionType : uint32_t
{
	/// The group's shape about the dance's place
	Formation = 14,
	/// Whether the group takes a fixed number of dancers, or a share of them
	Membership = 6,
	/// The kind of dancer the group takes
	DanceType = 15,
	/// The sexes the group takes
	Sexes = 16,
};

/// A group of dancers dancing one part
struct Group
{
	std::string name;
	/// Its dancers in the order they joined: a dancer's place in the group is its place here
	std::vector<entt::entity> dancers;
	/// Taking a fixed number of dancers: `quota` of them. Else a share of the dance's dancers: `quota` in a hundred.
	bool limited {false};
	uint32_t quota {100};
	/// Its share of each round of newcomers, its quota over what the shared groups' quotas have in common
	uint32_t weight {100};
	/// How many of its dancers it took as a group with a fixed number
	uint32_t limitedDancers {0};
	uint32_t danceType {0};
	uint32_t sexes {k_AnySex};
	/// Its shape about the dance's place; a group with one keeps its membership
	uint32_t formation {0};
};

/// A dance's groups, in the order they were made, and the order newcomers try them in
struct Groups
{
	std::vector<Group> all;
	/// The groups with a fixed number of dancers, tried first in this order
	std::vector<std::size_t> limited;
	/// The groups sharing the dancers between them, in this order, a newcomer going to each in turn by its weight
	std::vector<std::size_t> shared;
	/// Where the round of newcomers is, and how long a round is
	uint8_t round {0};
	uint32_t roundLength {0};
	uint32_t dancers {0};
};

/// The groups' weights in a round: each shared group's quota over the largest number that divides them all, and the
/// round 100 over it. Nothing changes when no shared group has a quota.
void SetWeights(Groups& groups);

/// What a key frame does to the groups' membership, each action done to its groups the last listed first. A group
/// numbered past the last is made.
void ApplyKeyFrame(Groups& groups, const dance::DanceKeyFrame& keyFrame);
/// Every key frame up to a beat, as a dance does once its file is read
void ApplyKeyFramesUpTo(Groups& groups, const dance::DanceFile& file, float beat);

/// A newcomer joins the first group with a fixed number of dancers that has room and takes its dance type and sex,
/// else the shared group whose turn it is in the round that takes them. None when no group does.
std::optional<std::size_t> AddDancer(Groups& groups, entt::entity dancer, uint32_t danceType, uint32_t sex);
/// A dancer leaves its group; those after it move up a place
void RemoveDancer(Groups& groups, std::size_t group, entt::entity dancer);
/// The first dancer of the first group that has one other than `exclude`
[[nodiscard]] entt::entity FirstDancer(const Groups& groups, entt::entity exclude);

/// Whether a key frame is the one for a beat: the same half second of beats
[[nodiscard]] bool KeyFrameDue(float keyFrameTime, float beat, uint32_t turnsPerSecond);
/// The first key frame due at a beat, if any
[[nodiscard]] const dance::DanceKeyFrame* DueKeyFrame(const dance::DanceFile& file, float beat, uint32_t turnsPerSecond);
/// The next beat: one on, back to 0 after 120 half seconds' worth for each of the dance's loops
[[nodiscard]] float NextBeat(float beat, uint32_t loops, uint32_t turnsPerSecond);

/// Whether a dance waiting to start starts: once it has dancers, when more than half `half` (none in the game) have
/// come, or after 90 seconds of waiting. The wait starts the first turn it has a dancer.
[[nodiscard]] bool ReadyToStart(uint32_t dancers, bool& waiting, uint32_t& waitStartTurn, uint32_t turn,
                                uint32_t turnsPerSecond);

} // namespace openblack::ecs::dance_rules
