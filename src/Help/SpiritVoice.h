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

#include <array>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// What an advisor's voice gives its body: the gesture tags of the line it says and its mouth's lip sync
namespace openblack::help::spirits
{

/// One audio tag (a 20-byte record): time, who, action, index, value
struct AudioTag
{
	float time {0.0f};
	int32_t who {0};    ///< 0 the speaker (T), 1 the other (O, !), 2 the good one (G), 3 the evil one (E), 4 both (B, *)
	int32_t action {0}; ///< 1 A play, 2 AS loop, 3 AR / R release, 4 E emotion, 5 L / 6 LS look mode, 7 LR restore
	int32_t index {0};  ///< anim (prefix match), emotion or look mode
	int32_t value {100};
};
/// The audio tag builder's per-label loop: one tag parsed at a time until the NUL, "[" then
/// "<talker><type> <name>[digits]" entries; each tag gets the cue's time. `errors` counts the "unrecognised ..." cases.
/// `lastWord` is the parser's word buffer, which keeps its word from one call to the next: a tag with no name takes the
/// last one read.
[[nodiscard]] std::vector<AudioTag> ParseAudioTags(std::string_view label, float time, std::string& lastWord,
                                                   int* errors = nullptr);

/// One cue label of a voice recording and its time in seconds from the start
struct TagLabel
{
	std::string text;
	float time;
};
/// A sentence's tags from all its labels in order; any error in any label leaves it with none
[[nodiscard]] std::vector<AudioTag> BuildSentenceTags(std::span<const TagLabel> labels, std::string& lastWord);

/// What the lip sync left for the mouth
struct LipSyncFrame
{
	/// Seconds since the line started by the computer's clock, or, once the sound reports a play position, that
	/// position. The line's tags fire on it in both cases
	float time {0.0f};
	/// The sound reported a play position: the mouth shapes are shown
	bool playing {false};
	/// The mouth shapes' weights, as the last lip sync step left them
	std::array<float, 3> weights {};
};

} // namespace openblack::help::spirits
