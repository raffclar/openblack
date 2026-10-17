/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SaveGameInfo.h"

#include <algorithm>

#include "SaveBytes.h"

using namespace openblack::gamesave;

namespace
{
void PutText(ByteWriter& writer, std::u16string_view text)
{
	std::array<char16_t, k_SaveGameInfoTextLength> characters {};
	// The text keeps room for its end mark
	const auto count = std::min(text.size(), k_SaveGameInfoTextLength - 1);
	std::copy_n(text.begin(), count, characters.begin());
	writer.Put(characters);
}

std::u16string GetText(ByteReader& reader)
{
	std::array<char16_t, k_SaveGameInfoTextLength> characters {};
	reader.Get(characters);
	const auto end = std::ranges::find(characters, u'\0');
	return {characters.begin(), end};
}
} // namespace

std::vector<uint8_t> openblack::gamesave::Write(const SaveGameInfo& info)
{
	ByteWriter writer;
	writer.Put(info.slot);
	PutText(writer, info.date.size() >= k_LongestDate ? std::u16string_view {} : std::u16string_view {info.date});
	PutText(writer, info.name);
	return writer.Take();
}

std::optional<SaveGameInfo> openblack::gamesave::ReadSaveGameInfo(std::span<const uint8_t> bytes)
{
	if (bytes.size() < k_SaveGameInfoSize)
	{
		return std::nullopt;
	}
	ByteReader reader(bytes);
	SaveGameInfo info;
	reader.Get(info.slot);
	info.date = GetText(reader);
	info.name = GetText(reader);
	return info;
}
