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

#include <functional>
#include <optional>
#include <string>

#include <glm/vec3.hpp>

namespace openblack::audio
{

/// A music bank's group, its number of samples and how far it carries
struct MusicBankInfo
{
	int32_t groupId;
	uint32_t chunkCount;
	/// The first sample's far distance in its header, -1 for a bank without samples
	float maxDistance {-1.0f};
};

/// How the world asks for a music bank to play. The audio library gives these values to its music player unchanged.
struct MusicRequest
{
	/// Target volume, 0 to 127
	int32_t volume {127};
	/// 1-based chunk to start from, the first one if out of range
	uint32_t startChunk {1};
	/// Frames of the start chunk to leave out, so that the music starts part way through it
	uint32_t startFrame {0};
	/// Times to play the bank again after the first, -1 for ever. The bank header can override it.
	int32_t loops {0};
	/// Start in time with a channel that plays a bank of the same music group
	bool sync {false};
	/// Fade in from silence, not at full volume at once
	bool fadeIn {false};
	int32_t pitchPercent {100};
	/// Positional music comes from this point in the world; other music is centred on the listener
	std::optional<glm::vec3> position;
	/// The distances of a positional piece, unless the header of the bank gives others
	float minDistance {10.0f};
	float maxDistance {100.0f};
	float distanceScale {1.0f};
	/// Called when the bank has played to its end
	std::function<void()> onFinished;
};

/// A music channel that plays: the music group of its bank and the 1-based chunk that the player hears
struct MusicChannelPlace
{
	int32_t groupId {0};
	uint32_t playingChunk {1};
};

/// A music channel that plays, as the debug tools show it: the path of its bank, its volume out of 127, and the place
/// from which the player hears it when it has a place
struct MusicChannelState
{
	std::string bank;
	int32_t volume {0};
	std::optional<glm::vec3> position;
};

} // namespace openblack::audio
