/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HeldCastRules.h"

using namespace openblack;

namespace
{
/// The miracles cast at a circle drawn on the land
bool CastAtCircle(MagicType type)
{
	switch (type)
	{
	case MagicType::StormWindRain:
	case MagicType::StormWindRainLightning:
	case MagicType::Tornado:
	case MagicType::Shield:
	case MagicType::PhysicalShield:
		return true;
	default:
		return false;
	}
}
} // namespace

bool magic::MayCastOn(SpellSeedType seed, const PointedThing& thing)
{
	if (thing.scriptHighlight)
	{
		return false;
	}
	if (thing.fireBall)
	{
		return seed == SpellSeedType::Fire;
	}
	return thing.castOnByKind;
}

magic::HeldCastOffer magic::OfferFor(const HeldCastAtPointer& held)
{
	if (!held.handInInfluence || !held.ready)
	{
		return HeldCastOffer::None;
	}
	// Over a thing only the thing counts, whatever the land beneath it allows
	if (held.onPointed.has_value())
	{
		return *held.onPointed ? HeldCastOffer::Cast : HeldCastOffer::None;
	}
	if (!held.atPoint)
	{
		return HeldCastOffer::None;
	}
	return CastAtCircle(held.type) ? HeldCastOffer::CastCircle : HeldCastOffer::Cast;
}
