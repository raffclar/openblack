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

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <LHVMTypes.h>
#include <glm/vec3.hpp>

/// The scripts' records of a challenge: one is taken when a challenge starts, and brought up to date as it goes on
namespace openblack::script::challenge_snapshots
{

/// The script calls that take or update a challenge's record
enum class Call
{
	/// A challenge starts: its title, the script that reminds the player of it and that script's arguments, how far it
	/// has got, its alignment, the camera's focus and place, and whether it is a quest
	Start,
	/// A challenge goes on: the same without the camera or quest
	Update,
	/// The picture of a challenge is taken: the camera, but no reminder script
	Picture,
};

/// One of the values a script gave, as it was on the stack
struct Value
{
	lhvm::VMValue value;
	lhvm::DataType type {lhvm::DataType::None};
};

/// What a script gave for a challenge's record
struct Snapshot
{
	uint32_t challenge {0};
	/// The values the reminder script is to be given, the last given first
	std::vector<Value> arguments;
	/// More arguments were given than a record keeps
	bool tooManyArguments {false};
	std::string reminderScript;
	uint32_t title {0};
	/// From -1 (evil) to 1 (good)
	float alignment {0.0f};
	/// How far the challenge has got, from 0 to 1
	float success {0.0f};
	std::optional<glm::vec3> focus;
	std::optional<glm::vec3> position;
	bool quest {false};
	bool takingPicture {false};
};

/// How the record's values come off the script's stack: the top value, and the text at an offset of the scripts' data
struct Stack
{
	std::function<Value()> pop;
	std::function<std::string(uint32_t offset)> text;
};

/// The most arguments a record of a call keeps for its reminder script
[[nodiscard]] size_t MaxArguments(Call call);
/// An alignment a script gave, kept to -1 to 1; anything that isn't a number is taken as -1
[[nodiscard]] float ClampAlignment(float alignment);
/// How far a challenge has got, kept to 0 to 1; anything that isn't a number is taken as 0
[[nodiscard]] float ClampSuccess(float success);
/// Takes every value a call was given off the stack, in the order the call takes them: the challenge, then for a start
/// or an update the number of arguments, the arguments and the reminder script, for a picture whether it is being
/// taken; then the title, the alignment and how far the challenge has got; then for a start or a picture the camera's
/// focus and place, and for a start whether it is a quest
[[nodiscard]] Snapshot Read(Call call, const Stack& stack);

} // namespace openblack::script::challenge_snapshots
