/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureNames.h"

using namespace openblack;

std::u16string creature_names::NewName(Owner owner, size_t creatures, std::optional<std::u16string_view> profileCreatureName,
                                       const std::function<std::u16string(uint32_t)>& helpWords)
{
	if (owner == Owner::Player)
	{
		return std::u16string(profileCreatureName.value_or(std::u16string_view {}));
	}
	return helpWords(StrayNameText(creatures));
}
