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

#include "ECS/Systems/GameAudioSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class GameAudioSystem final: public GameAudioSystemInterface
{
public:
	GameAudioSystem();
	~GameAudioSystem() override;
	GameAudioSystem(const GameAudioSystem&) = delete;
	GameAudioSystem& operator=(const GameAudioSystem&) = delete;
	GameAudioSystem(GameAudioSystem&&) = delete;
	GameAudioSystem& operator=(GameAudioSystem&&) = delete;

	void StartLandAudio(bool loadMusic) override;
	void ProcessMusicTurn(glm::vec3 camera, bool inCitadel) override;
	void ProcessAmbienceTurn(glm::vec3 camera, bool inCitadel) override;
	void ProcessTempleAudioTurn() override;

	[[nodiscard]] const audio::AtmosAudio* GetAtmosAudio() const override { return _atmosAudio.get(); }
	[[nodiscard]] audio::GameMusic* GetGameMusic() override { return _gameMusic.get(); }
	[[nodiscard]] const audio::GameMusic* GetGameMusic() const override { return _gameMusic.get(); }

private:
	std::unique_ptr<audio::AtmosAudio> _atmosAudio;
	std::unique_ptr<audio::GameMusic> _gameMusic;
};

} // namespace openblack::ecs::systems
