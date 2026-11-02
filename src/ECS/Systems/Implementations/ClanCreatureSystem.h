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
#include <span>
#include <vector>

#include <LobbyApi.h>

#include "ECS/Systems/ClanCreatureSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class LobbyClientSystemInterface;

class ClanCreatureSystem final: public ClanCreatureSystemInterface
{
public:
	/// Gives the client of the lobby, or null when the game has none
	using LobbyOf = std::function<LobbyClientSystemInterface*()>;

	explicit ClanCreatureSystem(LobbyOf lobby);

	void UseClan(std::optional<int64_t> clan) override;
	[[nodiscard]] std::optional<int64_t> UsedClan() const override { return _clan; }
	[[nodiscard]] std::optional<ClanStartCreature> StartCreature() const override;
	void KeepCreatureFile(std::vector<uint8_t> file) override;
	[[nodiscard]] std::span<const uint8_t> KeptCreatureFile() const override { return _keptFile; }
	ClanUpload SendAtGameEnd(float godAlignment) override;

private:
	LobbyOf _lobby;
	std::optional<int64_t> _clan;
	/// The answer of the lobby for the clan that the player uses
	std::optional<lobby_api::ClanCreatureReply> _reply;
	/// Each new choice of a clan makes the answers of the earlier choices old
	uint32_t _generation {0};
	/// The creature file of the creature of this computer's god, which stays from game to game in one run
	std::vector<uint8_t> _keptFile;
};

} // namespace openblack::ecs::systems
