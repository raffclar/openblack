/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpecialVillagers.h"

#include <algorithm>

namespace openblack::ecs::special_villager
{

std::optional<uint8_t> PickName(std::span<const Row> rows, const std::array<bool, k_NameCount>& used, SexType sex,
                                uint32_t start)
{
	const auto count = std::min(rows.size(), used.size());
	if (count == 0)
	{
		return std::nullopt;
	}
	for (size_t step = 0; step < count; ++step)
	{
		const auto index = (start + step) % count;
		const auto& row = rows[index];
		if ((sex == SexType::None || sex == row.sex) && !used.at(index) && row.hasName)
		{
			return static_cast<uint8_t>(index);
		}
	}
	return std::nullopt;
}

} // namespace openblack::ecs::special_villager
