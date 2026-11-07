/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/vec3.hpp>

namespace openblack::audio
{
class AtmosAudio;
class GameMusic;
} // namespace openblack::audio

namespace openblack::ecs::systems
{

/// The music and the ambience of the land that the player of this computer hears. The system makes them at the first
/// land and owns them. The turns of the view and of the temple tell them where the camera is. The dedicated server
/// does not make this system.
class GameAudioSystemInterface
{
public:
	virtual ~GameAudioSystemInterface() = default;

	/// A land starts: the ambience reads its banks again, and the music starts again from the start of a land. The
	/// first land makes the music. Without loadMusic, the game loads no music banks and no music of the game plays.
	virtual void StartLandAudio(bool loadMusic) = 0;
	/// The music of the turn picks the piece for a camera at a place, inside the citadel or not
	virtual void ProcessMusicTurn(glm::vec3 camera, bool inCitadel) = 0;
	/// The ambience of the turn follows the weather and the alignment at the camera
	virtual void ProcessAmbienceTurn(glm::vec3 camera, bool inCitadel) = 0;
	/// A turn in the temple: the citadel's music plays, and the ambience of the land fades out
	virtual void ProcessTempleAudioTurn() = 0;

	/// The ambience, none before the first land
	[[nodiscard]] virtual const audio::AtmosAudio* GetAtmosAudio() const = 0;
	/// The music, none before the first land
	[[nodiscard]] virtual audio::GameMusic* GetGameMusic() = 0;
	[[nodiscard]] virtual const audio::GameMusic* GetGameMusic() const = 0;
};

} // namespace openblack::ecs::systems
