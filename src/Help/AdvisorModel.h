/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <memory>

#include <HelpDudeFile.h>
#include <entt/core/hashed_string.hpp>

#include "Help/SpiritPose.h"
#include "Help/Spirits.h"

namespace openblack::graphics
{
class L3DMesh;
}

namespace openblack::help::spirits
{

/// One advisor as its .hd file makes it: the file, what the advisor's logic takes from it, its skeleton and its mesh
struct AdvisorModel
{
	helpdude::HelpDudeFile file;
	DudeData data;
	SpiritRig rig;
	std::shared_ptr<const graphics::L3DMesh> mesh;
};

/// The good advisor's and the evil advisor's models in the resource cache
constexpr auto k_GoodModelId = entt::hashed_string("helpdude/good");
constexpr auto k_EvilModelId = entt::hashed_string("helpdude/evil");
[[nodiscard]] constexpr entt::id_type ModelId(int dude)
{
	return dude == k_GoodDude ? k_GoodModelId.value() : k_EvilModelId.value();
}

} // namespace openblack::help::spirits
