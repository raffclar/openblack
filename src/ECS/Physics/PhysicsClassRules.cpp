/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PhysicsClassRules.h"

using namespace openblack;
using namespace openblack::ecs::physics;

bool class_rules::StandingBuilding(float percentBuilt, float life)
{
	return percentBuilt > k_LeastBuiltToHit && life > k_LeastLifeToHit;
}

bool class_rules::AbodeIsObstacle(AbodeNumber type, float percentBuilt, float life)
{
	switch (type)
	{
	case AbodeNumber::TownCentre:
		return true;
	case AbodeNumber::Graveyard:
	case AbodeNumber::FootballPitch:
	case AbodeNumber::Field:
		return false;
	default:
		return StandingBuilding(percentBuilt, life);
	}
}

bool class_rules::CitadelHeartIsObstacle(float percentBuilt)
{
	return static_cast<double>(percentBuilt) > k_LeastHeartBuiltToHit;
}

bool class_rules::IsHeavyStatic(MobileStaticInfo type)
{
	return (type >= MobileStaticInfo::GateTotemApe && type <= MobileStaticInfo::GateTotemTiger) ||
	       type == MobileStaticInfo::WeepingStone || type == MobileStaticInfo::WeepingStoneReward ||
	       type == MobileStaticInfo::SingingStone_1;
}

bool class_rules::IsToyModel(MeshId mesh)
{
	return mesh >= MeshId::ObjectToyBall && mesh <= MeshId::ObjectToySkittle;
}

bool class_rules::IsFenceModel(MeshId mesh)
{
	return mesh == MeshId::BuildingAmericanFence || mesh == MeshId::BuildingCelticFenceShort ||
	       mesh == MeshId::BuildingCelticFenceTall;
}

bool class_rules::IsPlainObjectStatic(MobileStaticInfo type)
{
	return type == MobileStaticInfo::StreetLantern || type == MobileStaticInfo::CountryLantern ||
	       type == MobileStaticInfo::SingingStoneBase || type == MobileStaticInfo::Bonfire;
}

bool class_rules::MobileStaticIsObstacle(MobileStaticInfo type, MobileStaticInfo mobileType, MeshId mesh, float percentBuilt,
                                         float life)
{
	// The lanterns and the bonfire are never hit; the singing stone's base is a plain object with a model, so it is
	if (type == MobileStaticInfo::StreetLantern || type == MobileStaticInfo::CountryLantern ||
	    type == MobileStaticInfo::Bonfire)
	{
		return false;
	}
	if (type == MobileStaticInfo::SingingStoneBase)
	{
		return true;
	}
	if (IsToyModel(mesh) || IsHeavyStatic(type) || IsFenceModel(mesh) || mobileType == MobileStaticInfo::Rock ||
	    mobileType == MobileStaticInfo::Idol)
	{
		return true;
	}
	return StandingBuilding(percentBuilt, life);
}

bool class_rules::MobileStaticCanBecomePhysicsObject(MobileStaticInfo type)
{
	return !IsPlainObjectStatic(type);
}

bool class_rules::MobileObjectIsObstacle(MobileObjectInfo type)
{
	return type != MobileObjectInfo::Creed;
}

bool class_rules::MobileObjectCanBecomePhysicsObject(MobileObjectInfo type)
{
	return type != MobileObjectInfo::Whale && type != MobileObjectInfo::Creed && type != MobileObjectInfo::HanoiPuzzleBase;
}

bool class_rules::AnimatedStaticIsObstacle(AnimatedStaticInfo type)
{
	return type == AnimatedStaticInfo::NorseGate || type == AnimatedStaticInfo::GateStonePlinth ||
	       type == AnimatedStaticInfo::PiperCaveEntrance || type == AnimatedStaticInfo::PhoneBox;
}

std::optional<MeshId> class_rules::AnimatedStaticCollisionMesh(AnimatedStaticInfo type, int32_t openState, int32_t plinthState,
                                                               int32_t plinthFull)
{
	switch (type)
	{
	case AnimatedStaticInfo::NorseGate:
		return openState == 1 ? MeshId::NorseGatePhys2 : MeshId::NorseGatePhys1;
	case AnimatedStaticInfo::GateStonePlinth:
		if (openState != 1 && plinthState != 0)
		{
			return plinthFull != 0 ? MeshId::GateTotemPlinthePhys3 : MeshId::GateTotemPlinthePhys2;
		}
		return MeshId::GateTotemPlinthePhys1;
	case AnimatedStaticInfo::PhoneBox:
		return MeshId::GateTotemPlinthePhys1;
	case AnimatedStaticInfo::PiperCaveEntrance:
		return MeshId::PiperEntrancePhys1;
	default:
		return std::nullopt;
	}
}
