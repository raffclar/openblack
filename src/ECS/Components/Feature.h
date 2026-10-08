/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/fwd.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{

struct Feature
{
	FeatureInfo type;
	/// The percent built: 1 when made by the map or a script, set by the CHL SET_PROPERTY 22 (ecs/FeatureBuild.h)
	float percentBuilt {1.0f};
	/// the model while the partly built one of ecs/FeatureBuild.h is drawn (0: none)
	entt::id_type intactMesh {0};
	entt::id_type builtMesh {0};
};

} // namespace openblack::ecs::components
