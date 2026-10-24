/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DeveloperTools.h"

#include <algorithm>
#include <iterator>

namespace openblack::developer_tools
{

namespace
{

/// The developer option of an argument, and whether the argument also holds its value, or none
struct Match
{
	const DeveloperOption* option;
	bool valueInside;
};

std::optional<Match> MatchOf(std::string_view argument, std::span<const DeveloperOption> options) noexcept
{
	if (argument.starts_with("--") && argument.size() > 2)
	{
		const auto text = argument.substr(2);
		const auto equals = text.find('=');
		const auto name = text.substr(0, equals);
		const auto found = std::ranges::find_if(options, [name](const DeveloperOption& option) { return option.name == name; });
		if (found == options.end())
		{
			return std::nullopt;
		}
		return Match {.option = &*found, .valueInside = equals != std::string_view::npos};
	}
	if (argument.size() >= 2 && argument[0] == '-' && argument[1] != '-')
	{
		const auto letter = argument[1];
		const auto found =
		    std::ranges::find_if(options, [letter](const DeveloperOption& option) { return option.shortName == letter; });
		if (found == options.end())
		{
			return std::nullopt;
		}
		// A short option that takes no value must be alone, else the argument is a group of other short options
		if (!found->takesValue && argument.size() > 2)
		{
			return std::nullopt;
		}
		return Match {.option = &*found, .valueInside = argument.size() > 2};
	}
	return std::nullopt;
}

} // namespace

bool IsDeveloperOption(std::string_view name, std::span<const DeveloperOption> options) noexcept
{
	return std::ranges::any_of(options, [name](const DeveloperOption& option) { return option.name == name; });
}

bool JoinsGameServer(std::span<const std::string_view> arguments) noexcept
{
	return std::ranges::any_of(arguments.subspan(arguments.empty() ? 0 : 1), [](std::string_view argument) {
		return argument == "--join" || argument.starts_with("--join=");
	});
}

std::vector<DeveloperOption> OptionsToRemove(std::span<const DeveloperOption> options, bool joins)
{
	std::vector<DeveloperOption> removed;
	std::ranges::copy_if(options, std::back_inserter(removed), [joins](const DeveloperOption& option) {
		return !joins || std::ranges::find(k_JoinWorldOptions, option.name) == k_JoinWorldOptions.end();
	});
	return removed;
}

FilteredArguments FilterDeveloperOptions(std::span<const std::string_view> arguments, std::span<const DeveloperOption> options)
{
	FilteredArguments filtered;
	bool optionsEnded = false;
	for (size_t index = 0; index < arguments.size(); ++index)
	{
		const auto argument = arguments[index];
		if (index == 0 || optionsEnded)
		{
			filtered.kept.push_back(index);
			continue;
		}
		if (argument == "--")
		{
			optionsEnded = true;
			filtered.kept.push_back(index);
			continue;
		}
		const auto match = MatchOf(argument, options);
		if (!match.has_value())
		{
			filtered.kept.push_back(index);
			continue;
		}
		std::string ignored(argument);
		if (match->option->takesValue && !match->valueInside && index + 1 < arguments.size())
		{
			++index;
			ignored += ' ';
			ignored += arguments[index];
		}
		filtered.ignored.push_back(std::move(ignored));
	}
	return filtered;
}

} // namespace openblack::developer_tools
