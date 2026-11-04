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

#include <algorithm>
#include <chrono>
#include <optional>
#include <utility>

#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <spdlog/spdlog.h>

#include "Common/GameRandom.h"
#include "ECS/Components/HandHeldSeed.h"
#include "ECS/Components/HandStillness.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/GamePlayers.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/SpellCreator.h"
#include "ECS/Statistics.h"
#include "ECS/Systems/EffectRequestSystemInterface.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/MiracleFxSystemInterface.h"
#include "ECS/Systems/ObjectDrawListSystemInterface.h"
#include "ECS/Systems/ParticleRuleSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/TeleportSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/ThingMapPlace.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Magic/SpellLifetime.h"
#include "Magic/SpellSeedRules.h"
#include "MagicSpellLinks.h"
#include "Particles/DrawnEffectSteps.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleShields.h"
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
