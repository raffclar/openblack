/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "MagicSystem.h"

#include <cmath>

#include <algorithm>
#include <chrono>
#include <limits>
#include <numbers>
#include <optional>
#include <utility>

#include <LNDFile.h>
#include <entt/core/hashed_string.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "3D/WaterRings.h"
#include "Animals/FishFarmRules.h"
#include "Camera/Camera.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureAudio.h"
#include "Creature/CreatureLocomotion.h"
#include "Creature/PerceivedDesires.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Archetypes/SpellSeedArchetype.h"
#include "ECS/BuildingSites.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/BuildingDamage.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/HandHeldSeed.h"
#include "ECS/Components/HandStillness.h"
#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/MagicPile.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/PrayerPower.h"
#include "ECS/Components/ReportedHand.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownAggression.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/CreatureSight.h"
#include "ECS/GameEndConditions.h"
#include "ECS/GamePhysicsHooksWorld.h"
#include "ECS/GamePlayers.h"
#include "ECS/Map.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Registry.h"
#include "ECS/SpellCreator.h"
#include "ECS/Statistics.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/EffectRequestSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/FootpathSystemInterface.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Systems/GameEndSystemInterface.h"
#include "ECS/Systems/GestureEventsInterface.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/MagicShieldSystemInterface.h"
#include "ECS/Systems/MiracleFxSystemInterface.h"
#include "ECS/Systems/ObjectDrawListSystemInterface.h"
#include "ECS/Systems/ParticleRuleSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/SpellIconSystemInterface.h"
#include "ECS/Systems/TeleportSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/Systems/WaterRingSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/TempleConstruction.h"
#include "ECS/TempleHeartPassOn.h"
#include "ECS/ThingMapPlace.h"
#include "ECS/WorldObjects.h"
#include "ECS/WorshipSites.h"
#include "FileSystem/FileSystemInterface.h"
#include "Hand/HandGrabRules.h"
#include "Help/HelpProfile.h"
#include "Help/VillageVoices.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/DispenserRules.h"
#include "Magic/MagicTables.h"
#include "Magic/MiracleDeeds.h"
#include "Magic/SpellLifetime.h"
#include "Magic/SpellSeedRules.h"
#include "Magic/WaterRules.h"
#include "MagicLiving.h"
#include "MagicSpellLinks.h"
#include "Particles/DrawnEffectSteps.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleMiracleMaths.h"
#include "Particles/ParticleShields.h"
#include "Resources/ResourcesInterface.h"
#include "Sound/GameSoundEffects.h"
#include "Sound/SoundId.h"
#include "Sound/SoundRequestInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
constexpr float k_TurnSeconds = std::chrono::duration<float>(TimeSystemInterface::k_TurnDuration).count();
constexpr float k_TurnsPerSecond = 1.0f / k_TurnSeconds;

/// The hand's speed is smoothed over about this many seconds
constexpr float k_HandSpeedSmoothing = 0.1f;
/// How far round a point a struck shield is looked for, to show its spark
constexpr float k_ShieldStrikeMargin = 2.0f;

ecs::Registry& EntityRegistry()
{
	return Locator::entitiesRegistry::value();
}

/// The left hand of a player, none for a player with no hands
[[nodiscard]] entt::entity LeftHandOf(PlayerNames player)
{
	if (!Locator::handSystem::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return entt::null;
	}
	const auto hands = Locator::handSystem::value().GetHandsOf(player);
	if (!hands.has_value())
	{
		return entt::null;
	}
	const auto hand = (*hands)[static_cast<size_t>(HandSystemInterface::Side::Left)];
	return Locator::entitiesRegistry::value().Valid(hand) ? hand : entt::null;
}
} // namespace

// The system

MagicSystem::MagicSystem()
    : _handEffectLink(std::make_unique<HandEffectLink>(*this))
{
	for (size_t p = 0; p < _players.size(); ++p)
	{
		// A player pays for their miracles from their prayer power
		_players.at(p) = std::make_unique<magic::PlayerSpellCaster>(static_cast<PlayerNames>(p),
		                                                            [this](PlayerNames player) { return PrayerOf(player); });
	}
}

MagicSystem::~MagicSystem() = default;

const InfoConstants& MagicSystem::Info() const
{
	return Locator::infoConstants::value();
}

magic::SpellCasterInterface* MagicSystem::CasterOf(const Spell& spell)
{
	switch (spell.caster.kind)
	{
	case SpellCaster::Kind::Player:
		// A miracle from a globe's seed is the player's own, which they top up with nothing
		if (spell.caster.withoutIcon)
		{
			_globeCaster.Bind(spell.caster.player);
			return &_globeCaster;
		}
		// A miracle from a seed a worship site's icon made is topped up by that site while it stands
		if (spell.caster.worshipSite != entt::null)
		{
			if (!EntityRegistry().Valid(spell.caster.worshipSite))
			{
				return nullptr;
			}
			_siteCaster.Bind(spell.caster.worshipSite, EntityRegistry().ToEntity(spell));
			return &_siteCaster;
		}
		return _players.at(static_cast<size_t>(spell.caster.player)).get();
	case SpellCaster::Kind::Object:
		return EntityRegistry().Valid(spell.caster.entity) ? &_objectCaster : nullptr;
	case SpellCaster::Kind::Creature:
		// A creature pays with its body while it is there
		if (!EntityRegistry().Valid(spell.caster.entity))
		{
			return nullptr;
		}
		_creatureCaster.Bind(spell.caster.entity, spell.magicType);
		return &_creatureCaster;
	case SpellCaster::Kind::None:
		break;
	}
	return nullptr;
}

namespace
{
/// The player's record on the land, if they are on it
Player* PlayerRecord(PlayerNames player)
{
	Player* found = nullptr;
	EntityRegistry().Each<Player>([player, &found](entt::entity, Player& record) {
		if (record.name == player)
		{
			found = &record;
		}
	});
	return found;
}
} // namespace

std::array<float, magic::k_TribeCount> MagicSystem::PlayerTribalMultipliers(PlayerNames player) const
{
	// The power each tribe gives the player's miracles is the player's; a player not on the land has the usual
	const auto* record = PlayerRecord(player);
	return record != nullptr ? record->miracles.tribalPower : Player::k_UsualTribalPower;
}

void MagicSystem::SetTribalPower(PlayerNames player, Tribe tribe, float power)
{
	auto* record = PlayerRecord(player);
	if (record == nullptr || tribe == Tribe::NONE || static_cast<size_t>(tribe) >= magic::k_TribeCount)
	{
		return;
	}
	// The most it has been goes with it, so that the power holds
	const auto index = static_cast<size_t>(tribe);
	record->miracles.tribalPower.at(index) = power;
	record->miracles.maxTribalPower.at(index) = power;
}

float MagicSystem::GetTribalPower(PlayerNames player, Tribe tribe) const
{
	if (tribe == Tribe::NONE || static_cast<size_t>(tribe) >= magic::k_TribeCount)
	{
		return 1.0f;
	}
	return PlayerTribalMultipliers(player).at(static_cast<size_t>(tribe));
}

float MagicSystem::PlayerTribalPower(PlayerNames player, MagicType type) const
{
	return magic::GetTribalPower(magic::GetMagicEffectInfo(Info(), type), PlayerTribalMultipliers(player));
}

float MagicSystem::TribalPower(const Spell& spell) const
{
	// A creature's miracle takes its player's tribal power
	return spell.caster.kind == SpellCaster::Kind::Player || spell.caster.kind == SpellCaster::Kind::Creature
	           ? PlayerTribalPower(spell.caster.player, spell.magicType)
	           : 1.0f;
}

float MagicSystem::SeedPower(const Spell& spell) const
{
	const auto& registry = EntityRegistry();
	if (spell.seed != entt::null && registry.Valid(spell.seed))
	{
		if (const auto* seed = registry.TryGet<const SpellSeed>(spell.seed))
		{
			return seed->power;
		}
	}
	return 1.0f;
}

void MagicSystem::AddEffectTarget(const Spell& spell, entt::entity target)
{
	if (spell.effect != k_NoEffectHandle)
	{
		Locator::effectRequestSystem::value().AddTarget(spell.effect, target);
	}
}

std::optional<entt::entity> MagicSystem::ShieldAt(glm::vec3 point, float margin)
{
	const auto shield = Locator::effectRequestSystem::value().FindShield(point, margin);
	if (!shield || shield->spell == entt::null)
	{
		return std::nullopt;
	}
	return shield->spell;
}

void MagicSystem::StrikeShield(entt::entity shieldSpell, glm::vec3 point)
{
	if (const auto shield = Locator::effectRequestSystem::value().FindShield(point, k_ShieldStrikeMargin);
	    shield && shield->spell == shieldSpell)
	{
		particles::StrikeShield(*shield, point);
	}
}

entt::entity MagicSystem::SpellEntity(const Spell& spell) const
{
	return EntityRegistry().ToEntity(spell);
}

Spell* MagicSystem::FindSpell(entt::entity spell)
{
	auto& registry = EntityRegistry();
	return registry.Valid(spell) ? registry.TryGet<Spell>(spell) : nullptr;
}

float MagicSystem::GameRandom(float max)
{
	return Locator::gameRandom::has_value() ? Locator::gameRandom::value().GameFloatRand(max) : 0.0f;
}

entt::entity MagicSystem::CastAtPoint(MagicType type, PlayerNames player, glm::vec3 point, const magic::SpellCastData& cast,
                                      const particles::ProcessInfo& info)
{
	return Cast(type, {.kind = SpellCaster::Kind::Player, .player = player, .entity = entt::null}, point, cast, info);
}

entt::entity MagicSystem::Cast(MagicType type, const components::SpellCaster& caster, glm::vec3 point,
                               const magic::SpellCastData& cast, const particles::ProcessInfo& info)
{
	const auto player = caster.player;
	if (!Locator::infoConstants::has_value() || static_cast<size_t>(type) >= magic::k_MagicTypeCount || type == MagicType::None)
	{
		return entt::null;
	}
	// The caster player's creature, seeing it, takes it the player wants what the miracle answers; the player's last cast
	// is remembered
	Empathise(type, player, point);
	RecordCast(type, player, point);
	auto& registry = EntityRegistry();
	const auto entity = registry.Create();
	auto& spell = registry.Assign<Spell>(entity);
	spell.magicType = type;
	spell.spellClass = magic::ClassOf(type);
	spell.caster = caster;
	spell.creator = ecs::spell_creator::CreatorOf(caster);
	// A person casts it: the hand of a player that a person plays
	spell.humanCasting = caster.kind == SpellCaster::Kind::Player && player != PlayerNames::NEUTRAL &&
	                     (Locator::playerSystem::has_value() ? Locator::playerSystem::value().IsHumanPlayer(player)
	                                                         : player == PlayerNames::PLAYER_ONE);
	magic::SetChants(spell.chants, cast.chants);
	spell.duration = cast.duration;
	spell.magnitude = cast.magnitude;
	spell.maxObjectsToCreate = cast.maxObjectsToCreate;
	spell.position = point;
	spell.castPosition = point;
	spell.originalCastPosition = point;
	spell.direction = info.direction;
	spell.processInfo = info;
	magic::spells::Prepare(*this, spell);

	auto link = std::make_unique<SpellLink>(*this, entity);
	const auto particleType = magic::spells::ParticleTypeOf(*this, spell);
	spell.hasParticleType = particleType != ParticleType::None;
	// The miracle steps its effect, and the effect tells the miracle of its events
	auto& effects = Locator::effectRequestSystem::value();
	spell.effect = effects.Start({.type = particleType,
	                              .position = point,
	                              .magnitude = spell.magnitude,
	                              .player = static_cast<int>(player),
	                              .direction = spell.direction,
	                              .processInfo = spell.processInfo,
	                              .sink = link.get()});
	// A miracle whose effect did not start keeps no number
	if (!effects.IsRunning(spell.effect))
	{
		spell.effect = k_NoEffectHandle;
	}
	_links.insert_or_assign(entity, std::move(link));
	_spells.push_front(entity);
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Magic: miracle {} cast at ({}, {})", static_cast<int>(type), point.x, point.z);
	// A miracle that is its particle effect is nothing if the effect can't start: the cast is refused
	if (magic::CastRefusedWithoutEffect(spell.hasParticleType, spell.effect != k_NoEffectHandle))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Magic: miracle {} refused, its effect couldn't start", static_cast<int>(type));
		Delete(entity);
		return entt::null;
	}
	if (!magic::spells::Start(*this, registry.Get<Spell>(entity)))
	{
		Delete(entity);
		return entt::null;
	}
	// One without an effect starts at its point by itself
	if (!registry.Get<Spell>(entity).hasParticleType)
	{
		OnSpellEvent(entity, {.type = particles::SpellEventInfo::Type::InitWithoutEffect, .position = point});
	}
	if (auto* started = FindSpell(entity);
	    started != nullptr && magic::GetMagicEffectInfo(Info(), type).createReactionOnCast != 0)
	{
		ReactToSpell(*started, true);
	}
	return FindSpell(entity) != nullptr ? entity : entt::null;
}

entt::entity MagicSystem::CastOnObject(MagicType type, PlayerNames player, entt::entity target,
                                       const magic::SpellCastData& cast, const particles::ProcessInfo& info)
{
	return CastOn(type, {.kind = SpellCaster::Kind::Player, .player = player, .entity = entt::null}, target, cast, info);
}

entt::entity MagicSystem::CastOn(MagicType type, const components::SpellCaster& caster, entt::entity target,
                                 const magic::SpellCastData& cast, const particles::ProcessInfo& info)
{
	const auto position = _world.PositionOf(target);
	if (!position.has_value() || !CanCastOn(type, target))
	{
		return entt::null;
	}
	const auto entity = Cast(type, caster, *position, cast, info);
	if (entity == entt::null)
	{
		return entt::null;
	}
	auto& spell = EntityRegistry().Get<Spell>(entity);
	spell.target = target;
	AddEffectTarget(spell, target);
	if (spell.spellClass == magic::SpellClass::Creature)
	{
		ReceiveCreatureSpell(target, entity, spell);
	}
	return entity;
}

// The creature spells' casting, receiving and turns are in MagicCreatureSpells.cpp

void MagicSystem::CloseDown(entt::entity entity)
{
	auto* spell = FindSpell(entity);
	if (spell == nullptr)
	{
		return;
	}
	if (!spell->closedDown)
	{
		StopHandGrain(*spell);
	}
	spell->closedDown = true;
	// The things that the miracle made and that go with it, its teleport stones, go at once
	if (Locator::teleportSystem::has_value())
	{
		Locator::teleportSystem::value().RemoveStonesOf(entity);
	}
	if (spell->effect != k_NoEffectHandle)
	{
		Locator::effectRequestSystem::value().Stop(spell->effect);
	}
}

bool MagicSystem::CanCastAt(MagicType type, PlayerNames player, glm::vec3 point)
{
	return Locator::infoConstants::has_value() && magic::spells::CanCastAt(*this, type, player, point, _ignoreInfluence);
}

bool MagicSystem::SpellEvent(entt::entity entity, const particles::SpellEventInfo& event)
{
	return OnSpellEvent(entity, event);
}

void MagicSystem::PayForSpell(entt::entity entity, float cost)
{
	if (auto* spell = FindSpell(entity); spell != nullptr && !spell->closedDown)
	{
		magic::PayFor(spell->chants, magic::spells::RulesOf(*this, *spell), CasterOf(*spell), cost);
	}
}

entt::entity MagicSystem::CreateTeleportStone(const Spell& spell)
{
	return Locator::teleportSystem::has_value() ? Locator::teleportSystem::value().CreateStone(
	                                                  spell.castPosition, spell.caster.player, EntityRegistry().ToEntity(spell))
	                                            : entt::null;
}

bool MagicSystem::CanPlaceTeleportStone(glm::vec3 point) const
{
	return !Locator::teleportSystem::has_value() || Locator::teleportSystem::value().CanPlaceStone(point);
}

bool MagicSystem::OnSpellEvent(entt::entity entity, const particles::SpellEventInfo& event)
{
	auto* spell = FindSpell(entity);
	return spell != nullptr && magic::spells::OnEvent(*this, *spell, event);
}

particles::ProcessInfo MagicSystem::TurnHandInfo(const particles::ProcessInfo& last, PlayerNames player) const
{
	// The turn reads the hand of the player as the reports give it, never the hand that this computer draws. Before the
	// first report the miracle keeps what it had.
	auto info = last;
	if (!Locator::handSystem::has_value())
	{
		return info;
	}
	const auto& hands = Locator::handSystem::value();
	if (const auto hand = ReportedHandOf(player); hand.has_value())
	{
		info.handPosition = *hand;
	}
	// The camera looks along the line from the camera to its focus, made a unit line
	if (const auto camera = hands.GetReportedCamera(player); camera.has_value())
	{
		const auto line = camera->focus - camera->origin;
		if (line != glm::vec3(0.0f))
		{
			info.cameraForward = line * (1.0f / glm::length(line));
		}
	}
	const auto hand = LeftHandOf(player);
	const auto* stillness = hand != entt::null ? EntityRegistry().TryGet<const HandStillness>(hand) : nullptr;
	info.direction = stillness != nullptr ? stillness->velocity : glm::vec3(0.0f);
	info.enabled = true;
	return info;
}

particles::ProcessInfo MagicSystem::HandInfo() const
{
	return {
	    .handPosition = _hand.handPosition,
	    .cameraForward = _hand.cameraForward,
	    .direction = _handVelocity,
	    .power = 1.0f,
	    .enabled = true,
	    .spin = _handSpin.spin,
	};
}

void MagicSystem::Maintain(entt::entity entity, Spell& spell)
{
	if (magic::AgeOneTurn(spell.age, spell.duration, k_TurnSeconds))
	{
		CloseDown(entity);
	}
	// Its caster gone, it closes down and is left as it is. From then on it has no creator.
	if (CasterOf(spell) == nullptr)
	{
		CloseDown(entity);
		spell.creator = entt::null;
		return;
	}
	if (spell.target != entt::null && !EntityRegistry().Valid(spell.target))
	{
		CloseDown(entity);
	}
	// A storm's swirl plays only while whoever cast it is still there
	if (spell.castEffect != k_NoEffectHandle && CasterOf(spell) == nullptr)
	{
		Locator::effectRequestSystem::value().Remove(spell.castEffect);
		spell.castEffect = k_NoEffectHandle;
	}
	spell.processInfo.enabled = true;
	// A creature's miracle flows from its hands as they move; a locked miracle follows the hand; anything else stays
	// where it was cast
	if (spell.caster.kind == SpellCaster::Kind::Creature)
	{
		UpdateCreatureCast(spell);
	}
	else if (spell.castFromHand && spell.seed != entt::null)
	{
		// A miracle held in the hand follows it, keeping the spin it was cast with: it is cast on the land under the hand
		// itself, which hangs a little short of where the cursor points
		const float power = spell.processInfo.power;
		const float spin = spell.processInfo.spin;
		spell.processInfo = TurnHandInfo(spell.processInfo, spell.caster.player);
		spell.processInfo.power = power;
		spell.processInfo.spin = spin;
		const auto& hand = spell.processInfo.handPosition;
		spell.castPosition = {hand.x, _world.LandHeight({hand.x, hand.z}), hand.z};
	}
	else if (magic::spells::FollowsHand(*this, spell))
	{
		// A flock keeps taking the hand's sweep while it makes its animals, keeping the spin it was cast with
		const float power = spell.processInfo.power;
		const float spin = spell.processInfo.spin;
		spell.processInfo = TurnHandInfo(spell.processInfo, spell.caster.player);
		spell.processInfo.power = power;
		spell.processInfo.spin = spin;
	}
	if (!spell.closedDown)
	{
		const float strength = magic::spells::StrengthOf(*this, spell);
		magic::PayForOneTurn(spell.chants, magic::spells::RulesOf(*this, spell), CasterOf(spell));
		spell.processInfo.power = strength;
	}
	else
	{
		spell.processInfo.enabled = false;
		spell.processInfo.power = 0.0f;
	}
}

bool MagicSystem::Process(entt::entity entity, Spell& spell)
{
	if (!spell.closedDown)
	{
		if (auto* caster = CasterOf(spell))
		{
			magic::Recharge(spell.chants, magic::spells::RulesOf(*this, spell), *caster);
		}
		if (!(spell.processInfo.power > 0.0f))
		{
			CloseDown(entity);
		}
	}
	if (spell.effect != k_NoEffectHandle &&
	    !Locator::effectRequestSystem::value().Step(spell.effect, spell.processInfo, k_TurnSeconds))
	{
		spell.effect = k_NoEffectHandle;
		// Its effect over, the people's reactions to it go, even while what it made keeps it
		if (Locator::reactionSystem::has_value())
		{
			Locator::reactionSystem::value().RemoveFrom(entity);
		}
		spell.reaction = 0;
	}
	// The effect's events may have closed it down or cast others: look it up again
	auto* after = FindSpell(entity);
	if (after == nullptr)
	{
		return false;
	}
	magic::spells::ProcessTurn(*this, *after);
	if (after->reaction != 0 && Locator::reactionSystem::has_value())
	{
		Locator::reactionSystem::value().Move(after->reaction, after->position, after->movement,
		                                      magic::spells::StrengthOf(*this, *after));
	}
	// It lives while its effect does, or until it closes down without one, or while what its kind made keeps it
	return magic::FateOf({
	           .hasParticleType = after->hasParticleType,
	           .effectRunning = after->effect != k_NoEffectHandle,
	           .closedDown = after->closedDown,
	           .keptByKind = magic::spells::KeptByKind(*this, *after),
	       }) == magic::SpellFate::Continue;
}

void MagicSystem::Delete(entt::entity entity)
{
	auto& registry = EntityRegistry();
	if (auto* spell = FindSpell(entity))
	{
		// A miracle that goes closes down first: its teleport stones go with it
		if (Locator::teleportSystem::has_value())
		{
			Locator::teleportSystem::value().RemoveStonesOf(entity);
		}
		if (spell->effect != k_NoEffectHandle)
		{
			Locator::effectRequestSystem::value().Remove(spell->effect);
		}
		if (spell->castEffect != k_NoEffectHandle)
		{
			Locator::effectRequestSystem::value().Remove(spell->castEffect);
		}
		if (spell->seed != entt::null && registry.Valid(spell->seed))
		{
			if (auto* seed = registry.TryGet<SpellSeed>(spell->seed); seed != nullptr && seed->spell == entity)
			{
				seed->spell = entt::null;
				// A seed linked to its miracle goes with it, out of the hand if it is held
				if (const auto holder = HolderOf(spell->seed); holder.has_value())
				{
					const ActingAs acting(*this, *holder);
					LetGoOfSeed(true);
				}
				else
				{
					registry.Destroy(spell->seed);
				}
			}
		}
		if (Locator::reactionSystem::has_value())
		{
			Locator::reactionSystem::value().RemoveFrom(entity);
		}
		registry.Destroy(entity);
	}
	_links.erase(entity);
	std::erase(_spells, entity);
}

void MagicSystem::HeldSeedTurn()
{
	auto& registry = EntityRegistry();
	if (!registry.Valid(*Held()))
	{
		SetHeld(std::nullopt);
		return;
	}
	const bool own = ActsOnOwnHand();
	auto& seed = registry.Get<SpellSeed>(*Held());
	// In its first turn in the hand, the seed makes the hand start the path of its gesture again
	if (own && seed.turnsInHand == 0 && Locator::gestureSystem::has_value())
	{
		Locator::gestureSystem::value().SeedFirstTurnInHand();
	}
	++seed.turnsInHand;
	const bool wasReady = seed.ready;
	seed.ready = seed.ready || magic::SeedReadyAfter(seed.turnsInHand, k_TurnSeconds, Info().spellSystem.delayBeforeSeedActive);
	// Becoming ready calls off a press made too early
	if (own && seed.ready && !wasReady)
	{
		Do(magic::Cancel(_input));
	}
	// The hand looks again at how it holds the seed, and the miracle shows in the hand once ready
	ShowHeldSeed();
	// A miracle locked on in the hand whose prayer power or time ran out is over, and its seed with it
	if (Held().has_value() && registry.Get<SpellSeed>(*Held()).spell != entt::null)
	{
		const auto spellEntity = registry.Get<SpellSeed>(*Held()).spell;
		const auto* spell = FindSpell(spellEntity);
		if (spellEntity != entt::null && (spell == nullptr || spell->closedDown))
		{
			LetGoOfSeed(true);
		}
	}
}

void MagicSystem::ProcessTurn()
{
	// The hum of an armed seed follows the hand one time each turn, before the audio turn
	if (_holdLoop != entt::null && Locator::sounds::has_value())
	{
		Locator::sounds::value().SetEmitterPosition(_holdLoop, _hand.handPosition);
	}
	if (!Locator::infoConstants::has_value() || !Locator::effectRequestSystem::has_value() ||
	    !Locator::effectRequestSystem::value().ShowsEffects())
	{
		return;
	}
	++_turn;
	_world.ProcessTurn();
	ProcessDispensers();
	ProcessCreatureSpells();
	KeepCreatureBeams();

	// The seed in the hand of each player waits until it is ready
	for (const auto player : ecs::game_players::Gods())
	{
		if (HeldOf(player).has_value())
		{
			const ActingAs acting(*this, player);
			HeldSeedTurn();
		}
	}

	for (const auto entity : std::vector<entt::entity>(_spells.begin(), _spells.end()))
	{
		if (auto* spell = FindSpell(entity))
		{
			Maintain(entity, *spell);
		}
	}
	for (const auto entity : std::vector<entt::entity>(_spells.begin(), _spells.end()))
	{
		// The seed it was cast from has its turn first
		if (const auto* before = FindSpell(entity))
		{
			KeepSeedInHand(*before);
		}
		if (const auto* before = FindSpell(entity))
		{
			KeepSeedInInfluence(entity, *before);
		}
		auto* spell = FindSpell(entity);
		if (spell == nullptr || !Process(entity, *spell))
		{
			Delete(entity);
		}
	}
	// A seed out of the hand shows over its miracle or not, as the miracles are after their turn
	ShowSeedsWithSpellsTurn();
	// The pour lifting and tipping the hand steps once the miracles have had their turn
	magic::StepPour(_pour, k_TurnSeconds);
}

void MagicSystem::Update(float /*seconds*/)
{
	auto& registry = EntityRegistry();
	TakeGestures();
	KeepCreatureSpellSounds();
	// A seed out of the hand stays with its miracle, shown over it or not
	ShowSeedsWithSpells();
	// The held seed itself is placed in the hand as the hand is posed, and its in-hand effect with it
	// (PlaceHandEffect)
	if (Held().has_value() && registry.Valid(*Held()))
	{
		// The miracle shown in the hand steps by the game time since its last step in whole milliseconds, at least one
		// even while the game is paused. It steps no more often than 60 times a second; between its steps it is drawn
		// where the hand moved it to (PlaceHandEffect).
		auto& seed = registry.Get<SpellSeed>(*Held());
		const auto step = Locator::time::has_value()
		                      ? particles::drawn_steps::StepTime(_handEffectPace, Locator::time::value().GetFrameRealTime(),
		                                                         Locator::time::value().GetFrameGameTime(), true)
		                      : std::optional(std::chrono::milliseconds {0});
		if (seed.handEffect != k_NoEffectHandle && Locator::effectRequestSystem::has_value() && step.has_value())
		{
			const auto milliseconds = std::max<int64_t>(step->count(), 1);
			const auto info = HandEffectInfo();
			auto& effects = Locator::effectRequestSystem::value();
			if (!effects.Step(seed.handEffect, info, static_cast<float>(milliseconds) * 0.001f))
			{
				seed.handEffect = k_NoEffectHandle;
			}
			_handAtStep = info.handPosition;
			effects.SetDrawOffset(seed.handEffect, glm::vec3(0.0f));
		}
	}
	// A miracle held from this computer's hand, such as the lightning or the rain cloud, keeps up with the hand between
	// turns. The draw shows its particles that follow the hand at the live hand, with no limit of height.
	if (Locator::effectRequestSystem::has_value())
	{
		for (const auto entity : _spells)
		{
			if (const auto* spell = FindSpell(entity); spell != nullptr && spell->castFromHand && spell->fromLocalHand &&
			                                           spell->caster.player == OwnPlayer() && spell->effect != k_NoEffectHandle)
			{
				Locator::effectRequestSystem::value().SetLiveHand(spell->effect, _hand.handPosition);
			}
		}
	}
}

void MagicSystem::UpdateDispenserFrame(const ObjectDrawListSystemInterface& drawList, std::chrono::milliseconds frameRealTime,
                                       std::chrono::milliseconds frameGameTime, bool landDrawn)
{
	const auto seconds = particles::drawn_steps::StepSeconds(_swirlPace, frameRealTime, frameGameTime, landDrawn);
	if (!Locator::particleRuleSystem::has_value())
	{
		return;
	}
	auto& particles = Locator::particleRuleSystem::value();
	const auto& registry = EntityRegistry();
	const auto stepSwirl = [&](entt::entity entity) {
		const auto* dispenser = registry.TryGet<const SpellDispenser>(entity);
		if (dispenser == nullptr || dispenser->effect == ParticleRuleSystemInterface::k_NoEffect)
		{
			return;
		}
		// The swirl shows and moves on only in the frames that draw its dispenser
		const bool drawn = drawList.Drawn(registry, entity);
		particles.SetShown(dispenser->effect, drawn);
		if (drawn && seconds.has_value())
		{
			particles.ProcessByFrame(dispenser->effect, *seconds);
		}
	};
	std::vector<entt::entity> dispensers;
	registry.Each<const SpellDispenser>(
	    [&dispensers](entt::entity entity, const SpellDispenser&) { dispensers.push_back(entity); });
	std::ranges::for_each(particles::drawn_steps::DrawOrder(
	                          drawList.GetList(), dispensers, drawList.GetView().has_value(),
	                          [&registry, &drawList](entt::entity entity) { return drawList.Decides(registry, entity); }),
	                      stepSwirl);
}

void MagicSystem::Reset()
{
	_swirlPace = {};
	_handEffectPace = {};
	_spells.clear();
	_links.clear();
	StopHoldLoop();
	// No hand holds a seed
	_acting.reset();
	if (Locator::entitiesRegistry::has_value())
	{
		EntityRegistry().Each<HandHeldSeed>([](HandHeldSeed& held) { held.seed = entt::null; });
	}
	_input = {};
	_actionDown = false;
	_fireBallPress.reset();
	_pour = {};
	_rainWatchers.clear();
	_circle.reset();
	_turn = 0;
	_driven.reset();
	_lastHandPosition.reset();
	_lastFrameHandPosition.reset();
	_handVelocity = glm::vec3(0.0f);
	_handEffectPoint.reset();
	_handScale = 1.0f;
	_lastHandResult = HandResult::None;
	_world.Reset();
	_creatureCasts.clear();
}

void MagicSystem::InterfaceTurn()
{
	if (!Held().has_value() || !EntityRegistry().Valid(*Held()) || !Locator::infoConstants::has_value())
	{
		return;
	}
	// A miracle held locked in the hand that the hand moved outside its cast rule is let go, as the button would
	LetGoIfOutsideCastRule();
	// While the button stays down a locked miracle is sent again where the hand is, at the turn that starts now; moved
	// somewhere it may not be cast it isn't sent, and stays locked until the button comes up
	if (_input.state == magic::CastInput::State::Locked)
	{
		const bool valid = _input.onObject ? HeldSeedTarget().has_value() : HandPointValid();
		if (const auto apply = magic::Tick(_input, _turn + 1, valid); apply.cast.has_value())
		{
			SendCast(*apply.cast);
		}
	}
}

bool MagicSystem::TapFireBall(entt::entity ball)
{
	if (!Locator::infoConstants::has_value() || Held().has_value() || !EntityRegistry().Valid(ball) ||
	    !EntityRegistry().AllOf<MagicFireBall>(ball) || !EmptyHandInInfluence())
	{
		return false;
	}
	SendFireBallCommand(ball, false);
	return true;
}

std::optional<entt::entity> MagicSystem::PickedUnderHand() const
{
	// What the cursor picked, as anything else under the hand is found
	if (!_hand.overWorld || !Locator::pickingSystem::has_value())
	{
		return std::nullopt;
	}
	const auto picked = Locator::pickingSystem::value().GetPick().object;
	if (!picked.has_value() || !EntityRegistry().Valid(*picked))
	{
		return std::nullopt;
	}
	return picked;
}

std::optional<entt::entity> MagicSystem::OrbUnderHand() const
{
	const auto picked = PickedUnderHand();
	return picked.has_value() && EntityRegistry().AllOf<OneOffSpellSeed>(*picked) ? picked : std::nullopt;
}

bool MagicSystem::EmptyHandInInfluence() const
{
	// The hand is this computer's player's
	return _hand.point.has_value() && _world.HandInInfluence(OwnPlayer(), *_hand.point);
}

bool MagicSystem::PressFireBall(uint32_t nowMs, uint32_t turn)
{
	_fireBallPress.reset();
	if (!Locator::infoConstants::has_value() || Held().has_value())
	{
		return false;
	}
	const auto picked = PickedUnderHand();
	const auto ball = picked.has_value() && EntityRegistry().AllOf<MagicFireBall>(*picked) ? picked : std::nullopt;
	// Out of the player's influence the hand neither taps nor takes it
	if (!ball.has_value() || !EmptyHandInInfluence())
	{
		return false;
	}
	// The player's own fireball cannot go into the hand, so the press taps it at once: the help counts the tap, and the
	// ball slips through the hand
	const auto& fireBall = EntityRegistry().Get<const MagicFireBall>(*ball);
	if (hand_grab::PressOnFireBall(fireBall.hasPlayer && fireBall.player == OwnPlayer()) == hand_grab::FireBallPress::TapAtOnce)
	{
		if (Locator::helpProfileSystem::has_value())
		{
			Locator::helpProfileSystem::value().Report(help::profile::k_TapThing);
		}
		return true;
	}
	// The press starts to take another's fireball: a quick release taps it, and the help counts that tap. Held as long
	// as a tap can last, the hand takes it, which catches it the same way, but the help counts no tap.
	_fireBallPress = FireBallPress {.ball = *ball, .pressMs = nowMs, .pressTurn = turn};
	return true;
}

void MagicSystem::HoldFireBall(uint32_t nowMs, uint32_t turn)
{
	if (!_fireBallPress.has_value() ||
	    !hand_grab::TakesFireBallAtHold(hand_grab::ElapsedMs(nowMs, _fireBallPress->pressMs, turn, _fireBallPress->pressTurn)))
	{
		return;
	}
	const auto ball = _fireBallPress->ball;
	_fireBallPress.reset();
	// The hand sends the take, which catches the ball at the turn
	if (!Held().has_value() && EntityRegistry().Valid(ball) && EntityRegistry().AllOf<MagicFireBall>(ball) &&
	    EmptyHandInInfluence())
	{
		SendFireBallCommand(ball, true);
	}
}

void MagicSystem::ReleaseFireBall(uint32_t nowMs, uint32_t turn)
{
	if (!_fireBallPress.has_value())
	{
		return;
	}
	const auto press = *_fireBallPress;
	_fireBallPress.reset();
	const bool quick = hand_grab::TapsFireBallAtRelease(hand_grab::ElapsedMs(nowMs, press.pressMs, turn, press.pressTurn));
	if (!quick || Held().has_value() || !EntityRegistry().Valid(press.ball) || !EmptyHandInInfluence())
	{
		return;
	}
	// The quick tap catches another player's fireball, and the help counts the tap first
	if (EntityRegistry().AllOf<MagicFireBall>(press.ball))
	{
		if (Locator::helpProfileSystem::has_value())
		{
			Locator::helpProfileSystem::value().Report(help::profile::k_TapThing);
		}
		// The hand sends the tap, which catches the ball at the turn
		SendFireBallCommand(press.ball, false);
	}
}

void MagicSystem::SendFireBallCommand(entt::entity ball, bool take)
{
	if (!Locator::playerCommandSystem::has_value())
	{
		DoCatchFireBall(OwnPlayer(), ball);
		return;
	}
	auto& commands = Locator::playerCommandSystem::value();
	if (take)
	{
		commands.Submit(player_commands::HandTake {.object = ball});
	}
	else
	{
		commands.Submit(player_commands::HandTap {.object = ball});
	}
}

void MagicSystem::DoCatchFireBall(PlayerNames player, entt::entity ball)
{
	// Every computer catches it into the hand of the player that sent the tap or the take, if the hand is still empty.
	// The press asked for the player's influence.
	const ActingAs acting(*this, player);
	if (!Locator::infoConstants::has_value() || Held().has_value() || !EntityRegistry().Valid(ball) ||
	    !EntityRegistry().AllOf<MagicFireBall>(ball))
	{
		return;
	}
	CatchFireBall(ball);
}

bool MagicSystem::CatchFireBall(entt::entity ball)
{
	auto& registry = EntityRegistry();
	const auto& fireBall = registry.Get<const MagicFireBall>(ball);
	// Only another's ball is caught: the player's own slips through the hand
	const auto catcher = ActingPlayer();
	if (fireBall.hasPlayer && fireBall.player == catcher)
	{
		return false;
	}
	// It becomes a fireball seed in the hand, charged in full and ready, and the ball goes
	if (GiveSeedToHand(catcher, SpellSeedType::Fire, magic::k_BasePowerUpLevel, 1.0f) == entt::null)
	{
		return false;
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Magic: the hand caught fireball #{}", entt::to_integral(ball));
	if (Locator::fireSystem::has_value())
	{
		Locator::fireSystem::value().Forget(ball);
	}
	registry.Destroy(ball);
	return true;
}

bool MagicSystem::AbsorbsFireBalls() const
{
	const auto& registry = EntityRegistry();
	if (!Held().has_value() || !registry.Valid(*Held()))
	{
		return false;
	}
	const auto& seed = registry.Get<const SpellSeed>(*Held());
	return seed.ready && seed.seedType == SpellSeedType::Fire;
}

bool MagicSystem::DoApplySeed(PlayerNames player, entt::entity target)
{
	// A miracle in the hand of the player takes the command, also when the thing is gone by the turn
	const ActingAs acting(*this, player);
	if (!Held().has_value())
	{
		return false;
	}
	if (EntityRegistry().Valid(target) && EntityRegistry().AllOf<MagicFireBall>(target))
	{
		AbsorbFireBall(target);
	}
	return true;
}

bool MagicSystem::AbsorbFireBall(entt::entity ball)
{
	auto& registry = EntityRegistry();
	if (!AbsorbsFireBalls())
	{
		return false;
	}
	auto& seed = registry.Get<SpellSeed>(*Held());
	// The catch uses the row of the fireball table that the ball uses
	const auto row = EntityRegistry().Get<const MagicFireBall>(ball).infoRow;
	seed.power *= 1.0f + Info().magicFireBall.at(row).catchIncreaseFactor;
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Magic: the fire seed took in fireball #{}, its power now {:.4f}",
	                    entt::to_integral(ball), seed.power);
	if (Locator::fireSystem::has_value())
	{
		Locator::fireSystem::value().Forget(ball);
	}
	registry.Destroy(ball);
	return true;
}

bool MagicSystem::PressAction()
{
	if (!_hand.overWorld || !Locator::infoConstants::has_value() || !Held().has_value())
	{
		return false;
	}
	_actionDown = true;
	// A ready fire seed put to a fireball, anyone's, takes it in: the seed grows stronger and stays in the hand. The hand
	// sends the use of the seed on the ball, which the turn does.
	if (const auto ball = PickedUnderHand();
	    ball.has_value() && EntityRegistry().AllOf<MagicFireBall>(*ball) && AbsorbsFireBalls())
	{
		if (Locator::playerCommandSystem::has_value())
		{
			Locator::playerCommandSystem::value().Submit(player_commands::HandApply {.target = *ball});
		}
		else
		{
			AbsorbFireBall(*ball);
		}
		return true;
	}
	const auto& seed = EntityRegistry().Get<const SpellSeed>(*Held());
	const auto& seedInfo = magic::GetSpellSeedInfo(Info(), seed.seedType);
	// Outside the player's influence a press does nothing at all
	const magic::PressContext context {
	    .inInfluence = IsHandInInfluence(),
	    .seedReady = seed.ready,
	    .onValidObject = HeldSeedTarget().has_value(),
	    .pointValid = HandPointValid() || seedInfo.sizingGesture == GestureType::Circle,
	    .turn = _turn,
	};
	Do(magic::Press(_input, {.castType = seedInfo.castType, .castOnObject = seedInfo.castOnObject != 0}, context));
	return true;
}

void MagicSystem::ReleaseAction()
{
	_actionDown = false;
	if (Held().has_value())
	{
		Do(magic::Release(_input));
	}
}

void MagicSystem::UnlockHeldMiracle()
{
	auto& registry = EntityRegistry();
	const bool own = ActsOnOwnHand();
	if (own)
	{
		magic::StopPour(_pour);
	}
	if (!Held().has_value() || !registry.Valid(*Held()))
	{
		return;
	}
	auto& seed = registry.Get<SpellSeed>(*Held());
	// The seed keeps what its miracle had left, also a miracle that closed down. A seed with no miracle keeps nothing
	// and always has enough.
	bool enough = true;
	if (auto* spell = FindSpell(seed.spell))
	{
		enough = StoreFromSpell(*Held(), seed, *spell);
	}
	else
	{
		ForgetStoredSpell(seed);
	}
	if (own)
	{
		_lastHandResult = HandResult::Released;
	}
	if (!magic::KeptAtUnlock(magic::GetSpellSeedInfo(Info(), seed.seedType).deleteSeedOnceCast != 0, enough))
	{
		LetGoOfSeed(true);
	}
}

bool MagicSystem::StoreFromSpell(entt::entity entity, SpellSeed& seed, Spell& spell)
{
	// The seed keeps what the miracle has left, and how long it has run, for another go. Its charge becomes what the
	// miracle has left.
	seed.storedChants = spell.chants.chants;
	seed.chantStore = spell.chants.chants;
	seed.storedAge = spell.age;
	seed.storedMaxObjects = spell.maxObjectsToCreate;
	const bool enough = magic::spells::HasEnoughForRecast(Info(), spell);
	// The seed lets go of the miracle. A miracle that still runs from this seed closes down.
	if (spell.seed == entity)
	{
		spell.seed = entt::null;
		CloseDown(seed.spell);
	}
	seed.spell = entt::null;
	// The store clears the mark of the cast's link, so the seed can cast again
	seed.linkedByCast = false;
	return enough;
}

void MagicSystem::ReleaseSpellLink(entt::entity entity, SpellSeed& seed)
{
	if (auto* spell = FindSpell(seed.spell); spell != nullptr && spell->seed == entity)
	{
		spell->seed = entt::null;
		CloseDown(seed.spell);
	}
	// The mark of the cast's link stays
	seed.spell = entt::null;
}

void MagicSystem::ForgetStoredSpell(SpellSeed& seed)
{
	// A seed with no miracle keeps no prayer power, age or number of objects of a miracle
	seed.storedChants = -1.0f;
	seed.storedAge = 0.0f;
	seed.storedMaxObjects = -1;
	seed.spell = entt::null;
}

void MagicSystem::DiscardHeldSeedOf(PlayerNames player)
{
	const ActingAs acting(*this, player);
	DiscardHeldSeed();
}

void MagicSystem::DiscardHeldSeed()
{
	if (!Held().has_value())
	{
		return;
	}
	// The hand of this computer calls off its press. A miracle locked on in the hand stores back what it has left, as the
	// let go of the button does.
	const bool own = ActsOnOwnHand();
	if (own && magic::Cancel(_input).stopHoldLoop)
	{
		StopHoldLoop();
	}
	if (HeldSpellRunsInHand())
	{
		UnlockHeldMiracle();
	}
	auto& registry = EntityRegistry();
	if (Held().has_value() && registry.Valid(*Held()))
	{
		// What the seed still holds goes back to the player's worship
		const auto& seed = registry.Get<const SpellSeed>(*Held());
		ReturnFromSeed(seed, magic::SeedRefund(seed.hasIcon, seed.chantStore));
	}
	// The hand shakes: its sound plays and a band flies off the hand
	if (own && Locator::miracleFxSystem::has_value())
	{
		Locator::miracleFxSystem::value().HandShaken();
	}
	LetGoOfSeed(true);
	if (own)
	{
		_lastHandResult = HandResult::Discarded;
	}
}

void MagicSystem::DeleteSeed(entt::entity entity)
{
	auto& registry = EntityRegistry();
	if (!registry.Valid(entity) || !registry.AllOf<SpellSeed>(entity))
	{
		return;
	}
	auto& seed = registry.Get<SpellSeed>(entity);
	// The miracle it cast that still runs from it closes down; one that has moved on from it carries on
	if (auto* spell = FindSpell(seed.spell); spell != nullptr && spell->seed == entity)
	{
		spell->seed = entt::null;
		CloseDown(seed.spell);
	}
	seed.spell = entt::null;
	// A seed in the hand of a player leaves that hand
	if (const auto holder = HolderOf(entity); holder.has_value())
	{
		const ActingAs acting(*this, *holder);
		LetGoOfSeed(true);
		return;
	}
	registry.Destroy(entity);
}

entt::entity MagicSystem::IconOfHeldSeed() const
{
	const auto& registry = EntityRegistry();
	if (!Held().has_value() || !registry.Valid(*Held()) || !Locator::spellIconSystem::has_value())
	{
		return entt::null;
	}
	return Locator::spellIconSystem::value().IconOfSeed(*Held());
}

void MagicSystem::AskPowerUpAtIcon(entt::entity icon, int level)
{
	if (!Locator::spellIconSystem::has_value())
	{
		return;
	}
	// The icon of the command charges its seed to the level from the prayer power of its site, in the turns of the site.
	// The seed keeps its level until the icon has charged it. The icon starts the charge only while a hand of the player
	// holds its seed.
	Locator::spellIconSystem::value().StartPowerUpCharge(icon, ActingPlayer(), level);
}

void MagicSystem::ShowHeldSeedPowerUp(int level, int previous)
{
	auto& seed = EntityRegistry().Get<SpellSeed>(*Held());
	// The hand of another player shows no effect on this computer
	if (!ActsOnOwnHand())
	{
		ShowHeldSeed();
		return;
	}
	// Its in-hand effect starts afresh at the new level, shown once the seed is ready
	StopHandEffect(seed);
	StartHandEffect(*Held());
	ShowHeldSeed();
	// The hand shows the name of the miracle at the new level, then a bracelet more, bands flying on and the announcer
	AnnounceHeldMiracle(seed);
	if (Locator::miracleFxSystem::has_value())
	{
		Locator::miracleFxSystem::value().SeedInHand(level, previous);
	}
}

void MagicSystem::AnnounceHeldMiracle(const SpellSeed& seed)
{
	_heldMiracleToolTip = magic::GetHeldToolTipText(Info(), magic::GetSpellSeedInfo(Info(), seed.seedType), seed.powerUp);
}

std::optional<uint32_t> MagicSystem::TakeHeldMiracleToolTip()
{
	return std::exchange(_heldMiracleToolTip, std::nullopt);
}

void MagicSystem::SetIconSeedPowerUp(entt::entity entity, int powerUp)
{
	auto& registry = EntityRegistry();
	if (!registry.Valid(entity) || !registry.AllOf<SpellSeed>(entity))
	{
		return;
	}
	auto& seed = registry.Get<SpellSeed>(entity);
	const int previous = seed.powerUp;
	seed.powerUp = powerUp;
	const auto type = magic::GetMagicTypeFromPowerUpLevel(magic::GetSpellSeedInfo(Info(), seed.seedType), powerUp);
	const float cost = magic::GetChantsRequiredToCreate(Info(), type);
	// What the seed holds past the cost of the new level goes back to the battery of its worship site
	const float spare = -magic::ChantNeeded(cost, seed.chantStore);
	if (spare > 0.0f && registry.Valid(seed.worshipSite) && Locator::worshipSiteSystem::has_value())
	{
		Locator::worshipSiteSystem::value().ReturnChants(seed.worshipSite, spare);
		seed.chantStore = seed.chantStore - spare;
	}
	seed.power = magic::SeedPower(seed.chantStore, cost);
	if (const auto holder = HolderOf(entity); holder.has_value())
	{
		const ActingAs acting(*this, *holder);
		ShowHeldSeedPowerUp(powerUp, previous);
		if (ActsOnOwnHand())
		{
			_lastHandResult = HandResult::PoweredUp;
		}
	}
}

void MagicSystem::TakeGestures()
{
	if (!Locator::gestureEvents::has_value())
	{
		return;
	}
	for (const auto& event : Locator::gestureEvents::value().TakeEvents())
	{
		switch (event.kind)
		{
		case GestureEvent::Kind::Circle:
		{
			auto centre = event.centre;
			centre.y = _world.LandHeight({centre.x, centre.z});
			_circle = Circle {.centre = centre, .radius = event.radius, .turn = _turn};
			break;
		}
		// The gestures on the seed in the hand go as commands of the player, which the turn does
		case GestureEvent::Kind::PowerUp:
			SendSeedGesture({.kind = player_commands::SeedGesture::Kind::PowerUp,
			                 .level = static_cast<int8_t>(event.powerUpLevel),
			                 .icon = IconOfHeldSeed()});
			break;
		case GestureEvent::Kind::Scribble:
			SendSeedGesture({.kind = player_commands::SeedGesture::Kind::Scribble, .level = 0});
			break;
		case GestureEvent::Kind::Shake:
			SendSeedGesture({.kind = player_commands::SeedGesture::Kind::Shake, .level = 0});
			break;
		}
	}
}

void MagicSystem::SendSeedGesture(const player_commands::SeedGesture& gesture)
{
	if (Locator::playerCommandSystem::has_value())
	{
		Locator::playerCommandSystem::value().Submit(gesture);
		return;
	}
	DoSeedGesture(OwnPlayer(), gesture);
}

void MagicSystem::DoSeedGesture(PlayerNames player, const player_commands::SeedGesture& gesture)
{
	// The gesture acts on the miracle in the hand of the player that sent it
	const ActingAs acting(*this, player);
	switch (gesture.kind)
	{
	case player_commands::SeedGesture::Kind::PowerUp:
		// The scribble that calls off the power-up asked for asks for the plain miracle. The hand shakes as it does when
		// the scribble drops the miracle.
		if (gesture.level < 0 && ActsOnOwnHand() && Locator::miracleFxSystem::has_value())
		{
			Locator::miracleFxSystem::value().HandShaken();
		}
		AskPowerUpAtIcon(gesture.icon, gesture.level);
		break;
	case player_commands::SeedGesture::Kind::Scribble:
		// The hand asked for no power-up, so the scribble drops the seed
		DiscardHeldSeed();
		break;
	case player_commands::SeedGesture::Kind::Shake:
		DiscardHeldSeed();
		break;
	}
}

PrayerPower* MagicSystem::PrayerOf(PlayerNames player) const
{
	auto& registry = EntityRegistry();
	PrayerPower* found = nullptr;
	registry.Each<const Player, PrayerPower>([&](entt::entity, const Player& component, PrayerPower& prayer) {
		if (component.name == player)
		{
			found = &prayer;
		}
	});
	return found;
}

float MagicSystem::DrawForSeed(const SpellSeed& seed, float amount)
{
	if (seed.worshipSite != entt::null && EntityRegistry().Valid(seed.worshipSite) && Locator::worshipSiteSystem::has_value())
	{
		return Locator::worshipSiteSystem::value().UseCreateChants(seed.worshipSite, amount);
	}
	auto* store = PrayerOf(seed.player);
	return store != nullptr ? magic::ChargeSeed(*store, amount) : 0.0f;
}

void MagicSystem::ReturnFromSeed(const SpellSeed& seed, float amount)
{
	if (seed.worshipSite != entt::null && EntityRegistry().Valid(seed.worshipSite) && Locator::worshipSiteSystem::has_value())
	{
		if (amount > 0.0f)
		{
			Locator::worshipSiteSystem::value().ReturnChants(seed.worshipSite, amount);
		}
		return;
	}
	if (auto* store = PrayerOf(seed.player))
	{
		magic::ReturnPrayer(*store, amount);
	}
}

float WorshipSiteSpellCaster::MaintainSpell(float amount)
{
	return Locator::worshipSiteSystem::has_value() ? Locator::worshipSiteSystem::value().MaintainSpell(_site, amount) : 0.0f;
}

void WorshipSiteSpellCaster::OnChantsSpent(float chants, magic::ChantCharge charge)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!Locator::worshipSiteSystem::has_value() || _spell == entt::null || !registry.Valid(_spell) ||
	    !registry.AllOf<Spell>(_spell))
	{
		return;
	}
	auto& spell = registry.Get<Spell>(_spell);
	// The motes fly from the icon that made the seed; without it there are none
	const auto icon = spell.caster.icon;
	if (icon == entt::null || !registry.Valid(icon) || !registry.AllOf<Transform>(icon))
	{
		return;
	}
	const bool upkeep = charge == magic::ChantCharge::PerTurn;
	float& spent = upkeep ? spell.upkeepMotes : spell.eventMotes;
	spent = chants + spent;
	if (!(magic::k_ChantsPerMote < spent))
	{
		return;
	}
	spent = 0.0f;
	// Both ends stand at their places on the map, as high above the land as they are
	const auto onMap = [](glm::vec3 point) {
		if (!Locator::terrainSystem::has_value())
		{
			return point;
		}
		const auto& land = Locator::terrainSystem::value();
		return map_coords::ToWorld(land, map_coords::HeldPlace(land, point));
	};
	const auto to = upkeep ? spell.castPosition : spell.position;
	Locator::worshipSiteSystem::value().AddMote(_site, onMap(registry.Get<const Transform>(icon).position), onMap(to));
}

MagicSystemInterface::HandCastState MagicSystem::GetHandCastState() const
{
	HandCastState state {.state = _input.state,
	                     .holding = _actionDown,
	                     .velocity = _handVelocity,
	                     .throwSpeed = particles::maths::HandThrow(_handVelocity).speed,
	                     .pour = magic::PourPoseAt(_pour, 1.0f),
	                     .pointValid = HandPointValid()};
	if (Held().has_value() && EntityRegistry().Valid(*Held()) && Locator::infoConstants::has_value())
	{
		const auto& seed = EntityRegistry().Get<const SpellSeed>(*Held());
		state.ready = seed.ready;
		state.readyIn =
		    seed.ready
		        ? 0.0f
		        : std::max(Info().spellSystem.delayBeforeSeedActive - static_cast<float>(seed.turnsInHand) * k_TurnSeconds,
		                   0.0f);
		state.origin = seed.origin;
		state.powerUp = seed.powerUp;
		state.chantStore = seed.chantStore;
		state.storedChants = seed.storedChants;
	}
	if (_circle.has_value())
	{
		state.circleCentre = _circle->centre;
		state.circleRadius = _circle->radius;
		state.circleSecondsLeft =
		    std::max(static_cast<float>(magic::k_CircleTurns) - static_cast<float>(_turn - _circle->turn), 0.0f) *
		    k_TurnSeconds;
	}
	return state;
}

void MagicSystem::Empathise(MagicType type, PlayerNames player, glm::vec3 point)
{
	auto* mind = ecs::creature_sight::MindSeeing(player, point);
	if (mind == nullptr)
	{
		return;
	}
	const auto& effect = magic::GetMagicEffectInfo(Info(), type);
	for (const auto desire : effect.perceivedPlayerDesire)
	{
		creature_perceived_desires::Increase(mind->perceivedDesires, static_cast<size_t>(desire), 1.0f);
	}
	creature_perceived_desires::IncreaseTown(mind->perceivedDesires, static_cast<size_t>(effect.townDesireBeingHelped), 1.0f);
}

void MagicSystem::RecordCast(MagicType type, PlayerNames player, glm::vec3 point)
{
	EntityRegistry().Each<components::Player>([&](entt::entity, components::Player& record) {
		if (record.name == player)
		{
			record.lastCast = components::Player::Cast {.position = point, .type = type, .turn = _turn};
		}
	});
	player_statistics::CountCast(EntityRegistry(), player, type);
}

void MagicSystem::ReactToSpell(Spell& spell, bool onCast)
{
	const auto& effect = magic::GetMagicEffectInfo(Info(), spell.magicType);
	if (effect.reactionType == Reaction::None || !Locator::reactionSystem::has_value())
	{
		return;
	}
	// A reaction that has ended lets the miracle's next event make another
	if (spell.reaction != 0 && !Locator::reactionSystem::value().Find(spell.reaction).has_value())
	{
		spell.reaction = 0;
	}
	if (spell.reaction != 0)
	{
		return;
	}
	const auto found = std::ranges::find_if(
	    _spells, [&spell](entt::entity e) { return EntityRegistry().Valid(e) && EntityRegistry().TryGet<Spell>(e) == &spell; });
	if (found == _spells.end())
	{
		return;
	}
	const auto entity = *found;
	spell.reaction = Locator::reactionSystem::value().Create({
	    .initiator = entity,
	    .type = effect.reactionType,
	    .player = spell.caster.player,
	    .position = spell.position,
	    .impressiveValue = effect.impressiveValue,
	    .power = 1.0f,
	    .strength = magic::spells::StrengthOf(*this, spell),
	    .magicType = spell.magicType,
	    .casterCreature = spell.caster.kind == SpellCaster::Kind::Creature ? spell.caster.entity : entt::null,
	    .onCast = onCast,
	    .balance = ecs::systems::ReactionSystemInterface::Balance::Miracles,
	});
}

// What the miracles' objects in the world do for them

void MagicSystem::ShieldStruck(const Spell& shield, bool destroyed)
{
	if (Locator::magicShieldSystem::has_value())
	{
		Locator::magicShieldSystem::value().ShieldStruck(EntityRegistry().ToEntity(shield), destroyed);
	}
}

bool MagicSystem::HasWorldObjects(const Spell& spell) const
{
	const auto entity = EntityRegistry().ToEntity(spell);
	if (spell.spellClass == magic::SpellClass::Shield)
	{
		return Locator::magicShieldSystem::has_value() && Locator::magicShieldSystem::value().HasObject(entity);
	}
	if (spell.spellClass == magic::SpellClass::Forest)
	{
		return Locator::forestSystem::has_value() && Locator::forestSystem::value().HasTrees(entity);
	}
	return false;
}

bool MagicSystem::PlantForest(Spell& spell)
{
	return Locator::forestSystem::has_value() &&
	       Locator::forestSystem::value().Plant(EntityRegistry().ToEntity(spell), spell, TribalPower(spell)) > 0;
}

bool MagicSystem::ForestCanGrowAt(glm::vec3 point) const
{
	return !Locator::forestSystem::has_value() || Locator::forestSystem::value().CanGrowAt(point);
}

bool MagicSystem::SendSpellEvent(entt::entity spell, const particles::SpellEventInfo& event)
{
	return OnSpellEvent(spell, event);
}

float MagicSystem::ForcePayForSpell(entt::entity entity, float cost)
{
	auto* spell = FindSpell(entity);
	if (spell == nullptr)
	{
		return 0.0f;
	}
	return magic::PayFor(spell->chants, magic::spells::RulesOf(*this, *spell), CasterOf(*spell), cost, magic::Refill::Whole);
}

void MagicSystem::SetSpellRadius(entt::entity entity, float radius)
{
	auto* spell = Locator::entitiesRegistry::value().TryGet<Spell>(entity);
	if (spell == nullptr)
	{
		return;
	}
	spell->magnitude = radius;
	if (Locator::effectRequestSystem::has_value() && Locator::effectRequestSystem::value().IsRunning(spell->effect))
	{
		Locator::effectRequestSystem::value().SetMagnitude(spell->effect, radius);
	}
	if (Locator::magicShieldSystem::has_value())
	{
		Locator::magicShieldSystem::value().FollowSpellRadius(entity, radius);
	}
}

float MagicSystem::SpellStrength(entt::entity entity)
{
	const auto* spell = FindSpell(entity);
	return spell != nullptr ? magic::spells::StrengthOf(*this, *spell) : 0.0f;
}

// For the debug window

void MagicSystem::StartCastEffect(Spell& spell, ParticleType type, bool synced)
{
	// It plays out by itself, as big as the miracle and heading the way it was cast
	spell.castEffect = Locator::effectRequestSystem::value().Start({
	    .type = type,
	    .position = spell.castPosition,
	    .magnitude = spell.magnitude,
	    .synced = synced,
	    .player = static_cast<int>(spell.caster.player),
	    .direction = spell.direction,
	    .processInfo = spell.processInfo,
	});
}

void MagicSystem::RainOnFire(const glm::vec3& point)
{
	if (!Locator::reactionSystem::has_value())
	{
		return;
	}
	auto& reactions = Locator::reactionSystem::value();
	auto& registry = EntityRegistry();
	bool done = false;
	registry.Each<Spell>([&](entt::entity entity, Spell& spell) {
		if (done || spell.spellClass != magic::SpellClass::StormAndTornado)
		{
			return;
		}
		auto& watching = _rainWatchers[entity];
		if (watching != 0 && reactions.IsActive(watching))
		{
			return;
		}
		if (glm::distance(glm::vec2(point.x, point.z), glm::vec2(spell.position.x, spell.position.z)) < spell.magnitude)
		{
			watching = reactions.Create({.initiator = entity,
			                             .type = Reaction::ReactToMagicWaterPuttingOutFire,
			                             .player = spell.caster.player,
			                             .position = spell.position,
			                             .onCast = true});
			done = true;
		}
	});
}

std::optional<entt::entity> MagicSystem::SpellAt(MagicType type, glm::vec3 point, float radius) const
{
	// The miracles are kept newest first
	for (const auto entity : _spells)
	{
		const auto* spell = EntityRegistry().TryGet<const Spell>(entity);
		if (spell != nullptr && spell->magicType == type && gutils::GetDistanceInMetres(spell->position, point) < radius)
		{
			return entity;
		}
	}
	return std::nullopt;
}

std::vector<MagicSystemInterface::SpellInfo> MagicSystem::GetSpells() const
{
	std::vector<SpellInfo> result;
	if (!Locator::infoConstants::has_value())
	{
		return result;
	}
	auto& self = const_cast<MagicSystem&>(*this);
	for (const auto entity : _spells)
	{
		const auto* spell = EntityRegistry().TryGet<const Spell>(entity);
		if (spell == nullptr)
		{
			continue;
		}
		result.push_back({
		    .entity = entity,
		    .magicType = spell->magicType,
		    .player = spell->caster.player,
		    .age = spell->age,
		    .duration = spell->duration,
		    .chants = spell->chants.chants,
		    .initialChants = spell->chants.initialChants,
		    .strength = magic::spells::StrengthOf(self, *spell),
		    .upkeep = magic::spells::CostToMaintain(Info(), *spell),
		    .closing = spell->closedDown,
		    .fromHand = spell->castFromHand,
		    .effect = spell->effect,
		    .position = spell->position,
		});
	}
	return result;
}

std::vector<MagicSystemInterface::DispenserInfo> MagicSystem::GetDispensers() const
{
	std::vector<DispenserInfo> result;
	EntityRegistry().Each<const SpellDispenser, const Transform>(
	    [&](entt::entity entity, const SpellDispenser& dispenser, const Transform& transform) {
		    result.push_back({
		        .entity = entity,
		        .magicType = dispenser.magicType,
		        .position = transform.position,
		        .hasOrb = dispenser.orb != entt::null,
		        .tick = dispenser.timer.tick,
		        .period = dispenser.timer.period,
		        .active = dispenser.timer.active,
		    });
	    });
	return result;
}

// The running state in a saved game (deviation 83)

void MagicSystem::SaveState(reflection::BinaryWriter& out) const
{
	// The miracles go in the order that they are processed. Each one makes its link to its effect again at the load.
	reflection::Save(out, std::vector<entt::entity>(_spells.begin(), _spells.end()));
	reflection::Save(out, _creatureCasts);
	reflection::Save(out, _creatureBeams);
	reflection::Save(out, _world.PuttingOutFire());
	// The movement of this computer's hand and the place of its effect: only the frames change them, and only the hand's
	// own effect and the casts that the hand sends read them
	out.BeginPresentation();
	reflection::Save(out, _handVelocity);
	reflection::Save(out, _lastHandPosition);
	out.WriteRaw(_handSpin);
	reflection::Save(out, _spinningSeed);
	reflection::Save(out, _handEffectPoint);
	reflection::Save(out, _handScale);
	reflection::Save(out, _handAtStep);
	out.EndPresentation();
	// The hands keep their seeds (HandHeldSeed). An older save kept the seed of the first player here.
	reflection::Save(out, std::optional<entt::entity> {});
	// The press of this computer's hand, its sound and its pour: the hand sends its casts as commands, which the turn does
	out.BeginPresentation();
	out.WriteRaw(_input);
	reflection::Save(out, _actionDown);
	out.WriteRaw(_fireBallPress);
	reflection::Save(out, _holdLoop != entt::null);
	out.WriteRaw(_pour);
	out.EndPresentation();
	reflection::Save(out, _rainWatchers);
	// The last circle that this computer's hand drew. The cast sends its place in the command.
	out.BeginPresentation();
	out.WriteRaw(_circle);
	out.EndPresentation();
	reflection::Save(out, _turn);
	// Only the tools read the result of the last action of the hand
	reflection::SavePresentation(out, _lastHandResult);
	reflection::Save(out, _ignoreInfluence);
}

bool MagicSystem::LoadState(reflection::BinaryReader& in)
{
	return LoadBody(in, false);
}

bool MagicSystem::LoadOldState(reflection::BinaryReader& in)
{
	return LoadBody(in, true);
}

bool MagicSystem::LoadBody(reflection::BinaryReader& in, bool hasSpellGrid)
{
	std::vector<entt::entity> spells;
	reflection::Load(in, spells);
	reflection::Load(in, _creatureCasts);
	reflection::Load(in, _creatureBeams);
	std::unordered_map<entt::entity, uint32_t> watchers;
	reflection::Load(in, watchers);
	_world.RestorePuttingOutFire(std::move(watchers));
	if (hasSpellGrid)
	{
		// An older save kept a grid of where miracles ran lately. Nothing reads it, so the load reads past it.
		std::array<std::array<uint8_t, k_OldSpellGridSide>, k_OldSpellGridSide> grid {};
		reflection::Load(in, grid);
	}
	reflection::Load(in, _handVelocity);
	reflection::Load(in, _lastHandPosition);
	in.ReadRaw(_handSpin);
	reflection::Load(in, _spinningSeed);
	reflection::Load(in, _handEffectPoint);
	reflection::Load(in, _handScale);
	reflection::Load(in, _handAtStep);
	std::optional<entt::entity> olderHeld;
	reflection::Load(in, olderHeld);
	in.ReadRaw(_input);
	reflection::Load(in, _actionDown);
	in.ReadRaw(_fireBallPress);
	bool holdLoop = false;
	reflection::Load(in, holdLoop);
	in.ReadRaw(_pour);
	reflection::Load(in, _rainWatchers);
	in.ReadRaw(_circle);
	reflection::Load(in, _turn);
	reflection::Load(in, _lastHandResult);
	reflection::Load(in, _ignoreInfluence);
	if (in.Failed())
	{
		return false;
	}
	// A save older than the seeds of the hands kept the seed of the first player
	if (olderHeld.has_value() && !HeldOf(PlayerNames::PLAYER_ONE).has_value())
	{
		const ActingAs acting(*this, PlayerNames::PLAYER_ONE);
		SetHeld(olderHeld);
	}
	// Each miracle links to its effect again, and the effect finds its miracle with no new event
	_spells.clear();
	_links.clear();
	if (spells.empty() && !Held().has_value())
	{
		if (holdLoop)
		{
			StartHoldLoop();
		}
		return true;
	}
	auto& registry = EntityRegistry();
	auto* effects = Locator::effectRequestSystem::has_value() ? &Locator::effectRequestSystem::value() : nullptr;
	for (const auto entity : spells)
	{
		const auto* spell = registry.Valid(entity) ? registry.TryGet<const Spell>(entity) : nullptr;
		if (spell == nullptr)
		{
			continue;
		}
		auto link = std::make_unique<SpellLink>(*this, entity);
		if (effects != nullptr && spell->effect != k_NoEffectHandle)
		{
			effects->RestoreSink(spell->effect, link.get());
		}
		_links.insert_or_assign(entity, std::move(link));
		_spells.push_back(entity);
	}
	if (Held().has_value())
	{
		const auto* seed = registry.Valid(*Held()) ? registry.TryGet<const SpellSeed>(*Held()) : nullptr;
		if (seed == nullptr)
		{
			SetHeld(std::nullopt);
		}
		else if (effects != nullptr && seed->handEffect != k_NoEffectHandle)
		{
			effects->RestoreSink(seed->handEffect, _handEffectLink.get());
		}
	}
	// The hum of an armed seed plays again
	if (holdLoop)
	{
		StartHoldLoop();
	}
	return true;
}
