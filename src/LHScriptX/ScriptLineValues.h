/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <string>

#include "CommandSignature.h"

namespace openblack::lhscriptx
{

/// The values that the reader of the land scripts keeps from the lines that it read. Each argument has a place, and
/// each place keeps a text, a number and a float. A line fills only the places of the arguments that it gives: the
/// text of each argument, the number of a number argument and the float of a float argument. The other values stay
/// as earlier lines left them, also from the scripts of earlier lands. The reader never clears them.
struct ScriptLineValues
{
	static constexpr size_t k_Places = 12;

	/// The text of each place. Only a text argument keeps its text here. The text of a number, a float or a position is
	/// never a name, and the one command that reads a place that its line did not fill looks for a name in it.
	std::array<std::string, k_Places> text {};
	std::array<int32_t, k_Places> number {};
	std::array<float, k_Places> real {};

	/// Keeps the arguments of a line in their places
	void Keep(const ScriptCommandParameters& parameters)
	{
		const auto count = std::min(parameters.size(), k_Places);
		for (size_t place = 0; place < count; ++place)
		{
			const auto& parameter = parameters[place];
			switch (parameter.GetType())
			{
			case ParameterType::String:
				text.at(place) = parameter.GetString();
				break;
			case ParameterType::Number:
				text.at(place).clear();
				number.at(place) = parameter.GetNumber();
				break;
			case ParameterType::Float:
				text.at(place).clear();
				real.at(place) = parameter.GetFloat();
				break;
			case ParameterType::Vector:
			case ParameterType::None:
				text.at(place).clear();
				break;
			}
		}
	}
};

} // namespace openblack::lhscriptx
