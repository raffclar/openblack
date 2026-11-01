/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>

#include <entt/entity/fwd.hpp>

namespace openblack::ecs
{

class Registry;

/// How a thing casts its shadow into the land's textures (see graphics::shape_shadows and graphics::tree_shadows)
enum class LandShadow : uint8_t
{
	/// It casts none
	None,
	/// Its triangles that face the sun cast, and those drawn from both sides
	Shape,
	/// Every one of its triangles casts: the temple's
	ShapeBothSides,
	/// Its triangles cast through its textures' alpha: the trees' and the big forests'
	Leaves,
};

/// How a thing casts its shadow into the land. Buildings cast once they are built (a football pitch and a worship site
/// from the start), and so do the island's features, rocks, teleport stones, spell dispensers, balls, rewards and the
/// other things lying about. Fields, animated scenery, flowers, dead trees, pots and piles, the village totem, miracle
/// shields, seeds and vortices, street lights and anything living cast none. A thing with NoLandShadow casts none.
[[nodiscard]] LandShadow LandShadowOf(const Registry& registry, entt::entity entity);

/// The stores of the components that LandShadowOf reads, found one time for a walk of many things. Each look after that
/// costs no search of the registry, and gives the same shadow as LandShadowOf. Make it again when the walk can make the
/// first component of a kind.
class LandShadowLookup
{
public:
	explicit LandShadowLookup(const Registry& registry);
	[[nodiscard]] LandShadow Of(entt::entity entity) const;

	/// How many kinds of component the shadow depends on
	static constexpr size_t k_StoreCount = 28;

private:
	template <typename... Components>
	[[nodiscard]] bool AnyOf(entt::entity entity) const;
	template <typename Component>
	[[nodiscard]] const Component* TryGet(entt::entity entity) const;
	[[nodiscard]] bool IsBuilt(entt::entity entity) const;

	std::array<const entt::sparse_set*, k_StoreCount> _stores {};
};

/// Whether the shadow that LandShadowOf gives a thing can change while the thing keeps all its components: a building's
/// shadow starts when its building ends, and an abode's shadow also depends on its type
[[nodiscard]] bool LandShadowChangesInPlace(const Registry& registry, entt::entity entity);

/// A count that increases each time a thing with a mesh and a transform gets or loses a component that its land shadow
/// depends on. While the count stays the same, the things that can cast into the land stay the same, and the shadow of
/// each can change only with its transform, its mesh, its map cells, or (see LandShadowChangesInPlace) its state.
[[nodiscard]] uint64_t LandShadowCastersVersion(Registry& registry);

} // namespace openblack::ecs
