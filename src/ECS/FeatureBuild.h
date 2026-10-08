/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <entt/entity/entity.hpp>

namespace openblack::ecs::feature_build
{

/// The built percentage of a Feature, the CHL property 22 BUILT_PERCENTAGE: Land 1's
/// TheMissionaries sets the ArkDryDock to 0.2 and TheMissionariesBuildingBoat adds 0.03 a hammer blow.

/// The built percentage of a multi-map fixed object, 1 for anything else. Only Features keep it here: nothing when
/// the object is some other multi-map fixed object (Abode...).
[[nodiscard]] std::optional<float> GetBuiltPercentage(entt::entity entity);

/// Sets the percentage to max(value, 0); at >= 1 the object is built (the percentage becomes 1). Then the town's
/// building list, which a Feature never has. False when the object is not a Feature.
///
/// The draw: only the ArkDryDock (feature info 69) draws as a building while not built; the rest would use a
/// building site, which Features never have. While it holds, the model is drawn partly built
/// (physics::PartialBuild) at min(percent built, percent repaired = 1 when not built), and nothing at 0.
bool SetBuiltPercentage(entt::entity entity, float value);

/// Test hook OPENBLACK_TEST_BUILT_PERCENTAGE="p": Land 1's ArkDryDock where TheMissionaries makes it, at p
void RunDebugHook();

} // namespace openblack::ecs::feature_build
