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

#include <map>
#include <optional>
#include <string_view>
#include <utility>

#include <entt/entity/entity.hpp>

#include "VideoRules.h"

/// The sounds of the falling spell's timeline: which bank and sample each cue plays or stops
namespace openblack::video
{

struct FallingSpellSound
{
	enum class Action : uint8_t
	{
		Play,
		Stop,
	};
	Action action {Action::Play};
	std::string_view bank;
	uint32_t sample {0};
	/// The sounds are kept apart by a small number, so that each loop can be stopped on its own
	uint32_t owner {0};
	/// Playback rate in percent, the bank's own when none
	std::optional<uint32_t> pitchPercent;

	bool operator==(const FallingSpellSound&) const = default;
};

/// The sound a cue plays or stops; none for the cues that aren't sounds (the music and the white fade)
[[nodiscard]] std::optional<FallingSpellSound> SoundOf(FallingSpellCue cue) noexcept;

/// Plays the falling spell's cues through the game's audio, the music and the temple's screen fade
class FallingSpellAudio
{
public:
	void Play(FallingSpellCue cue);

private:
	/// The sounds playing, by sample and owner, to stop them
	std::map<std::pair<uint32_t, uint32_t>, entt::entity> _playing;
};

} // namespace openblack::video
