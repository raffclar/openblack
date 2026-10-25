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

#include "ECS/Components/ScriptAnimation.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/ScriptRemind.h"
#include "ECS/Components/ScriptSpotVisual.h"
#include "ECS/Components/ScriptStateArguments.h"
#include "ECS/Components/ScriptTimer.h"
#include "ECS/Components/SeeThrough.h"
#include "ECS/Components/Shark.h"
#include "ECS/Components/Sky.h"
#include "ECS/Components/SnowLevel.h"
#include "ECS/Components/SoundTag.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/StormCloud.h"
#include "ECS/Components/Stream.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Swayable.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace components = openblack::ecs::components;

namespace openblack::ecs::reflection
{
void RegisterFields_Components_s(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Components_s(entt::meta_ctx& context)
{
	Reflect<components::ScriptAnimation>(context)
	    .Field<&components::ScriptAnimation::clip>("clip")
	    .Field<&components::ScriptAnimation::playsLeft>("playsLeft");
	Reflect<components::InScript> {context};
	Reflect<components::ScriptControlled> {context};
	Reflect<components::ScriptMarker> {context};
	Reflect<components::ScriptHighlight>(context)
	    .Field<&components::ScriptHighlight::kind>("kind")
	    .Field<&components::ScriptHighlight::scriptId>("scriptId")
	    .Field<&components::ScriptHighlight::category>("category")
	    .Field<&components::ScriptHighlight::active>("active")
	    .Field<&components::ScriptHighlight::drawHeight>("drawHeight")
	    .Field<&components::ScriptHighlight::heightAbove>("heightAbove")
	    .Field<&components::ScriptHighlight::yAngle>("yAngle")
	    .Field<&components::ScriptHighlight::glints>("glints")
	    .Field<&components::ScriptHighlight::activeEffect>("activeEffect")
	    .Field<&components::ScriptHighlight::glow>("glow")
	    .Field<&components::ScriptHighlight::centre>("centre")
	    .Field<&components::ScriptHighlight::radius>("radius")
	    .Field<&components::ScriptHighlight::sparks>("sparks")
	    .Field<&components::ScriptHighlight::sparkSprites>("sparkSprites")
	    .Field<&components::ScriptHighlight::onScreen>("onScreen");
	Reflect<components::ScriptHighlightGlow>(context).Field<&components::ScriptHighlightGlow::highlight>("highlight");
	Reflect<components::ScriptRemind>(context)
	    .Field<&components::ScriptRemind::returnTo>("returnTo")
	    .Field<&components::ScriptRemind::goal>("goal")
	    .Field<&components::ScriptRemind::arguments>("arguments")
	    .Field<&components::ScriptRemind::animation>("animation")
	    .Field<&components::ScriptRemind::clip>("clip");
	Reflect<components::ScriptSpotVisual>(context).Field<&components::ScriptSpotVisual::effect>("effect");
	Reflect<components::ScriptStateArguments>(context)
	    .Field<&components::ScriptStateArguments::place>("place")
	    .Field<&components::ScriptStateArguments::number>("number")
	    .Field<&components::ScriptStateArguments::first>("first")
	    .Field<&components::ScriptStateArguments::second>("second");
	Reflect<components::ScriptTimer>(context).Field<&components::ScriptTimer::timer>("timer");
	Reflect<components::SeeThrough>(context).Field<&components::SeeThrough::alpha>("alpha");
	Reflect<components::Shark>(context)
	    .Field<&components::Shark::position>("position")
	    .Field<&components::Shark::turnStart>("turnStart")
	    .Field<&components::Shark::heading>("heading")
	    .Field<&components::Shark::clipPlace>("clipPlace")
	    .Field<&components::Shark::drawn>("drawn");
	Reflect<components::SkyDome>(context)
	    .Field<&components::SkyDome::meshId>("meshId")
	    .Field<&components::SkyDome::textureId>("textureId")
	    .Field<&components::SkyDome::follow>("follow")
	    .Field<&components::SkyDome::frameRows>("frameRows")
	    .Field<&components::SkyDome::overcast>("overcast")
	    .Field<&components::SkyDome::landLight>("landLight");
	Reflect<components::DayNightCycle>(context).Field<&components::DayNightCycle::clock>("clock");
	Reflect<components::CelestialBody>(context)
	    .Field<&components::CelestialBody::meshId>("meshId")
	    .Field<&components::CelestialBody::textureId>("textureId")
	    .Field<&components::CelestialBody::alphaTextureId>("alphaTextureId");
	Reflect<components::CelestialGlow>(context)
	    .Field<&components::CelestialGlow::textureId>("textureId")
	    .Field<&components::CelestialGlow::alphaTextureId>("alphaTextureId");
	Reflect<components::Sun>(context)
	    .Field<&components::Sun::placement>("placement")
	    .Field<&components::Sun::colour>("colour")
	    .Field<&components::Sun::strength>("strength");
	Reflect<components::Moon>(context)
	    .Field<&components::Moon::phase>("phase")
	    .Field<&components::Moon::date>("date")
	    .Field<&components::Moon::dateReadAt>("dateReadAt")
	    .Field<&components::Moon::dateOverride>("dateOverride")
	    .Field<&components::Moon::placement>("placement")
	    .Field<&components::Moon::colour>("colour")
	    .Field<&components::Moon::strength>("strength");
	Reflect<components::SnowLevel>(context).Field<&components::SnowLevel::level>("level");
	Reflect<components::SoundTag>(context)
	    .Field<&components::SoundTag::sound>("sound")
	    .Field<&components::SoundTag::offset>("offset")
	    .Field<&components::SoundTag::active>("active")
	    .Field<&components::SoundTag::emitter>("emitter")
	    .Field<&components::SoundTag::point>("point")
	    .Field<&components::SoundTag::delayed>("delayed")
	    .Field<&components::SoundTag::turns>("turns")
	    .Field<&components::SoundTag::owner>("owner")
	    .Field<&components::SoundTag::loops>("loops");
	Reflect<components::SpellCaster>(context)
	    .Field<&components::SpellCaster::kind>("kind")
	    .Field<&components::SpellCaster::player>("player")
	    .Field<&components::SpellCaster::entity>("entity")
	    .Field<&components::SpellCaster::withoutIcon>("withoutIcon")
	    .Field<&components::SpellCaster::worshipSite>("worshipSite")
	    .Field<&components::SpellCaster::madeBySpellIcon>("madeBySpellIcon");
	Reflect<components::Spell>(context)
	    .Field<&components::Spell::magicType>("magicType")
	    .Field<&components::Spell::spellClass>("spellClass")
	    .Field<&components::Spell::caster>("caster")
	    .Field<&components::Spell::chants>("chants")
	    .Field<&components::Spell::age>("age")
	    .Field<&components::Spell::duration>("duration")
	    .Field<&components::Spell::magnitude>("magnitude")
	    .Field<&components::Spell::position>("position")
	    .Field<&components::Spell::castPosition>("castPosition")
	    .Field<&components::Spell::originalCastPosition>("originalCastPosition")
	    .Field<&components::Spell::direction>("direction")
	    .Field<&components::Spell::movement>("movement")
	    .Field<&components::Spell::processInfo>("processInfo")
	    .Field<&components::Spell::effect>("effect")
	    .Field<&components::Spell::castEffect>("castEffect")
	    .Field<&components::Spell::seed>("seed")
	    .Field<&components::Spell::target>("target")
	    .Field<&components::Spell::closedDown>("closedDown")
	    .Field<&components::Spell::hasParticleType>("hasParticleType")
	    .Field<&components::Spell::fromLocalHand>("fromLocalHand")
	    .Field<&components::Spell::castFromHand>("castFromHand")
	    .Field<&components::Spell::reaction>("reaction")
	    .Field<&components::Spell::humanCasting>("humanCasting")
	    .Field<&components::Spell::maxObjectsToCreate>("maxObjectsToCreate")
	    .Field<&components::Spell::resourceFirstDone>("resourceFirstDone")
	    .Field<&components::Spell::lastRipple>("lastRipple")
	    .Field<&components::Spell::forestPlanted>("forestPlanted")
	    .Field<&components::Spell::objectCount>("objectCount");
	Reflect<components::SpellDispenser>(context)
	    .Field<&components::SpellDispenser::magicType>("magicType")
	    .Field<&components::SpellDispenser::timer>("timer")
	    .Field<&components::SpellDispenser::orb>("orb")
	    .Field<&components::SpellDispenser::orbPosition>("orbPosition")
	    .Field<&components::SpellDispenser::effect>("effect")
	    .Field<&components::SpellDispenser::building>("building")
	    .Field<&components::SpellDispenser::town>("town")
	    .Field<&components::SpellDispenser::putUpForMending>("putUpForMending")
	    .Field<&components::SpellDispenser::age>("age");
	Reflect<components::TestbedDispenser> {context};
	Reflect<components::TownMagic>(context).Field<&components::TownMagic::held>("held");
	Reflect<components::TownCentreIcon>(context)
	    .Field<&components::TownCentreIcon::centre>("centre")
	    .Field<&components::TownCentreIcon::town>("town")
	    .Field<&components::TownCentreIcon::hand>("hand")
	    .Field<&components::TownCentreIcon::seed>("seed")
	    .Field<&components::TownCentreIcon::powerUps>("powerUps");
	Reflect<components::CentreHands>(context).Field<&components::CentreHands::hands>("hands");
	Reflect<components::SiteIcons>(context).Field<&components::SiteIcons::icons>("icons");
	Reflect<components::WorshipIcon>(context)
	    .Field<&components::WorshipIcon::site>("site")
	    .Field<&components::WorshipIcon::seed>("seed")
	    .Field<&components::WorshipIcon::charging>("charging")
	    .Field<&components::WorshipIcon::powerUp>("powerUp")
	    .Field<&components::WorshipIcon::charger>("charger")
	    .Field<&components::WorshipIcon::chargeStartTurn>("chargeStartTurn")
	    .Field<&components::WorshipIcon::store>("store")
	    .Field<&components::WorshipIcon::seedsOut>("seedsOut");
	Reflect<components::IconSeed>(context)
	    .Field<&components::IconSeed::seed>("seed")
	    .Field<&components::IconSeed::middle>("middle")
	    .Field<&components::IconSeed::powerUp>("powerUp")
	    .Field<&components::IconSeed::spin>("spin")
	    .Field<&components::IconSeed::ringSpin>("ringSpin")
	    .Field<&components::IconSeed::phialFrame>("phialFrame")
	    .Field<&components::IconSeed::phialPhase>("phialPhase");
	Reflect<components::SpellSeed>(context)
	    .Field<&components::SpellSeed::seedType>("seedType")
	    .Field<&components::SpellSeed::powerUp>("powerUp")
	    .Field<&components::SpellSeed::player>("player")
	    .Field<&components::SpellSeed::chantStore>("chantStore")
	    .Field<&components::SpellSeed::learnedFrom>("learnedFrom")
	    .Field<&components::SpellSeed::storedChants>("storedChants")
	    .Field<&components::SpellSeed::storedAge>("storedAge")
	    .Field<&components::SpellSeed::storedMaxObjects>("storedMaxObjects")
	    .Field<&components::SpellSeed::castMultiplier>("castMultiplier")
	    .Field<&components::SpellSeed::power>("power")
	    .Field<&components::SpellSeed::origin>("origin")
	    .Field<&components::SpellSeed::fromGlobe>("fromGlobe")
	    .Field<&components::SpellSeed::hasIcon>("hasIcon")
	    .Field<&components::SpellSeed::worshipSite>("worshipSite")
	    .Field<&components::SpellSeed::ready>("ready")
	    .Field<&components::SpellSeed::holdType>("holdType")
	    .Field<&components::SpellSeed::followsSpell>("followsSpell")
	    .Field<&components::SpellSeed::turnsInHand>("turnsInHand")
	    .Field<&components::SpellSeed::spell>("spell")
	    .Field<&components::SpellSeed::hasCast>("hasCast")
	    .Field<&components::SpellSeed::handEffect>("handEffect");
	Reflect<components::Sprite>(context)
	    .Field<&components::Sprite::texture>("texture")
	    .Field<&components::Sprite::uvMin>("uvMin")
	    .Field<&components::Sprite::uvExtent>("uvExtent")
	    .Field<&components::Sprite::tint>("tint")
	    .Field<&components::Sprite::additive>("additive")
	    .Field<&components::Sprite::facesCamera>("facesCamera")
	    .Field<&components::Sprite::alpha>("alpha");
	Reflect<components::StoragePit>(context)
	    .Field<&components::StoragePit::woodPiles>("woodPiles")
	    .Field<&components::StoragePit::foodPile>("foodPile");
	Reflect<components::StormCloud>(context)
	    .Field<&components::StormCloud::storm>("storm")
	    .Field<&components::StormCloud::index>("index")
	    .Field<&components::StormCloud::drift>("drift")
	    .Field<&components::StormCloud::sizeFactor>("sizeFactor")
	    .Field<&components::StormCloud::shade>("shade")
	    .Field<&components::StormCloud::flashGrey>("flashGrey");
	Reflect<components::Stream>(context).Field<&components::Stream::id>("id").Field<&components::Stream::points>("points");
	Reflect<components::StreamSegment> {context};
	Reflect<components::StreetLantern>(context)
	    .Field<&components::StreetLantern::country>("country")
	    .Field<&components::StreetLantern::info>("info");
	Reflect<components::Swayable>(context).Field<&components::Swayable::swaySlot>("swaySlot");
}
