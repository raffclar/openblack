/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include "Enums.h"

/// The help text a script asks a thing for, as the reward chests' scripts say it when a chest is clicked
namespace openblack::help::script_help_text
{

/// The help texts a thing has, each none when the thing has no such text
struct Texts
{
	/// A reward chest's own help text, from the row of its kind of reward
	std::optional<HelpText> reward;
	/// A miracle dispenser's: the first help text of the miracle it casts
	std::optional<HelpText> dispenser;
	/// Any other object's: the first help text of its kind
	std::optional<HelpText> kind;
};

/// The text that asking for a thing's help gives: a reward's own text first, then a dispenser's miracle's, then its
/// kind's, and 0 for a thing that has none
[[nodiscard]] constexpr HelpText Help(const Texts& texts)
{
	if (texts.reward.has_value())
	{
		return *texts.reward;
	}
	if (texts.dispenser.has_value())
	{
		return *texts.dispenser;
	}
	return texts.kind.value_or(HelpText {});
}

/// The text that asking for a thing's first (or last) help gives: as above, but a reward gives its kind's first text
/// rather than its own
[[nodiscard]] constexpr HelpText FirstHelp(const Texts& texts)
{
	return Help({.reward = std::nullopt, .dispenser = texts.dispenser, .kind = texts.kind});
}

} // namespace openblack::help::script_help_text
