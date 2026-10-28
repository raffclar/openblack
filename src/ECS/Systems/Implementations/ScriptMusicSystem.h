/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <memory>

#include "ECS/Systems/ScriptMusicSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class ScriptMusicSystem final: public ScriptMusicSystemInterface
{
public:
	void Start(audio::MusicType type) override { _music.Start(type); }
	void ResetLyrics() override { _music.ResetLyrics(); }
	void ResetWord() override { _music.ResetWord(); }
	void ProcessTurn() override;
	void Reset() override;
	void SetBanksLoaded(bool loaded) override { _banksLoaded = loaded; }
	[[nodiscard]] const audio::ScriptMusic& GetMusic() const override { return _music; }
	[[nodiscard]] bool HasMusic() const override;

	/// A save keeps the piece that the scripts want, whether it played, and the line and word that it reached
	void SaveState(reflection::BinaryWriter& out) const override;
	/// A piece that played at the save gives no sound and never ends, because no channel plays it after the load
	bool LoadState(reflection::BinaryReader& in) override;

private:
	/// The times of a piece from the resource cache, none when its bank does not play
	[[nodiscard]] std::shared_ptr<const audio::MusicTiming> Timing(audio::MusicType type) const;

	audio::ScriptMusic _music;
	bool _banksLoaded {true};
};

} // namespace openblack::ecs::systems
