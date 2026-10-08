/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// The heal miracle's rules (Rules/Heal.cpp): what the tests and other files need of them.

namespace openblack::psys::heal
{
/// UR_HealSpellChakra's fade: the chakra's strength t at that age of its burst, age / AtomAgeMaxAlpha up to 1, then
/// 1 - (age - AtomAgeMaxAlpha) / (AtomAgeZeroAlpha - AtomAgeMaxAlpha), clamped to 0..1. The burst's sprites get alpha
/// t x MaxAlpha and the target glows with t x SpecularColor (both truncated).
[[nodiscard]] float ChakraFade(float age, float ageMaxAlpha, float ageZeroAlpha);
} // namespace openblack::psys::heal
