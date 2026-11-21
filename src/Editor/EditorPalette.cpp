/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "EditorPalette.h"

#include <cctype>
#include <cmath>

#include <algorithm>

#include "Animals/BirdRules.h"
#include "Animals/GrazerRules.h"
#include "Animals/HunterRules.h"
#include "Common/IntegerSteps.h"

namespace openblack::editor
{

std::string TitleCase(std::string_view text)
{
	std::string result;
	result.reserve(text.size());
	bool startOfWord = true;
	for (const auto c : text)
	{
		if (c == '_' || c == ' ')
		{
			if (!result.empty() && result.back() != ' ')
			{
				result.push_back(' ');
			}
			startOfWord = true;
			continue;
		}
		const auto byte = static_cast<unsigned char>(c);
		result.push_back(static_cast<char>(startOfWord ? std::toupper(byte) : std::tolower(byte)));
		startOfWord = false;
	}
	while (!result.empty() && result.back() == ' ')
	{
		result.pop_back();
	}
	return result;
}

size_t CommonWordPrefix(std::span<const std::string_view> names)
{
	if (names.size() < 2)
	{
		return 0;
	}
	size_t common = 0;
	const auto first = names.front();
	for (size_t end = first.find('_'); end != std::string_view::npos; end = first.find('_', end + 1))
	{
		const auto prefix = first.substr(0, end + 1);
		// A name is never left empty
		const bool shared = std::ranges::all_of(
		    names, [prefix](std::string_view name) { return name.starts_with(prefix) && name.size() > prefix.size(); });
		if (!shared)
		{
			break;
		}
		common = prefix.size();
	}
	return common;
}

std::vector<std::string> ReadableNames(std::span<const std::string_view> names)
{
	const auto prefix = CommonWordPrefix(names);
	std::vector<std::string> result;
	result.reserve(names.size());
	for (const auto name : names)
	{
		result.push_back(TitleCase(name.substr(std::min(prefix, name.size()))));
	}
	return result;
}

std::string NameOfModel(std::string_view meshName)
{
	constexpr std::string_view k_Animal = "Animal";
	if (meshName.starts_with(k_Animal) && meshName.size() > k_Animal.size())
	{
		meshName.remove_prefix(k_Animal.size());
	}
	while (!meshName.empty() && std::isdigit(static_cast<unsigned char>(meshName.back())) != 0)
	{
		meshName.remove_suffix(1);
	}
	std::string result;
	for (const auto c : meshName)
	{
		if (std::isupper(static_cast<unsigned char>(c)) != 0 && !result.empty())
		{
			result.push_back(' ');
		}
		result.push_back(c);
	}
	return result;
}

bool IsPlaceableAnimal(AnimalInfo kind)
{
	return animals::grazers::IsGrazer(kind) || animals::hunters::IsWildHunter(kind) || animals::birds::IsLandBird(kind);
}

std::vector<int32_t> PlaceableAnimalRows(std::span<const AnimalInfo> rowKinds)
{
	std::vector<int32_t> rows;
	for (size_t row = 0; row < rowKinds.size(); ++row)
	{
		if (IsPlaceableAnimal(rowKinds[row]))
		{
			rows.push_back(static_cast<int32_t>(row));
		}
	}
	return rows;
}

std::vector<glm::vec2> GridLayout(size_t count, float spacing)
{
	std::vector<glm::vec2> points;
	if (count == 0)
	{
		return points;
	}
	const auto columns = static_cast<size_t>(std::ceil(std::sqrt(static_cast<float>(count))));
	const auto rows = (count + columns - 1) / columns;
	const auto halfWidth = static_cast<float>(columns - 1) * spacing * 0.5f;
	const auto halfDepth = static_cast<float>(rows - 1) * spacing * 0.5f;
	points.reserve(count);
	for (size_t i = 0; i < count; ++i)
	{
		const auto column = static_cast<float>(i % columns);
		const auto row = static_cast<float>(WholeQuotient(i, columns));
		// North is +z on the land, so the first row is the far one
		points.emplace_back((column * spacing) - halfWidth, halfDepth - (row * spacing));
	}
	return points;
}

} // namespace openblack::editor
