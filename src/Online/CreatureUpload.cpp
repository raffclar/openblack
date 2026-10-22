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
#include "Creature/CreatureWebPage.h"
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

std::optional<lobby_api::ClanCreature> PackClanCreature(float alignment, std::span<const uint8_t> mind,
                                                        std::span<const uint8_t> physique)
{
	if (!IsMind(mind) || !IsPhysique(physique))
	{
		return std::nullopt;
	}
	return lobby_api::ClanCreature {
	    .alignment = alignment,
	    .mind = {mind.begin(), mind.end()},
	    .physique = {physique.begin(), physique.end()},
	};
}

bool IsValidClanCreature(const lobby_api::ClanCreature& creature)
{
	return IsMind(creature.mind) && IsPhysique(creature.physique);
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
