/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ClanCreatureSystem.h"

#include <utility>

#include <spdlog/spdlog.h>

#include "ECS/Systems/LobbyClientSystemInterface.h"
#include "Online/CreatureUpload.h"

namespace openblack::ecs::systems
{

namespace
{
void Log(spdlog::level::level_enum level, std::string_view text)
{
	if (const auto logger = spdlog::get("game"))
	{
		logger->log(level, text);
	}
}
} // namespace

ClanCreatureSystem::ClanCreatureSystem(LobbyOf lobby)
    : _lobby(std::move(lobby))
{
}

void ClanCreatureSystem::UseClan(std::optional<int64_t> clan)
{
	++_generation;
	_clan = clan;
	_reply.reset();
	auto* lobby = _lobby ? _lobby() : nullptr;
	if (!clan.has_value() || lobby == nullptr)
	{
		return;
	}
	lobby->GetClanCreature(*clan, [this, generation = _generation](LobbyResult<lobby_api::ClanCreatureReply>&& result) {
		if (generation != _generation)
		{
			return;
		}
		if (!result.Ok())
		{
			// With no answer, the god brings the creature of the player's profile
			Log(spdlog::level::warn, "The lobby did not give the clan creature");
			return;
		}
		_reply = std::move(*result.value);
		Log(spdlog::level::info, _reply->kind == lobby_api::ClanCreatureKind::Stored ? "The lobby gave the stored clan creature"
		                         : _reply->kind == lobby_api::ClanCreatureKind::Create
		                             ? "The lobby gave the order for a new clan creature"
		                             : "The clan has no creature");
	});
}

std::optional<ClanStartCreature> ClanCreatureSystem::StartCreature() const
{
	if (!_clan.has_value() || !_reply.has_value())
	{
		return std::nullopt;
	}
	auto file = creature_upload::StartData(*_reply);
	if (!file.has_value())
	{
		return std::nullopt;
	}
	// The stored clan creature starts at the alignment kept with it. The order for a new creature has an alignment of 0.
	const float alignment = _reply->kind == lobby_api::ClanCreatureKind::Stored && _reply->creature.has_value()
	                            ? _reply->creature->alignment
	                            : 0.0f;
	return ClanStartCreature {.file = std::move(*file), .alignment = alignment};
}

void ClanCreatureSystem::KeepCreatureFile(std::vector<uint8_t> file)
{
	_keptFile = std::move(file);
}

ClanUpload ClanCreatureSystem::SendAtGameEnd(float godAlignment)
{
	if (!_clan.has_value())
	{
		return ClanUpload::NoClan;
	}
	if (_keptFile.empty())
	{
		return ClanUpload::NoCreatureFile;
	}
	// The second buffer is the user data that the god sent at the start: the second buffer of the stored clan creature.
	// With none, the game sends the user data of the player, which openblack does not make yet.
	const bool hasUserData = _reply.has_value() && _reply->creature.has_value() && !_reply->creature->physique.empty();
	if (!hasUserData)
	{
		Log(spdlog::level::warn, "The clan creature does not go to the lobby: the game has no user data of the player");
		return ClanUpload::NoUserData;
	}
	auto creature = creature_upload::PackClanCreature(godAlignment, _keptFile, _reply->creature->physique);
	if (!creature.has_value())
	{
		Log(spdlog::level::warn, "The clan creature does not go to the lobby: the kept file is not a creature file");
		return ClanUpload::BadCreatureFile;
	}
	auto* lobby = _lobby ? _lobby() : nullptr;
	if (lobby == nullptr)
	{
		return ClanUpload::NoLobby;
	}
	Log(spdlog::level::info, "The clan creature goes to the lobby");
	// The game does not read the answer of the lobby
	lobby->PutClanCreature(*_clan, std::move(*creature), [](LobbyResult<LobbyDone>&& result) {
		if (!result.Ok())
		{
			Log(spdlog::level::warn, "The lobby did not take the clan creature");
		}
	});
	return ClanUpload::Sent;
}

} // namespace openblack::ecs::systems
