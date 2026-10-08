/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MagicTree.h"

#include "3D/LandIslandInterface.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Components/MagicTree.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Tree.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Trees.h"
#include "Locator.h"
#include "Magic/Script/ScriptPlayer.h"

using namespace openblack;
using namespace openblack::magic;
using namespace openblack::ecs::components;

entt::entity magic_tree::Create(const glm::vec3& position, entt::entity spell, TreeInfo type, uint32_t forestId, float angle,
                                float scale, float woodValueMultiplier)
{
	auto& registry = Locator::entitiesRegistry::value();
	const float ground = Locator::terrainSystem::value().GetHeightAt(glm::vec2(position.x, position.z));
	// a tree in the forest, growing since maxScale 1.0 is
	// not the scale, with its random growth counter (ECS/Trees: TreeArchetype::Create)
	const auto tree = ecs::archetypes::TreeArchetype::Create(forestId, glm::vec3(position.x, ground, position.z), type, true,
	                                                         angle, 1.0f, scale);
	if (tree == entt::null)
	{
		return entt::null;
	}
	// the spell's player; no spell: the neutral one (Magic/Script/ScriptPlayer.h). A spell without a player would make
	// the original read through a null pointer; here it stays neutral (inferred: no caller does that)
	PlayerNames player = k_NeutralPlayerSlot;
	if (spell != entt::null && registry.Valid(spell) && registry.Get<const Spell>(spell).hasPlayer)
	{
		player = registry.Get<const Spell>(spell).player;
	}
	registry.Assign<MagicTree>(tree, player, woodValueMultiplier);
	// the tree's wood value reads the multiplier (ECS/Trees keeps it in Tree::woodValueMultiplier, 1 for a plain tree)
	registry.Get<Tree>(tree).woodValueMultiplier = woodValueMultiplier;
	ecs::effects::reactions::CreateReaction(tree, Reaction::ReactToMagicTree, player, false);
	return tree;
}

void magic_tree::StartOnFire(entt::entity tree)
{
	ecs::effects::reactions::RemoveAllReactionsOfTypeInitiatedBy(tree, Reaction::ReactToMagicTree);
}

void magic_tree::EndOnFire(entt::entity tree)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(tree) || !registry.AllOf<MagicTree>(tree))
	{
		return;
	}
	ecs::effects::reactions::CreateReaction(tree, Reaction::ReactToMagicTree, registry.Get<const MagicTree>(tree).player,
	                                        false);
}

float magic_tree::WoodValueMultiplier(entt::entity tree)
{
	auto& registry = Locator::entitiesRegistry::value();
	// a dead tree made from it (the same entity here) has its own wood value
	if (const auto* magic = registry.TryGet<const MagicTree>(tree); magic != nullptr && registry.AllOf<Tree>(tree))
	{
		return magic->woodValueMultiplier;
	}
	return 1.0f;
}

void magic_tree::ToBeDeleted(entt::entity tree)
{
	// the tree's deletion: ECS/Trees' DeleteTree (out of its forest and the
	// physics, gone); its reactions and the rest go in the tree-deleted listener below
	ecs::DeleteTree(tree);
}

namespace
{
/// The rest of a tree's deletion for a tree ECS/Trees takes away (DeleteTree, ShrinkAllTrees, FellTree, the hand's
/// MakeDeadTree): its reactions go (a MagicTree's REACT_TO_MAGIC_TREE with them) and a magic tree's forest is noted
/// ("the forest goes with its last tree", SpellForest). Only when the entity is destroyed (Removed): its fire goes and
/// the hand lets go of it (inferred). BecameDeadTree: the entity stays as the dead tree that took over the tree's 3D
/// object and fire, which is not a MagicTree.
void OnTreeDeleted(entt::entity tree, ecs::TreeDeletion how)
{
	auto& registry = Locator::entitiesRegistry::value();
	const bool magic = registry.AllOf<MagicTree>(tree);
	if (const auto* component = registry.TryGet<const Tree>(tree); component != nullptr && component->forestId != 0 && magic)
	{
		// The forest spell reads it later (the listener runs while the tree is still in its forest); without the
		// forests nobody could read it
		if (Locator::forestSystem::has_value())
		{
			Locator::forestSystem::value().NoteForestLostAMagicTree(component->forestId);
		}
	}
	ecs::effects::reactions::RemoveAllReactionsInitiatedByObject(tree);
	if (how == ecs::TreeDeletion::BecameDeadTree)
	{
		if (magic)
		{
			registry.Remove<MagicTree>(tree); // the dead tree is another object (inferred: the same entity in openblack)
		}
		return;
	}
	if (auto* fire = ecs::fire::Find(tree); fire != nullptr)
	{
		ecs::fire::ToBeDeleted(*fire);
	}
	if (Locator::handSystem::has_value())
	{
		const auto held = Locator::handSystem::value().GetHeldObject();
		if (held.has_value() && *held == tree)
		{
			Locator::handSystem::value().ForceDropHeld();
		}
	}
}
} // namespace

void magic_tree::RegisterTreeListener()
{
	ecs::AddTreeDeletedListener(&OnTreeDeleted);
}

bool magic_tree::ForestLostAMagicTree(uint32_t forestId)
{
	return Locator::forestSystem::has_value() && Locator::forestSystem::value().TakeForestLostAMagicTree(forestId);
}

void magic_tree::Clear()
{
	if (Locator::forestSystem::has_value())
	{
		Locator::forestSystem::value().ClearForestsThatLostAMagicTree();
	}
}
