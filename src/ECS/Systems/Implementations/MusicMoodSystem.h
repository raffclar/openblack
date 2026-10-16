/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <entt/entity/fwd.hpp>

#include "ECS/Systems/MusicMoodSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class MusicMoodSystem final: public MusicMoodSystemInterface
{
public:
	void LoadFolder() override;
	void UpdateOnGameTurn(bool reset) override;
	[[nodiscard]] std::optional<music_mood::Emotion> GetCurrentEmotion() const override { return _controller.Current(); }
	void Clear() override { _controller.Clear(); }
	void ReadRecords(const std::optional<std::vector<music_mood::Record>>& records, bool own) override;
	[[nodiscard]] std::vector<music_mood::Record> WriteRecords() override;
	[[nodiscard]] std::vector<music_mood::Record> GetRecords() const override;

private:
	/// The local player's creature, when it has one
	[[nodiscard]] static std::optional<entt::entity> LocalCreature();
	/// What the local player's creature feels now: the player's recent feedback and its fear
	[[nodiscard]] static std::optional<music_mood::Emotion> Feelings();
	/// The creature reacts to the track that starts
	static void Tell(const music_mood::Packet& packet);

	music_mood::Controller _controller;
};

} // namespace openblack::ecs::systems
