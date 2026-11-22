/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <filesystem>
#include <optional>
#include <string_view>

#include "Online/GodsRingMap.h"

/// When the turns of the next frame play beside the draw of this frame (deviation row 153).
///
/// The turns play beside the draw on the map for 32 gods, and in their frame on all other lands. The developer switch
/// --turn-beside-draw can make them play beside the draw on all lands, or never. The synced results are the same in
/// each mode: only the time at which a turn starts changes.
namespace openblack::turn_beside_draw
{

/// The value of the developer switch
enum class Mode : uint8_t
{
	/// Beside the draw on the map for 32 gods only
	ByLand,
	/// Beside the draw on all lands
	On,
	/// Never beside the draw
	Off,
};

/// The mode of a value of the switch: "auto", "on" or "off". Another value gives none.
[[nodiscard]] constexpr std::optional<Mode> ModeOf(std::string_view text) noexcept
{
	if (text == "auto")
	{
		return Mode::ByLand;
	}
	if (text == "on")
	{
		return Mode::On;
	}
	if (text == "off")
	{
		return Mode::Off;
	}
	return std::nullopt;
}

/// The land plays its turns beside the draw when the switch does not say otherwise. Only the map for 32 gods does: its
/// turns take most of a frame of 10 ms, so only they need the time of the draw.
[[nodiscard]] inline bool LandPlaysBeside(const std::filesystem::path& landPath)
{
	return landPath == std::filesystem::path(gods_ring_map::k_File);
}

/// The turns of the next frame play beside the draw of this frame
[[nodiscard]] constexpr bool PlaysBeside(Mode mode, bool landPlaysBeside) noexcept
{
	switch (mode)
	{
	case Mode::On:
		return true;
	case Mode::Off:
		return false;
	case Mode::ByLand:
		return landPlaysBeside;
	}
	return false;
}

} // namespace openblack::turn_beside_draw
