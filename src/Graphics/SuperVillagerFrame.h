/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <unordered_set>
#include <vector>

#include <entt/core/fwd.hpp>
#include <entt/entity/fwd.hpp>

// The SuperVillagers' part of the frame (ECS/SuperVillager.h), decided before the draw as Graphics/OverlayFrame.h is:
// Game.cpp fills it once a frame before DrawScene
// (ecs::super_villager::FillFrame, next to FillOverlayFrame) and the Renderer reads it through
// DrawSceneDesc::superVillagers; nothing in the draw reads the SuperVillager list or asks the registry for it.
namespace openblack::graphics
{

/// What the SuperVillagers' draw and the land's swim cut need of the SuperVillager list this frame
struct SuperVillagerFrame
{
	/// The meshes drawn with the light at the default sun: the HD bodies of the list and their eye meshes
	std::unordered_set<entt::id_type> litByDefaultSun;
	/// The ones animated "M_P_Swim2", list order (the newest first): cut under the water
	std::vector<entt::entity> swimmers;
	/// The things whose temporary shadow turned the human shadow off: no ground blobs, their projected shadow instead
	/// (components::DynamicShadow)
	std::vector<entt::entity> notHumanShadowed;
};

} // namespace openblack::graphics
