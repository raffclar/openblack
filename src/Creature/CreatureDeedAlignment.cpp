/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureDeedAlignment.h"

#include "Magic/AreaEffect.h"

using namespace openblack;

float creature_deed_alignment::TurnChange(const Deed& deed, float speciesScale, float alignment)
{
	const float change = deed.eatenVillagerWeight.value_or(deed.desireChange) * speciesScale;
	return magic::DampAlignmentChange(change, alignment);
}
