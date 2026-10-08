/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Spell.h"

#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <memory>
#include <string>

#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Debug/DebugEnv.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Transform.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/HandGrain.h"
#include "ECS/Systems/SpellSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Magic/Objects/MapShield.h"
#include "Magic/Spells/SpellClasses.h"
#include "Particles/PSysManager.h"
#include "Particles/ParticleTypes.h"
#include "Players.h"
#include "SpellCreator.h"
#include "SpellGrid.h"
#include "SpellSeed.h"
#include "Worship/Worship.h"

using namespace openblack;
using namespace openblack::magic;
using namespace openblack::ecs::components;

namespace
{
/// The spell as the PSys manager sees it (the owner a spell's PSys is created with)
class Sink final: public psys::SpellSink
{
public:
	explicit Sink(entt::entity spell)
	    : _spell(spell)
	{
	}
	int SpellEvent(const psys::SpellEventInfo& event) override
	{
		auto& registry = Locator::entitiesRegistry::value();
		if (!registry.Valid(_spell))
		{
			return 0;
		}
		return OpsOf(registry.Get<Spell>(_spell).spellClass).spellEvent(_spell, event);
	}
	[[nodiscard]] int PowerUpLevel() const override
	{
		auto& registry = Locator::entitiesRegistry::value();
		if (!registry.Valid(_spell) || !Locator::infoConstants::has_value())
		{
			return -1;
		}
		return GetPowerUpLevelOfMagicInfo(Locator::infoConstants::value(), registry.Get<Spell>(_spell).magicType);
	}
	[[nodiscard]] bool IsMyInterfaceCasting() const override
	{
		auto& registry = Locator::entitiesRegistry::value();
		return registry.Valid(_spell) && registry.Get<Spell>(_spell).isMyInterfaceCasting;
	}
	[[nodiscard]] bool IsHumanPlayerCasting() const override
	{
		auto& registry = Locator::entitiesRegistry::value();
		return registry.Valid(_spell) && registry.Get<Spell>(_spell).isHumanPlayerCasting;
	}
	[[nodiscard]] bool IsScriptCasting() const override
	{
		// the creator is the scripts' player (inferred: the neutral player, whose spells the scripts cast)
		auto& registry = Locator::entitiesRegistry::value();
		if (!registry.Valid(_spell))
		{
			return false;
		}
		const auto& creator = registry.Get<Spell>(_spell).creator;
		return creator.kind == ecs::components::SpellCreator::Kind::Player && creator.player == PlayerNames::NEUTRAL;
	}
	[[nodiscard]] bool Player(int& player) const override
	{
		auto& registry = Locator::entitiesRegistry::value();
		if (!registry.Valid(_spell) || !registry.Get<Spell>(_spell).hasPlayer)
		{
			return false;
		}
		player = static_cast<int>(registry.Get<Spell>(_spell).player);
		return true;
	}
	[[nodiscard]] entt::entity SpellEntity() const override
	{
		return Locator::entitiesRegistry::value().Valid(_spell) ? _spell : entt::null;
	}

private:
	entt::entity _spell;
};

/// The live spells, their sinks and the spell classes' operations (Locator::spellSystem)
ecs::systems::SpellSystemInterface& SpellState()
{
	if (!Locator::spellSystem::has_value())
	{
		std::fputs("magic: no spell system in the locator (Locator::spellSystem)\n", stderr);
		std::abort();
	}
	return Locator::spellSystem::value();
}

Spell& SpellOf(entt::entity spell)
{
	return Locator::entitiesRegistry::value().Get<Spell>(spell);
}

float LandAt(const glm::vec3& position)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(position.x, position.z))
	                                           : 0.0f;
}

void EnsureOps()
{
	if (SpellState().TakeOpsRegistration())
	{
		RegisterSpellClasses();
	}
}

/// A spell's upkeep turn: age, duration, the creator's update and the chants paid (always succeeds)
void ProcessMaintainRequest(entt::entity entity)
{
	auto& spell = SpellOf(entity);
	spell.age += static_cast<float>(k_TurnMs) * 0.001f;
	if (spell.duration >= 0.0f && spell.age > spell.duration)
	{
		CloseDown(entity);
	}
	if (spell.creator.kind == SpellCreator::Kind::None || !creator::IsFunctional(spell.creator))
	{
		CloseDown(entity);
		spell.creator = {};
		return;
	}
	spell.processInfo.enabled = true;
	creator::UpdateSpellInfo(spell.creator, entity, spell.processInfo);
	if (IsCastFromHand(entity))
	{
		// castPos follows the hand (truncated to MapCoords, altitude 0). The interface's "can't cast here" feedback for
		// a human caster comes with the hand casting (pending).
		spell.castPos = glm::vec3(map_coords::Quantise(spell.processInfo.handPos.x), 0.0f,
		                          map_coords::Quantise(spell.processInfo.handPos.z));
	}
	if (!spell.closedDown)
	{
		const float strength = GetSpellStrength(entity);
		chants::PayForOneTurn(spell, ChantContextOf(entity));
		spell_grid::Mark(spell.position, 0xFF);
		spell.processInfo.power = strength;
	}
	else
	{
		spell.processInfo.enabled = false;
		spell.processInfo.power = 0.0f;
	}
}

/// The seed follows the spell (seed::ProcessFromSpell); returns 1
int ProcessSpellSeed(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& spell = SpellOf(entity);
	if (spell.seed != entt::null && registry.Valid(spell.seed) && registry.AllOf<SpellSeed>(spell.seed))
	{
		seed::ProcessFromSpell(spell.seed);
		return 1;
	}
	spell.seed = entt::null;
	return 1;
}

void Trace(entt::entity entity, const char* what)
{
	if (!TraceEnabled())
	{
		return;
	}
	const auto& spell = SpellOf(entity);
	const auto context = ChantContextOf(entity);
	const auto* effect = psys::manager::Find(spell.psys);
	SPDLOG_LOGGER_INFO(spdlog::get("game"),
	                   "Spell trace: turn {} spell {} {} ({}) {}: chants {:.2f} safety {:.2f} strength {:.3f} upkeep {:.2f} "
	                   "age {:.1f}/{:.1f} closed {} psys {} atoms {}",
	                   game_clock::Turn(), static_cast<uint32_t>(entity), EffectInfoOf(entity).debugString.data(),
	                   static_cast<int>(spell.magicType), what, spell.chants, chants::GetChantSafetyLevel(spell, context),
	                   chants::GetSpellStrength(spell, context), context.costToMaintain, spell.age, spell.duration,
	                   spell.closedDown, spell.psys, effect != nullptr ? effect->AtomCount() : 0);
}
} // namespace

bool magic::TraceEnabled()
{
	static const bool trace = debug_env::SpellTrace();
	return trace;
}

glm::vec3 magic::ToWorld(const glm::vec3& mapPosition)
{
	// x, z are the MapCoords' (already whole 16.16 units in metres: not truncated again, a second
	// round trip can lose a unit), y = GetAltitude + the altitude
	return {mapPosition.x, LandAt(mapPosition) + mapPosition.y, mapPosition.z};
}

glm::vec3 magic::ToMap(const glm::vec3& worldPoint)
{
	// x, z truncated to 16.16 (x * 6553.6f), altitude = y - the land's height there
	const auto coords = map_coords::FromWorld(worldPoint);
	return {map_coords::ToMetres(coords.x), coords.altitude, map_coords::ToMetres(coords.z)};
}

void magic::RegisterSpellClasses()
{
	RegisterGeneralSpell();
	RegisterHealSpell();
	RegisterResourceSpell();
	RegisterForestSpell();
	RegisterTeleportSpell();
	RegisterShieldSpell();
	RegisterFlockSpells();
	RegisterWaterSpell();
	RegisterStormSpell();
	RegisterCreatureSpell();
}

const SpellOps& magic::OpsOf(SpellClass spellClass)
{
	EnsureOps();
	const auto& ops = SpellState().Ops(spellClass);
	if (ops.initWithPos != nullptr)
	{
		return ops;
	}
	// (inferred: placeholder) a class nobody registered (every class is registered now) runs as a plain Spell
	if (auto logger = spdlog::get("game"); logger != nullptr && SpellState().TakeNotPortedWarning(spellClass))
	{
		SPDLOG_LOGGER_WARN(logger, "Spell: class {} not ported, run as a plain Spell", static_cast<int>(spellClass));
	}
	return SpellState().Ops(SpellClass::General);
}

void magic::RegisterOps(SpellClass spellClass, const SpellOps& ops)
{
	SpellState().Ops(spellClass) = ops;
}

SpellClass magic::ClassOf(MagicType type)
{
	// the info.dat section of the magic type is its GMagicInfo class
	using S = MagicInfoSection;
	switch (SlotOf(type).section)
	{
	case S::General:
		return SpellClass::General;
	case S::Heal:
		return SpellClass::Heal;
	case S::Teleport:
		return SpellClass::Teleport;
	case S::Forest:
		return SpellClass::Forest;
	case S::Food:
	case S::Wood:
		return SpellClass::Resource;
	case S::StormAndTornado:
		return SpellClass::StormAndTornado;
	case S::Shield:
		return SpellClass::Shield;
	case S::Water:
		return SpellClass::Water;
	case S::FlockFlying:
		return SpellClass::FlockFlying;
	case S::FlockGround:
		return SpellClass::FlockGround;
	case S::CreatureSpell:
		return SpellClass::Creature;
	case S::_COUNT:
		break;
	}
	return SpellClass::General; // (inferred: unreachable guard for _COUNT)
}

const GMagicInfo& magic::MagicInfoOf(entt::entity spell)
{
	return GetMagicInfo(Locator::infoConstants::value(), SpellOf(spell).magicType);
}

const GMagicEffectInfo& magic::EffectInfoOf(entt::entity spell)
{
	return GetMagicEffectInfo(Locator::infoConstants::value(), SpellOf(spell).magicType);
}

bool magic::IsCastFromHand(entt::entity spell)
{
	const auto& component = SpellOf(spell);
	auto& registry = Locator::entitiesRegistry::value();
	if (component.seed == entt::null || !registry.Valid(component.seed))
	{
		return false;
	}
	const auto* seedComponent = registry.TryGet<const SpellSeed>(component.seed);
	return seedComponent != nullptr && seed::InfoOf(*seedComponent).castType == SpellCastType::SpellCastInHand;
}

float magic::GetTribalPower(entt::entity spell)
{
	const auto& component = SpellOf(spell);
	return players::TribalPower(EffectInfoOf(spell), component.hasPlayer ? &component.player : nullptr);
}

chants::Context magic::ChantContextOf(entt::entity spell)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& component = SpellOf(spell);
	chants::Context context {
	    .effect = &EffectInfoOf(spell),
	    .maintained = IsMaintainedSpell(component.magicType),
	    .recharged = MagicInfoOf(spell).isSpellRecharged != 0,
	    .hasCreator = component.creator.kind != SpellCreator::Kind::None,
	    .tribalPower = GetTribalPower(spell),
	};
	if (component.seed != entt::null && registry.Valid(component.seed))
	{
		if (const auto* seedComponent = registry.TryGet<const SpellSeed>(component.seed); seedComponent != nullptr)
		{
			context.seedPower = seedComponent->psysPower;
		}
	}
	context.costToMaintain = OpsOf(component.spellClass).costToMaintain(spell);
	context.turnMs = game_clock::MsPerTurn();
	const auto creator = component.creator;
	context.maintain = [creator, spell](float amount) { return creator::MaintainSpell(creator, spell, amount); };
	// TODO: the mana path sprites of a spell with a worship site
	return context;
}

float magic::GetSpellStrength(entt::entity spell)
{
	return chants::GetSpellStrength(SpellOf(spell), ChantContextOf(spell));
}

// ---- the base Spell ----

int base::InitWithPos(entt::entity entity, const glm::vec3& position, SpellCastData* castData, const psys::ProcessInfo& info)
{
	auto& spell = SpellOf(entity);
	// the creature empathises with the player's desires (perceivedPlayerDesire, townDesireBeingHelped).
	// TODO: the creature's mind.
	spell.originalCastPos = position;
	if (spell.hasPlayer)
	{
		// the player's statistics: one more spell of the type; the modulo is an openblack guard (inferred: the original
		// indexes by the type directly)
		auto& counts = players::MagicOf(spell.player).castCount;
		++counts[static_cast<size_t>(spell.magicType) % counts.size()];
	}
	chants::SetChants(spell, castData->chants);
	spell.maxObjectsToCreate = castData->maxObjectsToCreate;
	spell.duration = castData->duration;
	spell.position = position;
	spell.castPos = position;
	const glm::vec3 point = ToWorld(position);
	spell.processInfo = info;
	spell.direction = info.direction;
	// the default magnitude 40; castData is dereferenced above without a test, as in the original
	spell.magnitude = castData != nullptr ? castData->magnitude : 40.0f;
	const auto particleType = OpsOf(spell.spellClass).particleType(entity);
	const auto file = psys::ParticleTypeFile(particleType);
	if (!file.empty())
	{
		auto sink = std::make_unique<Sink>(entity);
		// NET_GAME_TYPE 1: the spell's own effect draws on the synced seed
		spell.psys = psys::manager::StartForSpell(std::string(file), point, spell.direction, spell.magnitude, sink.get(),
		                                          game_random::psys::NetGameType::Synced);
		SpellState().SetSink(entity, std::move(sink));
	}
	// the player's last cast: its position, magic type and game turn
	auto& last = players::MagicOf(spell.player).lastCast;
	last = {spell.castPos, spell.magicType, game_clock::Turn()};
	if (spell.psys != 0)
	{
		if (auto* effect = psys::manager::Find(spell.psys); effect != nullptr)
		{
			effect->SetPlayer(static_cast<int>(spell.player));
		}
	}
	else if (MagicInfoOf(entity).particleType != ParticleType::None)
	{
		// no failure path: without a PSys the spell goes on (the base Process then returns 5). Here it happens when
		// openblack has no .psys for the type.
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Spell: {} has no PSys for particle type {} ({}): cast without one",
		                   EffectInfoOf(entity).debugString.data(), static_cast<int>(particleType), file);
	}
	else
	{
		const psys::SpellEventInfo event {.type = psys::SpellEventInfo::Type::InitWithoutPSys, .position = point};
		OpsOf(spell.spellClass).spellEvent(entity, event);
	}
	const auto& effect = EffectInfoOf(entity);
	if (effect.createReactionOnCast != 0 && spell.reaction == 0 && effect.reactionType != Reaction::None)
	{
		spell.reaction = ecs::effects::reactions::CreateReaction(entity, effect.reactionType, spell.player, true);
	}
	// TODO(interface): the minimap blip (only in some game modes)
	return 1;
}

int base::InitWithObject(entt::entity spell, entt::entity object, SpellCastData* castData, const psys::ProcessInfo& info)
{
	// InitWithPos(creator, the object's position...) and, when it worked, psys->AddTarget(object)
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const ecs::components::Transform>(object);
	const glm::vec3 position = transform != nullptr ? ToMap(transform->position) : glm::vec3(0.0f);
	const int result = OpsOf(SpellOf(spell).spellClass).initWithPos(spell, position, castData, info);
	if (result == 1)
	{
		if (auto* effect = psys::manager::Find(SpellOf(spell).psys); effect != nullptr)
		{
			effect->AddTarget(object);
		}
	}
	return result;
}

float base::CoreProcess(entt::entity entity)
{
	auto& spell = SpellOf(entity);
	if (!spell.closedDown)
	{
		chants::Recharge(spell, ChantContextOf(entity));
		if (spell.processInfo.power <= 0.0f)
		{
			magic::CloseDown(entity);
		}
	}
	if (spell.psys != 0 &&
	    !psys::manager::ProcessForSpell(spell.psys, spell.processInfo, static_cast<float>(k_TurnMs) * 0.001f))
	{
		// the PSys returned 5: its reactions go and the PSys is deleted
		if (spell.reaction != 0)
		{
			ecs::effects::reactions::RemoveAllReactionsInitiatedByObject(entity);
		}
		spell.psys = 0;
	}
	return spell.processInfo.power;
}

int base::Process(entt::entity spell)
{
	CoreProcess(spell);
	return SpellOf(spell).psys != 0 ? 1 : 5;
}

float base::CalculateCostToMaintain(entt::entity spell)
{
	return EffectInfoOf(spell).costPerGameTurn;
}

void base::CloseDown(entt::entity entity)
{
	// the first close-down of a spell of this computer's interface stops the hand's grain raise
	auto& spell = SpellOf(entity);
	if (!spell.closedDown && spell.isMyInterfaceCasting)
	{
		ecs::systems::hand_grain::Stop();
	}
	spell.closedDown = true;
	if (spell.psys != 0)
	{
		psys::manager::CloseDown(spell.psys);
	}
}

void base::ToBeDeleted(entt::entity entity)
{
	// (the list and the entity are the caller's)
	auto& registry = Locator::entitiesRegistry::value();
	auto& spell = SpellOf(entity);
	if (spell.psys != 0)
	{
		psys::manager::Delete(spell.psys);
		spell.psys = 0;
	}
	SpellState().EraseSink(entity);
	if (spell.reaction != 0)
	{
		ecs::effects::reactions::RemoveAllReactionsInitiatedByObject(entity);
	}
	// the seed goes too, then the link is cleared
	if (spell.seed != entt::null && registry.Valid(spell.seed) && registry.AllOf<SpellSeed>(spell.seed))
	{
		const auto seedEntity = spell.seed;
		spell.seed = entt::null;
		seed::ToBeDeleted(seedEntity);
	}
	spell.seed = entt::null;
	magic::CloseDown(entity);
	// TODO(interface): the minimap blip
}

bool base::HasEnoughChantsAndLifeForRecast(entt::entity /*spell*/)
{
	return true; // the base Spell always has enough
}

ParticleType base::GetParticleType(entt::entity spell)
{
	return MagicInfoOf(spell).particleType;
}

// ---- lifecycle ----

entt::entity magic::AllocSpell(MagicType type, const SpellCreator& creator)
{
	EnsureOps();
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	auto& spell = registry.Assign<Spell>(entity);
	spell.magicType = type;
	spell.spellClass = ClassOf(type);
	spell.creator = creator;
	if (creator.kind != SpellCreator::Kind::None)
	{
		spell.player = creator.player; // creator->GetPlayer(), or the neutral player
		spell.hasPlayer = true;
		spell.isCreatureCasting = creator::IsCreature(creator);
		spell.isHumanPlayerCasting = creator::IsHumanPlayerCasting(creator);
	}
	// pushed at the front
	auto& spells = SpellState().Spells();
	spells.insert(spells.begin(), entity);
	return entity;
}

int magic::CastAtPos(MagicType type, SpellCreator creator, const glm::vec3& position, entt::entity* out,
                     SpellCastData* castData, const psys::ProcessInfo& info)
{
	if (creator.kind == SpellCreator::Kind::None)
	{
		creator = creator::NeutralPlayer();
	}
	const auto spell = AllocSpell(type, creator);
	const int result = OpsOf(SpellOf(spell).spellClass).initWithPos(spell, position, castData, info);
	if (result != 1)
	{
		DeleteSpell(spell);
		*out = entt::null;
		return result;
	}
	*out = spell;
	Trace(spell, "cast");
	return 1;
}

int magic::CastAtObject(MagicType type, SpellCreator creator, entt::entity object, entt::entity* out, SpellCastData* castData,
                        const psys::ProcessInfo& info)
{
	const auto& tables = Locator::infoConstants::value();
	const auto seedType = GetFirstSpellSeedForMagicType(tables, type);
	const bool onObject = seedType != SpellSeedType::None && GetSpellSeedInfo(tables, seedType).castOnObject != 0;
	auto& registry = Locator::entitiesRegistry::value();
	if (!onObject)
	{
		const auto* transform = registry.TryGet<const ecs::components::Transform>(object);
		const glm::vec3 position = transform != nullptr ? ToMap(transform->position) : glm::vec3(0.0f);
		return CastAtPos(type, creator, position, out, castData, info);
	}
	if (creator.kind == SpellCreator::Kind::None)
	{
		creator = creator::NeutralPlayer();
	}
	const auto spell = AllocSpell(type, creator);
	const int result = OpsOf(SpellOf(spell).spellClass).initWithObject(spell, object, castData, info);
	if (result != 1)
	{
		DeleteSpell(spell);
		*out = entt::null;
		return result;
	}
	*out = spell;
	Trace(spell, "cast on object");
	return 1;
}

void magic::CloseDown(entt::entity spell)
{
	OpsOf(SpellOf(spell).spellClass).closeDown(spell);
}

void magic::DeleteSpell(entt::entity spell)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(spell) || !registry.AllOf<Spell>(spell))
	{
		return;
	}
	Trace(spell, "deleted");
	std::erase(SpellState().Spells(), spell);
	const auto& ops = OpsOf(SpellOf(spell).spellClass);
	if (ops.toBeDeleted != nullptr)
	{
		ops.toBeDeleted(spell);
	}
	base::ToBeDeleted(spell);
	registry.Destroy(spell);
}

void magic::ProcessSpells([[maybe_unused]] unsigned int turn)
{
	auto& registry = Locator::entitiesRegistry::value();
	spell_grid::Decay();
	map_shield::ProcessShields(); // the MapShields' turn (Magic/Objects/MapShield)
	// the SpellSeedGraphic list (every turn, a second pass every 30) and the players' spell icons
	worship::ProcessSpellIcons(); // Worship/Worship.cpp
	// every spell's upkeep first, then every spell's turn
	for (const auto spell : std::vector<entt::entity>(SpellState().Spells()))
	{
		if (registry.Valid(spell))
		{
			ProcessMaintainRequest(spell);
		}
	}
	for (const auto spell : std::vector<entt::entity>(SpellState().Spells()))
	{
		if (!registry.Valid(spell) || !registry.AllOf<Spell>(spell))
		{
			continue;
		}
		Trace(spell, "turn");
		if (ProcessSpellSeed(spell) == 5 || OpsOf(SpellOf(spell).spellClass).process(spell) == 5)
		{
			DeleteSpell(spell);
		}
	}
}

unsigned int magic::CurrentTurn()
{
	return game_clock::Turn();
}

const std::vector<entt::entity>& magic::Spells()
{
	return SpellState().Spells();
}

void magic::ClearSpells()
{
	// the entities go with the registry's reset; the PSys with psys::manager::Clear
	SpellState().Clear();
}
