/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <type_traits>

namespace openblack::ecs::components
{
struct Abode;
struct Alpha;
struct AnimatedStatic;
struct BigForest;
struct Creature;
struct DeadTree;
struct DrawMesh;
struct Feature;
struct Field;
struct Fixed;
struct Hand;
struct Mesh;
struct MobileObject;
struct MobileStatic;
struct MorphWithTerrain;
struct NeedsSorting;
struct NotDrawn;
struct Pot;
struct TempleInteriorPart;
struct Transform;
struct Tree;
struct Unavailable;
struct Villager;
} // namespace openblack::ecs::components

namespace openblack::ecs
{

/// The components the draw lists are made from (systems::RenderingSystem): which entities are drawn, in which range,
/// with which mesh, and which of them cast a static shadow. An entity gaining or losing one of them has the draw lists
/// made again; any other change only has the instances written again into the ranges they already have.
template <typename Component>
constexpr bool k_ChangesDrawLayout = [] {
	using namespace components;
	using C = std::remove_cv_t<Component>;
	// the ranges and the drawn mesh
	return std::is_same_v<C, Mesh> || std::is_same_v<C, Transform> || std::is_same_v<C, DrawMesh> ||
	       std::is_same_v<C, MorphWithTerrain> || std::is_same_v<C, TempleInteriorPart> || std::is_same_v<C, Alpha> ||
	       std::is_same_v<C, NotDrawn> || std::is_same_v<C, NeedsSorting> || std::is_same_v<C, Unavailable> ||
	       // the static shadow casters
	       std::is_same_v<C, Fixed> || std::is_same_v<C, MobileStatic> || std::is_same_v<C, MobileObject> ||
	       std::is_same_v<C, Tree> || std::is_same_v<C, Abode> || std::is_same_v<C, Feature> || std::is_same_v<C, BigForest> ||
	       std::is_same_v<C, Pot> || std::is_same_v<C, AnimatedStatic> || std::is_same_v<C, DeadTree> ||
	       std::is_same_v<C, Field> || std::is_same_v<C, Villager> || std::is_same_v<C, Creature> || std::is_same_v<C, Hand>;
}();

/// Whether any of these components changes the draw layout
template <typename... Components>
constexpr bool k_AnyChangesDrawLayout = (k_ChangesDrawLayout<Components> || ...);

} // namespace openblack::ecs
