/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureUpload.h"

#include <string>

#include <LhpFile.h>
#include <MindFile.h>
#include <PhysiqueFile.h>

#include "Common/PlayerProfiles.h"
#include "Creature/ClanCreatureOrder.h"
#include "Creature/CreatureWebPage.h"
#include "ECS/Components/Alignment.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureCarryOverSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/ProfileFolderSystemInterface.h"
#include "Gui/TextDatabase.h"
#include "Locator.h"

namespace openblack::creature_upload
{
namespace
{

/// The bytes of a file, through the creature mind component, none when the file is not there
std::optional<std::vector<uint8_t>> ReadIfThere(const std::filesystem::path& path)
{
	if (path.empty())
	{
		return std::nullopt;
	}
	return creaturemind::ReadFileBytes(path);
}

bool IsMind(std::span<const uint8_t> bytes)
{
	creaturemind::MindFileData data;
	return creaturemind::Read(bytes, data) == creaturemind::MindResult::Success;
}

bool IsPhysique(std::span<const uint8_t> bytes)
{
	return creaturemind::ReadPhysique(bytes).has_value();
}

} // namespace

std::variant<lobby_api::CreatureUpload, Problem> Gather(const Paths& paths)
{
	auto package = ReadIfThere(paths.package);
	if (!package.has_value())
	{
		return Problem::NoPackage;
	}
	if (!lhp::Read(*package).has_value())
	{
		return Problem::BadPackage;
	}
	auto mind = ReadIfThere(paths.mind);
	if (!mind.has_value())
	{
		return Problem::NoMind;
	}
	if (!IsMind(*mind))
	{
		return Problem::BadMind;
	}
	auto physique = ReadIfThere(paths.physique);
	if (!physique.has_value())
	{
		return Problem::NoPhysique;
	}
	if (!IsPhysique(*physique))
	{
		return Problem::BadPhysique;
	}
	return lobby_api::CreatureUpload {
	    .package = std::move(*package),
	    .mind = std::move(*mind),
	    .mindFileName = paths.mind.filename().string(),
	    .physique = std::move(*physique),
	    .physiqueFileName = paths.physique.filename().string(),
	};
}

std::optional<lobby_api::ClanCreature> PackClanCreature(float godAlignment, std::span<const uint8_t> creatureFile,
                                                        std::span<const uint8_t> userData)
{
	if (!IsMind(creatureFile))
	{
		return std::nullopt;
	}
	return lobby_api::ClanCreature {
	    .alignment = godAlignment,
	    .mind = {creatureFile.begin(), creatureFile.end()},
	    .physique = {userData.begin(), userData.end()},
	};
}

bool IsValidClanCreature(const lobby_api::ClanCreature& creature)
{
	// The game reads only the creature file. The second buffer is the user data, which the game does not read as a file.
	return IsMind(creature.mind);
}

std::optional<lobby_api::ClanCreature> ClanCreatureAtGameEnd(entt::entity god, entt::entity creature,
                                                             std::span<const uint8_t> userData)
{
	if (!Locator::entitiesRegistry::has_value() || !Locator::creatureCarryOverSystem::has_value())
	{
		return std::nullopt;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* alignment = registry.Valid(god) ? registry.TryGet<const ecs::components::Alignment>(god) : nullptr;
	const auto file = Locator::creatureCarryOverSystem::value().CreatureFile(creature);
	if (alignment == nullptr || !file.has_value())
	{
		return std::nullopt;
	}
	return PackClanCreature(alignment->value, *file, userData);
}

std::optional<std::vector<uint8_t>> StartData(const lobby_api::ClanCreatureReply& reply)
{
	switch (reply.kind)
	{
	case lobby_api::ClanCreatureKind::Stored:
		// Every computer reads the bytes as a creature file. Bytes that are not a creature file give no creature.
		if (reply.creature.has_value())
		{
			return reply.creature->mind;
		}
		return std::nullopt;
	case lobby_api::ClanCreatureKind::Create:
		if (reply.make.has_value())
		{
			return clan_creature_order::Write({.row = reply.make->type, .name = gui::ToUtf16(reply.make->name)});
		}
		return std::nullopt;
	case lobby_api::ClanCreatureKind::None:
		break;
	}
	return std::nullopt;
}

std::optional<Paths> PlayersCreaturePaths()
{
	if (!Locator::playerProfileSystem::has_value() || !Locator::profileFolderSystem::has_value() ||
	    !Locator::creatureCarryOverSystem::has_value())
	{
		return std::nullopt;
	}
	const auto profile = Locator::playerProfileSystem::value().GetCurrentProfile();
	if (!profile.has_value())
	{
		return std::nullopt;
	}
	const auto folder = Locator::profileFolderSystem::value().GetPlayerFolder(gui::ToUtf16(profile->name));
	const auto& carryOver = Locator::creatureCarryOverSystem::value();
	const auto mind = carryOver.ProfileMind();
	const auto physique = carryOver.ProfilePhysique();
	if (!folder.has_value() || !mind.has_value() || !physique.has_value())
	{
		return std::nullopt;
	}
	return Paths {
	    .package = *folder / std::string(creature_web_page::k_UploadFile),
	    .mind = *mind,
	    .physique = *physique,
	};
}

} // namespace openblack::creature_upload
