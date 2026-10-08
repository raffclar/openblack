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

#include <string_view>

#include "Enums.h"

// PARTICLE_TYPE -> spell file, the table the magic info rows (particleType, particleTypeInHand), the seeds'
// holderParticle and the spot visuals go through when the particle files are loaded.

namespace openblack::psys
{
constexpr size_t k_ParticleTypeCount = 150;

/// The spell file of a particle type, as File::Load takes it ("SF_Forest"); empty when the original has none (NONE,
/// TORNADO, FOOD_IN_HAND...: 29 of the 150 have no file)
[[nodiscard]] std::string_view ParticleTypeFile(ParticleType type);
} // namespace openblack::psys
