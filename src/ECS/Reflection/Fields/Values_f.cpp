/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Written by tools/inspector/generate_component_fields.py: run it again rather than editing this file.

#include <array>
#include <span>
#include <string_view>

#include "3D/FieldCrop.h"
#include "Animals/FishShoal.h"
#include "Common/FramePace.h"
#include "ECS/Reflection/ComponentReflection.h"
#include "Fire/FireGraphic.h"
#include "Fire/FireModel.h"
#include "Video/FilmCreature.h"
#include "Video/FilmEffects.h"

namespace openblack::ecs::reflection
{
void RegisterFields_Values_f(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Values_f(entt::meta_ctx& context)
{
	Reflect<openblack::field_crop::Crop>(context, ValueOnly {})
	    .Field<&openblack::field_crop::Crop::timesSown>("timesSown")
	    .Field<&openblack::field_crop::Crop::age>("age")
	    .Field<&openblack::field_crop::Crop::food>("food");
	Reflect<openblack::field_crop::Settle>(context, ValueOnly {})
	    .Field<&openblack::field_crop::Settle::position>("position")
	    .Field<&openblack::field_crop::Settle::speed>("speed");
	Reflect<openblack::film_creature::Clock>(context, ValueOnly {}).Field<&openblack::film_creature::Clock::lastMs>("lastMs");
	Reflect<openblack::film_creature::Look>(context, ValueOnly {})
	    .Field<&openblack::film_creature::Look::colour>("colour")
	    .Field<&openblack::film_creature::Look::added>("added");
	Reflect<openblack::film_effects::Burst>(context, ValueOnly {})
	    .Field<&openblack::film_effects::Burst::rays>("rays")
	    .Field<&openblack::film_effects::Burst::spin>("spin")
	    .Field<&openblack::film_effects::Burst::open>("open")
	    .Field<&openblack::film_effects::Burst::grow>("grow");
	Reflect<openblack::film_effects::FanCorner>(context, ValueOnly {})
	    .Field<&openblack::film_effects::FanCorner::pixel>("pixel")
	    .Field<&openblack::film_effects::FanCorner::uv>("uv")
	    .Field<&openblack::film_effects::FanCorner::argb>("argb");
	Reflect<openblack::film_effects::Ray>(context, ValueOnly {})
	    .Field<&openblack::film_effects::Ray::length>("length")
	    .Field<&openblack::film_effects::Ray::angle>("angle")
	    .Field<&openblack::film_effects::Ray::speed>("speed")
	    .Field<&openblack::film_effects::Ray::wobble>("wobble");
	Reflect<openblack::film_effects::Star>(context, ValueOnly {})
	    .Field<&openblack::film_effects::Star::screen>("screen")
	    .Field<&openblack::film_effects::Star::depth>("depth")
	    .Field<&openblack::film_effects::Star::size>("size")
	    .Field<&openblack::film_effects::Star::age>("age")
	    .Field<&openblack::film_effects::Star::spin>("spin")
	    .Field<&openblack::film_effects::Star::shrink>("shrink")
	    .Field<&openblack::film_effects::Star::rgb>("rgb");
	Reflect<openblack::film_effects::StarSprite>(context, ValueOnly {})
	    .Field<&openblack::film_effects::StarSprite::pixel>("pixel")
	    .Field<&openblack::film_effects::StarSprite::depth>("depth")
	    .Field<&openblack::film_effects::StarSprite::halfSize>("halfSize")
	    .Field<&openblack::film_effects::StarSprite::roll>("roll")
	    .Field<&openblack::film_effects::StarSprite::frame>("frame")
	    .Field<&openblack::film_effects::StarSprite::argb>("argb");
	Reflect<openblack::fire::State>(context, ValueOnly {})
	    .Field<&openblack::fire::State::temperature>("temperature")
	    .Field<&openblack::fire::State::previous>("previous")
	    .Field<&openblack::fire::State::charring>("charring")
	    .Field<&openblack::fire::State::flags>("flags");
	Reflect<openblack::fire::graphic::Graphic>(context, ValueOnly {})
	    .Field<&openblack::fire::graphic::Graphic::kinds>("kinds")
	    .Field<&openblack::fire::graphic::Graphic::maxFlames>("maxFlames")
	    .Field<&openblack::fire::graphic::Graphic::localScale>("localScale")
	    .Field<&openblack::fire::graphic::Graphic::flameAccumulator>("flameAccumulator")
	    .Field<&openblack::fire::graphic::Graphic::flameCount>("flameCount")
	    .Field<&openblack::fire::graphic::Graphic::flared>("flared")
	    .Field<&openblack::fire::graphic::Graphic::flames>("flames")
	    .Field<&openblack::fire::graphic::Graphic::steamStart>("steamStart")
	    .Field<&openblack::fire::graphic::Graphic::steamAccumulator>("steamAccumulator")
	    .Field<&openblack::fire::graphic::Graphic::steamCount>("steamCount")
	    .Field<&openblack::fire::graphic::Graphic::steamTemperature>("steamTemperature")
	    .Field<&openblack::fire::graphic::Graphic::steam>("steam")
	    .Field<&openblack::fire::graphic::Graphic::smokeStart>("smokeStart")
	    .Field<&openblack::fire::graphic::Graphic::smokeAccumulator>("smokeAccumulator")
	    .Field<&openblack::fire::graphic::Graphic::smokeCount>("smokeCount")
	    .Field<&openblack::fire::graphic::Graphic::smokePoint>("smokePoint")
	    .Field<&openblack::fire::graphic::Graphic::smokeBone>("smokeBone")
	    .Field<&openblack::fire::graphic::Graphic::smoke>("smoke")
	    .Field<&openblack::fire::graphic::Graphic::drawnTurn>("drawnTurn")
	    .Field<&openblack::fire::graphic::Graphic::drawnFraction>("drawnFraction");
	Reflect<openblack::fire::graphic::Kinds>(context, ValueOnly {})
	    .Field<&openblack::fire::graphic::Kinds::flames>("flames")
	    .Field<&openblack::fire::graphic::Kinds::smoke>("smoke")
	    .Field<&openblack::fire::graphic::Kinds::steam>("steam")
	    .Field<&openblack::fire::graphic::Kinds::lightMap>("lightMap")
	    .Field<&openblack::fire::graphic::Kinds::followsLand>("followsLand");
	Reflect<openblack::fire::graphic::Sprite>(context, ValueOnly {})
	    .Field<&openblack::fire::graphic::Sprite::position>("position")
	    .Field<&openblack::fire::graphic::Sprite::bone>("bone")
	    .Field<&openblack::fire::graphic::Sprite::scale>("scale")
	    .Field<&openblack::fire::graphic::Sprite::age>("age")
	    .Field<&openblack::fire::graphic::Sprite::alpha>("alpha")
	    .Field<&openblack::fire::graphic::Sprite::velocity>("velocity")
	    .Field<&openblack::fire::graphic::Sprite::baseScale>("baseScale");
	Reflect<openblack::fish_shoal::Fish>(context, ValueOnly {})
	    .Field<&openblack::fish_shoal::Fish::position>("position")
	    .Field<&openblack::fish_shoal::Fish::heading>("heading")
	    .Field<&openblack::fish_shoal::Fish::speed>("speed")
	    .Field<&openblack::fish_shoal::Fish::turnRate>("turnRate")
	    .Field<&openblack::fish_shoal::Fish::phase>("phase")
	    .Field<&openblack::fish_shoal::Fish::panic>("panic")
	    .Field<&openblack::fish_shoal::Fish::size>("size")
	    .Field<&openblack::fish_shoal::Fish::frame>("frame");
	Reflect<openblack::fish_shoal::Shoal>(context, ValueOnly {})
	    .Field<&openblack::fish_shoal::Shoal::centre>("centre")
	    .Field<&openblack::fish_shoal::Shoal::target>("target")
	    .Field<&openblack::fish_shoal::Shoal::retargetSeconds>("retargetSeconds")
	    .Field<&openblack::fish_shoal::Shoal::fullness>("fullness")
	    .Field<&openblack::fish_shoal::Shoal::fish>("fish");
	Reflect<openblack::frame_pace::Pace>(context, ValueOnly {})
	    .Field<&openblack::frame_pace::Pace::sinceStep>("sinceStep")
	    .Field<&openblack::frame_pace::Pace::gameSinceStep>("gameSinceStep");
}
