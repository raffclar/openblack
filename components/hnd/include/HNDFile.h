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
#include <span>
#include <string_view>
#include <vector>

/// A hand demonstration, the files in Data/HandDemo: a recording of the player's interface, one little-endian record
/// of 124 bytes for each message the interface got (the cursor moving, a button going down or up), with the camera, the
/// camera hints and the time it was at, and the marks the person recording left for the scripts. No header.
namespace openblack::hnd
{

enum class HNDResult : uint8_t
{
	Success = 0,
	/// The file isn't a whole number of records
	ErrPartialRecord,
};

std::string_view ResultToStr(HNDResult result);

/// The interface's messages a record replays
enum class HNDMessage : uint32_t
{
	/// The cursor moved
	Move = 0,
	/// The button that grips and moves the land went down, or up
	MoveButtonDown = 1,
	MoveButtonUp = 2,
	/// The action button went down, or up
	ActionButtonDown = 3,
	ActionButtonUp = 4,
	/// A double click, and one more the files don't use
	DoubleClick = 5,
	Other = 6,
};

/// A map position as the game keeps it: x and z in its fixed point (65536 to a 10 metre cell), and the height
struct HNDMapPosition
{
	int32_t x {0};
	int32_t z {0};
	float altitude {0.0f};
};

struct HNDRecord
{
	HNDMessage message {HNDMessage::Move};
	/// Where the hand placed what it held, as the hand keeps it: three points and three angles
	std::array<float, 12> heldPlacement {};
	/// The cursor across the screen, and down the 16:9 picture between the cinema bars, from 0 to 1
	std::array<float, 2> cursor {};
	std::array<float, 3> cameraPosition {};
	std::array<float, 3> cameraFocus {};
	/// The camera hints shown about the hand, and their value (the turn's angle while rotating)
	uint32_t hints {0};
	float hintValue {0.0f};
	/// Not 0 where the person recording marked a point for the scripts to wait for
	uint32_t trigger {0};
	/// The game's time when it was recorded, in hundredths of a turn (milliseconds)
	uint32_t time {0};
	/// For a button: the thing under the hand, its distance, place and kind; the kind's subtype 9999 for none
	float objectDistance {0.0f};
	HNDMapPosition objectPosition;
	uint32_t objectType {0};
	uint32_t objectSubtype {0};
};

/// The subtype of a record with no thing under the hand
constexpr uint32_t k_NoObject = 9999;
/// The size of a record in the file
constexpr size_t k_RecordSize = 0x7C;

struct HNDFile
{
	std::vector<HNDRecord> records;

	/// Reads a demonstration from its bytes
	HNDResult Open(std::span<const uint8_t> buffer);
};

} // namespace openblack::hnd
