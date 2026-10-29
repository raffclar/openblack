/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Audio/ScriptMusic.h"
#include "ECS/Systems/SavedStateInterface.h"

namespace openblack::ecs::systems
{

/// The music that the scripts start, as the world sees it: the piece that the scripts want, the line and word that the
/// piece has reached, and its end. A game turn moves its clock on, so that every computer and the server agree, with or
/// without a sound device. The music heard follows it (see audio::ScriptMusic).
class ScriptMusicSystemInterface: public SavedStateInterface
{
public:
	~ScriptMusicSystemInterface() override = default;

	/// A script starts a piece of music, or stops it with MusicType::None
	virtual void Start(audio::MusicType type) = 0;
	/// A script starts music: the lyrics start again with no line and no word
	virtual void ResetLyrics() = 0;
	/// The end of a dialogue starts the sung line again at no word
	virtual void ResetWord() = 0;
	/// One game turn of the music, in the real time that the turn takes at the game's speed
	virtual void ProcessTurn() = 0;
	/// A new land: no piece plays
	virtual void Reset() = 0;
	/// With no music banks loaded (the -NOLOADMUSIC switch), no piece plays and none is ever over
	virtual void SetBanksLoaded(bool loaded) = 0;
	/// Registers the times of each piece of the scripts in the resource cache and asks for them, so that the loading
	/// threads read them behind the loading screen and not in the turn that starts the piece
	virtual void RegisterTimings() = 0;
	[[nodiscard]] virtual const audio::ScriptMusic& GetMusic() const = 0;
	/// Whether the world has the game's music (see audio::WorldHasMusic). Without it, no piece plays, and the scripts
	/// that wait on music wait on the help text.
	[[nodiscard]] virtual bool HasMusic() const = 0;
};

} // namespace openblack::ecs::systems
