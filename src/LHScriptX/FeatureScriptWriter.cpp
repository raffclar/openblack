/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FeatureScriptWriter.h"

#include <fmt/format.h>

#include "Common/X87.h"

namespace openblack::lhscriptx::writer
{

namespace
{

/// A map unit is a 65536th of a cell. The game multiplies by 10 first, then by this power of two.
constexpr float k_CellsPerUnit = 1.0f / 65536.0f;

/// The metres of a place along one axis, as the game works them out: the whole number times 10, rounded once to a
/// float, then times a 65536th, which is exact
[[nodiscard]] float MetresOf(int32_t units)
{
	return x87::Mul(static_cast<double>(units), 10.0) * k_CellsPerUnit;
}

} // namespace

std::string PlaceText(glm::ivec2 place)
{
	return fmt::format("\"{:.2f},{:.2f}\"", static_cast<double>(MetresOf(place.x)), static_cast<double>(MetresOf(place.y)));
}

std::string CommandLine(std::string_view name, std::string_view kinds, std::span<const Argument> arguments)
{
	if (kinds.size() != arguments.size())
	{
		return {};
	}
	std::string line {name};
	line += '(';
	const auto opened = line.size();
	for (size_t i = 0; i < kinds.size(); ++i)
	{
		const auto& argument = arguments[i];
		switch (kinds[i])
		{
		case 'A':
			if (!std::holds_alternative<Place>(argument))
			{
				return {};
			}
			line += fmt::format("{}, ", std::get<Place>(argument).text);
			break;
		case 'F':
			if (const auto* single = std::get_if<float>(&argument); single != nullptr)
			{
				line += fmt::format("{:f}, ", static_cast<double>(*single));
				break;
			}
			if (!std::holds_alternative<double>(argument))
			{
				return {};
			}
			line += fmt::format("{:f}, ", std::get<double>(argument));
			break;
		case 'L':
			if (!std::holds_alternative<Name>(argument))
			{
				return {};
			}
			line += fmt::format("\"{}\", ", std::get<Name>(argument).text);
			break;
		case 'N':
			if (!std::holds_alternative<int32_t>(argument))
			{
				return {};
			}
			line += fmt::format("{}, ", std::get<int32_t>(argument));
			break;
		default:
			break;
		}
	}
	// The separator after the last argument goes
	if (line.size() != opened)
	{
		line.resize(line.size() - 2);
	}
	line += ")\n";
	return line;
}

std::string VersionLine(float version)
{
	return fmt::format("VERSION({:f})\n", static_cast<double>(version));
}

} // namespace openblack::lhscriptx::writer
