/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// A dance's choreography, the files in Scripts/Dance: little-endian, a version, the key frames with their actions,
/// the dance's settings and, from version 1, the names of its groups of dancers.
namespace openblack::dance
{

enum class DanceResult : uint8_t
{
	Success = 0,
	ErrTruncated,
	ErrTrailingBytes,
};

std::string_view ResultToStr(DanceResult result);

/// One thing the dance does to some of its groups at a key frame
struct DanceAction
{
	/// The groups it is done to, by number, in the file's order
	std::vector<uint32_t> groups;
	/// What it does
	uint32_t type {0};
	/// What it is done with: 56 bytes the type reads as whole numbers or floats
	std::array<uint32_t, 14> arguments {};

	/// An argument read as a float
	[[nodiscard]] float Float(std::size_t index) const;
};

/// What happens at a beat of the dance
struct DanceKeyFrame
{
	/// The beat it happens at
	float time {0.0f};
	/// 1 on the first key frame of the game's dances, 0 on the others; read by nothing traced
	uint32_t flags {0};
	std::vector<DanceAction> actions;
};

struct DanceFile
{
	uint32_t version {0};
	std::vector<DanceKeyFrame> keyFrames;
	/// The beat the dance stood at when it was saved: 0 in every file of the game
	float beat {0.0f};
	/// How many groups of dancers it has
	uint32_t groupCount {0};
	/// Two settings not yet traced
	uint32_t unknown44 {0};
	uint32_t unknown48 {0};
	/// How many times 120 beats a turn's half-seconds' worth the dance runs before it starts again
	uint32_t loops {0};
	/// From version 2: the way the dance faces
	std::optional<float> angle;
	/// From version 1: each group's name
	std::vector<std::string> groupNames;

	/// Reads a dance from its bytes, which must all be read
	DanceResult Open(std::span<const uint8_t> buffer);
};

} // namespace openblack::dance
