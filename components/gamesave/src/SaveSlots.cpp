/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SaveSlots.h"

using namespace openblack::gamesave;

std::filesystem::path openblack::gamesave::SlotFolder(uint32_t slot, uint32_t land)
{
	auto folder = std::filesystem::path(k_SavedGamesFolder) / std::to_string(slot);
	return HasLandFolder(slot) ? folder / std::to_string(land) : folder;
}

std::filesystem::path openblack::gamesave::MindFolder(uint32_t slot, uint32_t land)
{
	auto folder = std::filesystem::path(k_SavedGamesFolder) / std::to_string(slot);
	return HasLandFolderForMinds(slot) ? folder / std::to_string(land) : folder;
}

std::string openblack::gamesave::SavedCreatureFileName(uint32_t index)
{
	return "SavedCreature" + std::to_string(index);
}

std::string openblack::gamesave::WorldFileName(uint32_t slot)
{
	return std::to_string(slot) + ".obsav";
}

std::string openblack::gamesave::VanillaWorldFileName(uint32_t slot)
{
	return std::to_string(slot) + ".sav";
}

std::string openblack::gamesave::VanillaScriptFileName(uint32_t slot)
{
	return "Script_" + std::to_string(slot) + ".sav";
}
