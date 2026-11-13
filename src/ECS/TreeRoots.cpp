/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TreeRoots.h"

#include <algorithm>
#include <vector>

#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/FallingRoots.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Tree.h"
#include "ECS/MobileObjectRecord.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using physics::objects::RootedKind;
using physics::objects::TreePlace;

namespace
{
/// Whether a hand is pulling at the thing, which hasn't yet come into it
bool PulledByAHand(const Registry& registry, entt::entity object)
{
	bool pulled = false;
	registry.Each<const HandGrab>([&pulled, object](const HandGrab& grab) {
		pulled = pulled || ((grab.state == HandGrab::State::Grabbing || grab.state == HandGrab::State::Taking) &&
		                    grab.tug.has_value() && grab.object == object);
	});
	return pulled;
}
} // namespace

TreePlace tree_roots::PlaceOf(const Registry& registry, entt::entity tree)
{
	// Held or carried, it is drawn by what holds it, out of the land, whether or not the physics has it too
	if (registry.AnyOf<InHand, HeldByCreature, CarriedByTornado>(tree) || PulledByAHand(registry, tree))
	{
		return TreePlace::OutOfTheLand;
	}
	if (registry.AllOf<InPhysics>(tree))
	{
		// Only a body still moving has a pose drawn between turns; one at rest, or sunk out of sight, has none to draw
		const auto* drawn = registry.TryGet<const PhysicsDrawPose>(tree);
		return drawn != nullptr && !drawn->underSea ? TreePlace::Moving : TreePlace::Still;
	}
	return TreePlace::InTheLand;
}

void tree_roots::Show(Registry& registry, const InfoConstants& info)
{
	// The trees that may be out of the land, and those drawn with their roots last frame
	std::vector<entt::entity> trees;
	registry.Each<const Tree, const InHand>([&trees](entt::entity tree, const Tree&, const InHand&) { trees.push_back(tree); });
	registry.Each<const Tree, const InPhysics>([&trees](entt::entity tree, const Tree&) { trees.push_back(tree); });
	registry.Each<const Tree, const HeldByCreature>(
	    [&trees](entt::entity tree, const Tree&, const HeldByCreature&) { trees.push_back(tree); });
	registry.Each<const Tree, const CarriedByTornado>(
	    [&trees](entt::entity tree, const Tree&, const CarriedByTornado&) { trees.push_back(tree); });
	registry.Each<const HandGrab>([&registry, &trees](const HandGrab& grab) {
		if ((grab.state == HandGrab::State::Grabbing || grab.state == HandGrab::State::Taking) && grab.tug.has_value() &&
		    registry.Valid(grab.object) && registry.AllOf<Tree>(grab.object))
		{
			trees.push_back(grab.object);
		}
	});
	// A crop is rooted too, and has roots drawn while it moves in the physics
	const auto isCrop = [&registry, &info](entt::entity object) {
		const auto* mobile = registry.TryGet<const MobileObject>(object);
		return mobile != nullptr && mobile_object_record::ClassOf(info, *mobile) == mobile_object_record::Class::Crop;
	};
	registry.Each<const MobileObject, const InPhysics>([&trees, &isCrop](entt::entity object, const MobileObject&) {
		if (isCrop(object))
		{
			trees.push_back(object);
		}
	});
	registry.Each<const ShownRoots>([&trees](entt::entity tree, const ShownRoots&) { trees.push_back(tree); });
	std::ranges::sort(trees);
	const auto [first, last] = std::ranges::unique(trees);
	trees.erase(first, last);

	// Changed only where it changes, as each change has the draw lists made again
	for (const auto tree : trees)
	{
		if (!registry.Valid(tree))
		{
			continue;
		}
		auto place = TreePlace::InTheLand;
		auto kind = RootedKind::Tree;
		if (registry.AllOf<Tree>(tree))
		{
			place = PlaceOf(registry, tree);
		}
		else if (isCrop(tree))
		{
			place = PlaceOf(registry, tree);
			kind = RootedKind::Crop;
		}
		const bool drawn = !physics::objects::ShownRootsScales(place, kind).empty();
		const auto* shown = registry.TryGet<const ShownRoots>(tree);
		if (!drawn)
		{
			if (shown != nullptr)
			{
				registry.Remove<ShownRoots>(tree);
			}
		}
		else if (shown == nullptr || shown->place != place)
		{
			registry.AssignOrReplace<ShownRoots>(tree, ShownRoots {.place = place});
		}
	}
}

std::span<const float> tree_roots::ShownScales(const Registry& registry, entt::entity rooted, const ShownRoots& shown)
{
	// Only trees and crops have shown roots, and a crop draws fewer of them
	return physics::objects::ShownRootsScales(shown.place, registry.AllOf<Tree>(rooted) ? RootedKind::Tree : RootedKind::Crop);
}
