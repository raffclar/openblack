/*****************************************************************************
 * Copyright (c) 2018-2020 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include "AudioLoader.h"
#include "FfmpegLoader.h"
#include "OpenAlPlayer.h"
#include "SoundPack.h"
#include <SadFile.h>
#include <glm/glm.hpp>
#include <map>
#include <string>
#include <type_traits>
#include <vector>

namespace openblack
{
class Game;
}

namespace openblack::audio
{

class SoundHandler
{
public:
	SoundHandler(std::unique_ptr<AudioLoader>&& loader, std::unique_ptr<AudioPlayer>&& player);
	void RegisterSoundPack(std::unique_ptr<SoundPack>& soundPack);
	void PlaySound(SoundId id);
	AudioEmitterId CreateEmitter(const std::vector<SoundId> ids, glm::vec3& position, glm::vec3& velocity, glm::vec2& radius, float volume, bool world);
	AudioEmitterId CreateEmitter(SoundId id, glm::vec3& position, glm::vec3& velocity, glm::vec2& radius, float volume, bool world);
	void DestroyEmitter(AudioEmitterId id);
	[[nodiscard]] const std::vector<std::shared_ptr<SoundPack>>& GetSoundPacks() const { return _soundPacks; }
	[[nodiscard]] std::unique_ptr<Sound>& GetSound(SoundId id);
	void addSoundPriority(std::string name);
	[[nodiscard]] int GetSoundPriority(std::string name);
	[[nodiscard]] float GetGlobalVolume() const { return _audioPlayer->GetVolume(); };
	[[nodiscard]] std::string GetAudioLoaderName() const { return _audioLoader->GetName(); }
	[[nodiscard]] std::string GetAudioPlayerName() const { return _audioPlayer->GetName(); }
	[[nodiscard]] std::vector<AudioEmitter> GetActiveEmitters() const;
	void ReplaceAudioLoader(std::unique_ptr<AudioLoader> audioLoader) { _audioLoader.reset(audioLoader.release()); }
	void ReplaceAudioPlayer(std::unique_ptr<AudioPlayer> audioPlayer) { _audioPlayer.reset(audioPlayer.release()); }
	const std::unique_ptr<AudioPlayer>& GetPlayer() const { return _audioPlayer; }
	void SetGlobalVolume(float volume);
	void Tick(Game& game);
private:
	std::unique_ptr<AudioPlayer> _audioPlayer;
	std::unique_ptr<AudioLoader> _audioLoader;
	std::map<std::string, size_t> _soundPriorities;
	/// Lookup for sound ID ownership
	std::map<SoundId, std::shared_ptr<SoundPack>> _soundIdLookup;
	std::vector<std::shared_ptr<SoundPack>> _soundPacks;
	std::map<AudioEmitterId, AudioEmitter> _emitters;
};

std::unique_ptr<SoundHandler> createSoundHandler();
}