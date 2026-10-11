/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerHeldObject.h"

#include <glm/vec4.hpp>

using namespace openblack;
using namespace openblack::ecs;

std::optional<MeshId> villager_held::MeshOf(CarriedObject carried)
{
	switch (carried)
	{
	case CarriedObject::Axe:
		return MeshId::ObjectAxe;
	case CarriedObject::FishingRod:
		return MeshId::ObjectFishingRod;
	case CarriedObject::Crook:
		return MeshId::ObjectCrook;
	case CarriedObject::Saw:
		return MeshId::ObjectSaw;
	case CarriedObject::Bag:
		return MeshId::ObjectBag;
	case CarriedObject::Ball:
		return MeshId::ObjectBall;
	case CarriedObject::Hammer:
		return MeshId::ObjectHammer;
	case CarriedObject::MalletHeavy:
		return MeshId::ObjectMalletHeavy;
	case CarriedObject::Scythe:
		return MeshId::ObjectScythe;
	case CarriedObject::Spade:
		return MeshId::ObjectSpade;
	case CarriedObject::Wood:
		return MeshId::ObjectWoodInHand;
	case CarriedObject::Tree_1:
		return MeshId::ObjectBranchEvergreenInHand;
	case CarriedObject::Tree_2:
		return MeshId::ObjectBranchFruitInHand;
	case CarriedObject::Tree_3:
		return MeshId::ObjectBranchHardWoodInHand;
	case CarriedObject::NoChange:
	case CarriedObject::None:
	default:
		return std::nullopt;
	}
}

glm::mat4 villager_held::HeldMatrix(const glm::mat4& villagerModel, const glm::mat4& gripBone)
{
	// The held model's sideways axis runs against the grip's, its up axis against the grip's forward and its forward
	// against the grip's up
	const glm::mat4 turn {
	    glm::vec4(-1.0f, 0.0f, 0.0f, 0.0f),
	    glm::vec4(0.0f, 0.0f, -1.0f, 0.0f),
	    glm::vec4(0.0f, -1.0f, 0.0f, 0.0f),
	    glm::vec4(0.0f, 0.0f, 0.0f, 1.0f),
	};
	return villagerModel * gripBone * turn;
}

std::optional<uint8_t> villager_held::HeldAlpha(std::optional<uint8_t> ownFade, std::optional<uint8_t> villagerFade)
{
	return ownFade.has_value() ? ownFade : villagerFade;
}

bool villager_held::DropsLog(CarriedObject shown, int16_t wood, uint32_t minWoodToShowGraphic)
{
	return MeshOf(shown).has_value() && static_cast<int32_t>(wood) > static_cast<int32_t>(minWoodToShowGraphic);
}

float villager_held::LogWoodMultiplier(int16_t wood, float pineWoodValue)
{
	return static_cast<float>(wood) / pineWoodValue;
}

uint8_t villager_held::WoodKindOfFallenTree(MeshId mesh, CarriedTreeType treeKind)
{
	switch (mesh)
	{
	case MeshId::ObjectWoodInHand:
		return 0;
	case MeshId::ObjectBranchEvergreenInHand:
		return 1;
	case MeshId::ObjectBranchFruitInHand:
		return 2;
	case MeshId::ObjectBranchHardWoodInHand:
		return 3;
	default:
		return static_cast<uint8_t>(treeKind);
	}
}
