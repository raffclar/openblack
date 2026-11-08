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

#include <bit>
#include <optional>

/// How a workshop shows the scaffolds of its yard that are away. When the hand takes a scaffold from its place and the
/// workshop still counts the scaffold, a see-through copy of the scaffold pulses at the empty place. These rules use
/// only the frames: the game turns do not read them.
namespace openblack::ecs::yard_copy
{

/// The copy pulses on a clock of milliseconds that goes round every two seconds
inline constexpr int32_t k_PeriodMs = 2000;
/// The angle of one full turn of the pulse, as the game keeps it in a float
inline constexpr float k_FullTurn = std::bit_cast<float>(0x40C90FDBU); // 2 pi
/// The pulse goes from no alpha to a third of the alpha of the land's light
inline constexpr float k_Third = std::bit_cast<float>(0x3EAAAAABU); // 1/3
/// The alpha of the land's light at a place on the map
inline constexpr int32_t k_LandAlpha = 255;
/// The workshop changes the alpha reference of the copy's materials only when this value of its alpha is more than 0:
/// the alpha times 250 divided by 255, less 10
inline constexpr int32_t k_ReferenceScale = 250;
inline constexpr int32_t k_ReferenceLess = 10;
/// The alpha reference that the copy then uses in place of the references of its materials. The game uses the
/// reference that the last draw of a different see-through thing set. Before such a draw, that reference is 0.
inline constexpr uint8_t k_ChangedReference = 0;

/// The clock of the pulse after one workshop draws its yard in a frame. The clock moves on by the frame's game time
/// `frameMs`. When it goes past the full period, it starts again from the remainder. Each workshop that the frame draws
/// moves the clock again, because all workshops use one clock.
[[nodiscard]] constexpr int32_t StepClock(int32_t clockMs, int32_t frameMs)
{
	const int32_t clock = clockMs + frameMs;
	return clock > k_PeriodMs ? clock % k_PeriodMs : clock;
}

/// The alpha of a copy at the clock `clockMs` on a land whose light has the alpha `landAlpha`: 0 to a third of it
[[nodiscard]] uint8_t Alpha(int32_t clockMs, int32_t landAlpha = k_LandAlpha);

/// Whether the copy with the alpha `alpha` uses `k_ChangedReference` in place of the alpha references of its materials
[[nodiscard]] constexpr bool ChangesReference(uint8_t alpha)
{
	return (static_cast<int32_t>(alpha) * k_ReferenceScale) / 255 - k_ReferenceLess > 0;
}

/// The alpha reference that the copy uses in place of the references of its materials, none when it uses the references
/// of its materials
[[nodiscard]] constexpr std::optional<uint8_t> SeeThroughRef(uint8_t alpha)
{
	return ChangesReference(alpha) ? std::optional<uint8_t>(k_ChangedReference) : std::nullopt;
}

} // namespace openblack::ecs::yard_copy
