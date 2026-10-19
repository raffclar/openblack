/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SaveSlots.h"

#include <cctype>

#include <algorithm>

using namespace openblack::gamesave;

std::filesystem::path openblack::gamesave::SlotFolder(uint32_t slot, uint32_t land)
{
	auto folder = std::filesystem::path(k_SavedGamesFolder) / std::to_string(slot);
	return HasLandFolder(slot) ? folder / std::to_string(land) : folder;
}

std::filesystem::path openblack::gamesave::LoadSlotFolder(uint32_t slot, uint32_t land)
{
	if (slot == k_PreviousLandSlot)
	{
		return SlotFolder(slot, land - 1);
	}
	if (slot == k_NextLandSlot)
	{
		return SlotFolder(slot, land + 1);
	}
	return SlotFolder(slot, land);
}

std::optional<uint32_t> openblack::gamesave::SaveBeforeRoomLoad(uint32_t slot)
{
	if (slot == k_PreviousLandSlot)
	{
		return k_NextLandSlot;
	}
	if (slot == k_NextLandSlot)
	{
		return k_PreviousLandSlot;
	}
	return std::nullopt;
}

bool openblack::gamesave::RoomSaveStartsLoadHelp(RoomSave why, bool slotHasSave)
{
	return why == RoomSave::BeforeLoad || !slotHasSave;
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

int32_t openblack::gamesave::LeadingNumber(std::string_view name)
{
	size_t place = 0;
	const auto isSpace = [](char c) { return c == ' ' || (c >= '\t' && c <= '\r'); };
	while (place < name.size() && isSpace(name[place]))
	{
		++place;
	}
	bool negative = false;
	if (place < name.size() && (name[place] == '-' || name[place] == '+'))
	{
		negative = name[place] == '-';
		++place;
	}
	uint32_t value = 0;
	for (; place < name.size() && name[place] >= '0' && name[place] <= '9'; ++place)
	{
		value = (value * 10u) + static_cast<uint32_t>(name[place] - '0');
	}
	return static_cast<int32_t>(negative ? 0u - value : value);
}

std::optional<RoomFolder> openblack::gamesave::RoomFolderOf(std::string_view name, uint32_t land)
{
	if (name == "." || name == "..")
	{
		return std::nullopt;
	}
	RoomFolder found {.folder = std::filesystem::path(name), .fileSlot = LeadingNumber(name)};
	if (name == "15" || name == "16" || name == "17")
	{
		found.folder /= std::to_string(land);
	}
	else if (name == "14" && land > 1)
	{
		found.folder /= std::to_string(land - 1);
	}
	else if (name == "18" && land >= 1 && land < 5)
	{
		found.folder /= std::to_string(land + 1);
	}
	return found;
}

bool openblack::gamesave::ListedBefore(std::string_view a, std::string_view b)
{
	const auto upper = [](char c) { return static_cast<char>(std::toupper(static_cast<unsigned char>(c))); };
	return std::ranges::lexicographical_compare(a, b, {}, upper, upper);
}
