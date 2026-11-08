/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "StartupBoxInput.h"

#include <algorithm>

namespace openblack::inspector::startup_box
{

namespace
{

/// The most bytes of the text of one input.text request, as the game's frames take it
constexpr size_t k_MostTextBytes = 256;

[[nodiscard]] constexpr bool IsContinuationByte(char byte) noexcept
{
	return (static_cast<unsigned char>(byte) & 0xC0U) == 0x80U;
}

} // namespace

std::vector<std::string> TextPieces(std::string_view text, size_t mostBytes)
{
	std::vector<std::string> pieces;
	while (!text.empty())
	{
		auto size = std::min(text.size(), std::max<size_t>(mostBytes, 1));
		while (size < text.size() && size > 0 && IsContinuationByte(text[size]))
		{
			--size;
		}
		// A character longer than a piece goes whole
		if (size == 0)
		{
			size = 1;
			while (size < text.size() && IsContinuationByte(text[size]))
			{
				++size;
			}
		}
		pieces.emplace_back(text.substr(0, size));
		text.remove_prefix(size);
	}
	return pieces;
}

std::variant<std::vector<Input>, std::string> FromRequest(std::string_view query, const Json& params)
{
	if (query == "input.text")
	{
		const auto text = StringMember(params, "text");
		if (!text.has_value() || text->empty() || text->size() > k_MostTextBytes)
		{
			return std::string("text is 1 to 256 bytes of UTF-8");
		}
		std::vector<Input> inputs;
		for (auto& piece : TextPieces(*text, k_MostTextEventBytes))
		{
			inputs.emplace_back(Text {.text = std::move(piece)});
		}
		return inputs;
	}
	if (query == "input.key")
	{
		if (StringMember(params, "action").has_value())
		{
			return std::string("the box before the first land takes keys by their names, not actions");
		}
		const auto key = StringMember(params, "key");
		if (!key.has_value() || key->empty())
		{
			return std::string("input.key needs a key");
		}
		const auto how = StringMember(params, "how").value_or("tap");
		if (how == "press")
		{
			return std::vector<Input> {Key {.name = *key, .down = true}};
		}
		if (how == "release")
		{
			return std::vector<Input> {Key {.name = *key, .down = false}};
		}
		if (how == "tap")
		{
			return std::vector<Input> {Key {.name = *key, .down = true}, Key {.name = *key, .down = false}};
		}
		return std::string("how is press, release or tap");
	}
	return "the box before the first land takes only input.text and input.key, not " + std::string(query);
}

} // namespace openblack::inspector::startup_box
