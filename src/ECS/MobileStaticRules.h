/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Enums.h"

/// The rules of what a mobile static is, by its kind and the class its row in the info table gives it
namespace openblack::ecs::mobile_static_rules
{

/// Whether a mobile static is a rock, a thing of its own to the scripts. Every mobile static whose row is of the rock
/// class is one, however it is made: by the land's script, by a script asking for a mobile static or for a rock, or by
/// a rock splitting. A bonfire is built as a rock but stays a mobile static to the scripts.
[[nodiscard]] constexpr bool IsScriptRock(MobileStaticInfo type, MobileStaticInfo rowClass)
{
	return type != MobileStaticInfo::Bonfire && rowClass == MobileStaticInfo::Rock;
}

} // namespace openblack::ecs::mobile_static_rules
