/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The game world as the miracle rules see it: the land, the objects on it, and what an effect does to each
// object.

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <numbers>
#include <optional>

#include <LNDFile.h>
#include <glm/gtx/euler_angles.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "ECS/BuildingSites.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/BuildingDamage.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/GamePhysicsHooksWorld.h"
#include "ECS/Map.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/Registry.h"
#include "ECS/Statistics.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Systems/GameEndSystemInterface.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/ObjectDrawListSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/WaterRingSystemInterface.h"
#include "ECS/TempleHeartPassOn.h"
#include "ECS/ThingMapPlace.h"
#include "ECS/WorldObjects.h"
#include "ECS/WorshipSites.h"
#include "Help/VillageVoices.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Magic/MiracleDeeds.h"
#include "Magic/WaterRules.h"
#include "MagicLiving.h"
#include "MagicSystem.h"
#include "ObjectMeasures.h"
#include "Resources/ResourcesInterface.h"
#include "Sound/GameSoundEffects.h"
#include "Sound/SoundId.h"
#include "Sound/SoundRequestInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
constexpr float k_TwoPi = 2.0f * std::numbers::pi_v<float>;

// The game's world
/// The colours of the rings the water's drops leave on the land, picked at random
constexpr std::array<uint32_t, 5> k_RippleColours = {0xFF80CBC5u, 0xFF8599C5u, 0xFFBA97B2u, 0xFFB9CA86u, 0xFFBD9C8Au};
/// A tree rustles as the water grows it, one of these at random
constexpr std::array<audio::SoundId, 9> k_TreeGrowSounds {
    audio::SoundId::G_TreeGrow_01_1, audio::SoundId::G_TreeGrow_02_1, audio::SoundId::G_TreeGrow_03_1,
    audio::SoundId::G_TreeGrow_04_1, audio::SoundId::G_TreeGrow_01_2, audio::SoundId::G_TreeGrow_02_2,
    audio::SoundId::G_TreeGrow_03_2, audio::SoundId::G_TreeGrow_04_2, audio::SoundId::G_TreeGrow_01_3};

ecs::Registry& EntityRegistry()
{
	return Locator::entitiesRegistry::value();
}

const GObjectInfo* InfoOfLiving(entt::entity entity)
{
	return magic_living::InfoOf(entity);
}

/// A living thing's life, 0 to 1, none for anything else
std::optional<float> LifeOf(entt::entity entity)
{
	return magic_living::LifeOf(entity);
}

void SetLife(entt::entity entity, float life)
{
	magic_living::SetLife(entity, life);
}

/// Calls the function with every living thing (creatures, villagers and animals) and where it stands
template <typename F>
void EachLiving(F&& function)
{
	const auto& registry = EntityRegistry();
	registry.Each<const Creature, const Transform>(
	    [&](entt::entity entity, const Creature&, const Transform& transform) { function(entity, transform.position); });
	registry.Each<const Villager, const Transform>(
	    [&](entt::entity entity, const Villager&, const Transform& transform) { function(entity, transform.position); });
	registry.Each<const Animal, const Transform>(
	    [&](entt::entity entity, const Animal&, const Transform& transform) { function(entity, transform.position); });
}

/// The alignment kind of what isn't living, by what it is
AlignmentType AlignmentKindOf(entt::entity entity)
{
	const auto& registry = EntityRegistry();
	if (registry.AnyOf<Tree>(entity))
	{
		return AlignmentType::Plant;
	}
	if (registry.AnyOf<Abode>(entity))
	{
		return AlignmentType::Building;
	}
	if (registry.AnyOf<Feature>(entity))
	{
		return AlignmentType::Feature;
	}
	return AlignmentType::MobileObject;
}

/// How an object stands for an effect: where, how big, its life and its defence
std::optional<magic::EffectReceiver> ReceiverOf(entt::entity entity)
{
	const auto& registry = EntityRegistry();
	const auto* transform = registry.TryGet<const Transform>(entity);
	if (transform == nullptr)
	{
		return std::nullopt;
	}
	// Each kind of thing takes an effect by the reach of its fire: from its fire middle, by its fire radius across the land
	// and by its height up and down
	magic::EffectReceiver receiver {.entity = entity,
	                                .position = transform->position,
	                                .middle = object_measures::FireMiddle(registry, entity),
	                                .radius = object_measures::FireRadius(registry, entity),
	                                .height = object_measures::Height(registry, entity)};
	if (const auto* info = InfoOfLiving(entity))
	{
		receiver.life = LifeOf(entity);
		receiver.defence = magic_living::DefenceOf(entity);
		receiver.alignmentType = info->alignmentType;
		receiver.crushable = true;
		receiver.creature = registry.AllOf<Creature>(entity);
	}
	else if (registry.AllOf<Temple>(entity))
	{
		// A temple takes an effect as its heart: by the heart's own defences
		const auto& heart = Locator::infoConstants::value().citadelHeart;
		receiver.life = ecs::world_objects::LifeOf(entity);
		receiver.defence = magic::EffectDefence::From(heart);
		receiver.alignmentType = heart.alignmentType;
		receiver.crushable = ecs::world_objects::CanBeCrushed(entity);
	}
	else if (const auto* objectInfo = ecs::world_objects::EffectInfoOf(entity))
	{
		// Every object loses life to a crush or a hit by its own defence, but a field: crushing or hitting it takes nothing
		if (!registry.AnyOf<Field>(entity))
		{
			receiver.life = ecs::world_objects::LifeOf(entity);
		}
		receiver.defence = magic::EffectDefence::From(*objectInfo);
		receiver.alignmentType = objectInfo->alignmentType;
		receiver.crushable = ecs::world_objects::CanBeCrushed(entity);
	}
	else
	{
		receiver.alignmentType = AlignmentKindOf(entity);
		receiver.crushable = ecs::world_objects::CanBeCrushed(entity);
	}
	return receiver;
}

/// The player whose object was harmed remembers the harm against the player whose miracle did it
void AddDamageFrom(PlayerNames owner, PlayerNames from, float damage)
{
	const auto index = static_cast<size_t>(from);
	EntityRegistry().Each<Player>([&](entt::entity, Player& player) {
		if (player.name == owner && index < player.damageFrom.size())
		{
			player.damageFrom.at(index) += damage;
		}
	});
}

/// What a miracle does to an object it takes the last of the life from: a creature that can die faints and one that
/// can't has all its life back, anything else is destroyed as its kind is
void DestroyedByEffect(entt::entity entity, const ecs::world_objects::EffectDeath& death)
{
	auto& registry = EntityRegistry();
	if (const auto* creature = registry.TryGet<const Creature>(entity))
	{
		if (!creature->canDie)
		{
			SetLife(entity, 1.0f);
		}
		else if (Locator::creatureFightSystem::has_value())
		{
			Locator::creatureFightSystem::value().ForceFaint(entity);
		}
		return;
	}
	ecs::world_objects::DestroyedByEffect(entity, death);
}

/// The creature that applied an effect, when a creature did: the creature that cast the miracle, or the creature that
/// applied it when no miracle did. None when something other than a creature applied it.
entt::entity ActingCreature(const magic::EffectSource& source)
{
	const auto& registry = EntityRegistry();
	const auto applier = magic::ApplierOf(source);
	return applier != entt::null && registry.Valid(applier) && registry.AllOf<Creature>(applier) ? applier : entt::null;
}

/// The alignment of whoever an effect comes from, which damps how far it moves
float AlignmentOf(const magic::EffectSource& source)
{
	const auto& registry = EntityRegistry();
	if (const auto creature = ActingCreature(source); creature != entt::null)
	{
		return registry.Get<const Creature>(creature).alignment;
	}
	return Locator::alignmentSystem::has_value() ? Locator::alignmentSystem::value().GetPlayerAlignment(source.player) : 0.0f;
}
} // namespace

// The game's world

float GameMagicWorld::LandHeight(glm::vec2 xz) const
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(xz) : 0.0f;
}

bool GameMagicWorld::InBounds(glm::vec3 point) const
{
	if (!Locator::terrainSystem::has_value())
	{
		return false;
	}
	const auto extent = Locator::terrainSystem::value().GetExtent();
	return point.x >= extent.minimum.x && point.z >= extent.minimum.y && point.x < extent.maximum.x &&
	       point.z < extent.maximum.y;
}

bool GameMagicWorld::IsDryLand(glm::vec3 point) const
{
	if (!InBounds(point) || point.x < 0.0f || point.z < 0.0f)
	{
		return false;
	}
	// Land is dry where it stands above the sea's level, by its cell's altitude
	constexpr float k_CellSize = 10.0f;
	constexpr uint8_t k_LowestDryAltitude = 4;
	const auto cell = glm::u16vec2(glm::floor(glm::vec2(point.x, point.z) / k_CellSize));
	const auto* landCell = Locator::terrainSystem::value().FindCell(cell);
	return landCell != nullptr && landCell->altitude >= k_LowestDryAltitude;
}

bool GameMagicWorld::IsLand(glm::vec3 point) const
{
	if (!InBounds(point) || point.x < 0.0f || point.z < 0.0f)
	{
		return false;
	}
	// Land is a cell without water in it
	constexpr float k_CellSize = 10.0f;
	const auto cell = glm::u16vec2(glm::floor(glm::vec2(point.x, point.z) / k_CellSize));
	const auto* landCell = Locator::terrainSystem::value().FindCell(cell);
	return landCell != nullptr && landCell->properties.hasWater == 0;
}

bool GameMagicWorld::InInfluence(PlayerNames player, glm::vec3 point) const
{
	return Locator::influenceSystem::has_value() && Locator::influenceSystem::value().PlayerInfluence(player, point) > 0.0f;
}

bool GameMagicWorld::HandInInfluence(PlayerNames player, glm::vec3 hand) const
{
	return Locator::influenceSystem::has_value() && Locator::influenceSystem::value().IsHandInInfluence(player, hand);
}

void GameMagicWorld::HeldThingUsedOnLand(PlayerNames player)
{
	if (Locator::influenceSystem::has_value())
	{
		Locator::influenceSystem::value().HeldThingUsedOnLand(player);
	}
}

std::optional<glm::vec3> GameMagicWorld::PositionOf(entt::entity object) const
{
	const auto& registry = EntityRegistry();
	if (!registry.Valid(object))
	{
		return std::nullopt;
	}
	const auto* transform = registry.TryGet<const Transform>(object);
	return transform != nullptr ? std::optional(transform->position) : std::nullopt;
}

bool GameMagicWorld::ApplyEffect(entt::entity object, const magic::EffectValues& values, const magic::EffectSource& source)
{
	if (!EntityRegistry().Valid(object))
	{
		return false;
	}
	const auto receiver = ReceiverOf(object);
	// An empty pot, or a field without food under an effect that burns, does not take the effect
	if (!receiver.has_value() || !ecs::world_objects::TakesEffect(object, values[magic::EffectKind::Burn]))
	{
		return false;
	}
	// A creature takes some effects its own way, such as a heal in a fight. A creature that blocks makes the effect less.
	auto taken = values;
	if (magic_living::TakesEffectItsOwnWay(object, taken, source))
	{
		return true;
	}
	const auto& info = Locator::infoConstants::value();
	const std::array outcomes {magic::ApplyEffectTo(taken, *receiver, info.alignment,
	                                                info.player.applyEffectAlignmentChangeAddition, AlignmentOf(source))};
	Apply(taken, outcomes, source);
	return receiver->life.has_value();
}

std::vector<entt::entity> GameMagicWorld::ApplyEffectAt(glm::vec3 point, const magic::EffectValues& values,
                                                        const magic::EffectSource& source)
{
	// Everything in the cells round the point that the effect reaches takes it, one after another. A temple's heart or a
	// worship site gives it to what its heart chooses, which takes it as it is when its turn comes.
	const auto receivers = ReceiversIn(magic::EffectCellsAround(point, values.radius));
	const auto& info = Locator::infoConstants::value();
	const float casterAlignment = AlignmentOf(source);
	const bool breaksBuildings = values.IsDestructive();
	ecs::GamePhysicsHooksWorld hooksWorld;
	std::vector<entt::entity> reached;
	// All the things that the effect reaches share one effect. A creature that blocks in a fight makes it a tenth, also
	// for the things that take it after the creature.
	auto shared = values;
	for (const auto& receiver : receivers)
	{
		// An empty pot, or a field without food under an effect that burns, does not take the effect
		if (!ecs::world_objects::TakesEffect(receiver.entity, shared[magic::EffectKind::Burn]) ||
		    !magic::Reaches(shared, point, receiver))
		{
			continue;
		}
		const auto taker = ecs::temple_heart_pass_on::TakerOf(hooksWorld, receiver.entity, breaksBuildings);
		// A creature takes some effects its own way, such as a heal in a fight
		if (!EntityRegistry().Valid(taker) || magic_living::TakesEffectItsOwnWay(taker, shared, source))
		{
			continue;
		}
		const auto takerReceiver = ReceiverOf(taker);
		if (!takerReceiver.has_value())
		{
			continue;
		}
		const std::array outcomes {magic::ApplyEffectTo(shared, *takerReceiver, info.alignment,
		                                                info.player.applyEffectAlignmentChangeAddition, casterAlignment)};
		Apply(shared, outcomes, source);
		reached.push_back(taker);
	}
	return reached;
}

std::vector<magic::EffectReceiver> GameMagicWorld::ReceiversIn(const magic::CellRange& cells)
{
	auto& registry = EntityRegistry();
	const auto& map = Locator::entitiesMap::value();
	std::vector<magic::EffectReceiver> receivers;
	// x by x, and in each x z by z; in each cell everything as the cell keeps it, a building once for every cell it
	// covers
	for (int x = cells.first.x; x <= cells.last.x; ++x)
	{
		for (int z = cells.first.y; z <= cells.last.y; ++z)
		{
			for (const auto entity : map.GetAllInCell({x, z}))
			{
				if (!registry.Valid(entity) ||
				    !registry.AnyOf<Creature, Villager, Animal, Tree, Abode, Feature, Pot, Field, DeadTree, MobileStatic,
				                    MobileObject, AnimatedStatic, BigForest, Flowers, SpellDispenser, WorshipSite, Temple,
				                    StreetLantern, FishFarm, Ball>(entity))
				{
					continue;
				}
				if (auto receiver = ReceiverOf(entity))
				{
					receivers.push_back(*receiver);
				}
			}
		}
	}
	return receivers;
}

/// What harmed a town, for the cry of its people: the thing that applied the harm, or else the miracle
static guidance::Attacker CryAttacker(const ecs::Registry& registry, const magic::EffectSource& source)
{
	if (source.appliedBy.has_value())
	{
		const auto thing = *source.appliedBy;
		if (thing == entt::null || !registry.Valid(thing))
		{
			return guidance::Attacker::Other;
		}
		if (registry.AllOf<Creature>(thing))
		{
			return guidance::Attacker::Creature;
		}
		if (ecs::object_physics::IsRock(thing) || registry.AnyOf<DeadTree, BuildingPiece>(thing))
		{
			return guidance::Attacker::Rock;
		}
		return guidance::Attacker::Other;
	}
	if (source.magicType.has_value() && Locator::infoConstants::has_value() &&
	    magic::GetMagicInfo(Locator::infoConstants::value(), *source.magicType).spellSeedType == SpellSeedType::LightningBolt)
	{
		return guidance::Attacker::Lightning;
	}
	return guidance::Attacker::Other;
}

void GameMagicWorld::Apply(const magic::EffectValues& values, std::span<const magic::EffectOutcome> outcomes,
                           const magic::EffectSource& source)
{
	auto& registry = EntityRegistry();
	const float burn = values[magic::EffectKind::Burn];
	// The creature that applied the effect, if a creature did: its alignment moves rather than its player's, and it counts
	// what the effect destroys
	const auto actingCreature = ActingCreature(source);
	const bool byCreature = actingCreature != entt::null;
	// The cry of a town that the harm attacks: none when nothing burns, crushes, hits or throws
	std::optional<guidance::TownAttackVoice> cry;
	if (const auto fire = guidance::FireLeads(values); fire.has_value())
	{
		cry = guidance::TownAttackVoice {.fire = *fire, .attacker = CryAttacker(registry, source)};
	}
	// What applied the effect, when no miracle did: none for a fall onto the land
	const bool appliedByNothing =
	    source.appliedBy.has_value() && (*source.appliedBy == entt::null || !PositionOf(*source.appliedBy).has_value());
	ecs::GamePhysicsHooksWorld hooksWorld;
	for (const auto& outcome : outcomes)
	{
		const auto entity = outcome.entity;
		if (!registry.Valid(entity))
		{
			continue;
		}
		// The heat of an effect goes where heat that comes to the object goes: a temple's heart or a worship site asks
		// the heart, and beams as heat breaks buildings, also when the effect has no heat
		const auto heatTaker = ecs::temple_heart_pass_on::TakerOf(hooksWorld, entity, true);
		// A burn heats what it reaches, which may catch and burn; a negative one, as water's, cools what is hot
		if (burn != 0.0f && Locator::fireSystem::has_value() && registry.Valid(heatTaker))
		{
			Locator::fireSystem::value().ApplyBurn(heatTaker, burn, source.player);
		}
		// A creature mends its marks and changes its opinion before it takes the heal and the harm
		magic_living::AfterEffect(entity, values, source);
		if (outcome.lifeAfter.has_value())
		{
			if (registry.AllOf<CreatureNeeds>(entity))
			{
				SetLife(entity, *outcome.lifeAfter);
			}
			else
			{
				// Anything else living takes the heal, then the harm, as its kind takes it: a building hurt sends its
				// people out. A thing that a script holds in a cutscene, or an indestructible thing, keeps its life when
				// a step leaves nearly no life.
				if (outcome.healed > 0.0f)
				{
					ecs::world_objects::IncreaseLife(entity, outcome.healed);
				}
				if (outcome.damaged > 0.0f)
				{
					ecs::world_objects::ReduceLife(entity, outcome.damaged,
					                               source.playerless ? std::nullopt : std::optional(source.player),
					                               source.creatureSmash);
				}
			}
		}
		// The steps after the harm still see a thing that the harm destroys, so read what they need of it first. A
		// building or a dispenser that goes leaves its town, but a fish farm that goes keeps its town.
		const bool townBuilding = registry.AnyOf<Abode, SpellDispenser>(entity);
		const auto ownerBefore = ecs::world_objects::PlayerOf(entity);
		const auto positionBefore = PositionOf(entity);
		const auto keptTown = registry.AllOf<FishFarm>(entity) ? ecs::world_objects::TownAttackTargetOf(entity) : std::nullopt;
		// The life is read again after the heal and the harm, as a protected thing or a building not yet built can keep
		// its life. A thing that went with the harm has none left.
		const bool destroyed =
		    outcome.lifeBefore.has_value() && *outcome.lifeBefore != 0.0f && ecs::world_objects::LifeOf(entity) == 0.0f;
		// Brought from some life to none, it is destroyed first. The creature that applied the effect counts it.
		if (destroyed)
		{
			if (byCreature)
			{
				registry.Get<Creature>(actingCreature).objectsDestroyed += 1.0f;
			}
			SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Magic: a miracle took the last of {}'s life", entt::to_integral(entity));
			DestroyedByEffect(entity, {.killer = source.playerless ? std::nullopt : std::optional(source.player),
			                           .weight = outcome.lifeBefore.value_or(0.0f)});
		}
		// The player of the thing after the destruction: a dead villager has no town and so no player. A building that
		// went has no town and is the neutral player's.
		const bool stays = registry.Valid(entity);
		const auto owner = stays          ? ecs::world_objects::PlayerOf(entity)
		                   : townBuilding ? std::optional(PlayerNames::NEUTRAL)
		                                  : ownerBefore;
		// The people nearby react to what was crushed, once for each thing: to what applied the blow when it stands
		// somewhere, else to the thing itself, for the thing's player
		if (outcome.crushed && Locator::reactionSystem::has_value() && !Locator::reactionSystem::value().HasReaction(entity))
		{
			const auto initiator = byCreature                                          ? actingCreature
			                       : source.appliedBy.has_value() && !appliedByNothing ? *source.appliedBy
			                                                                           : entity;
			if (const auto position = initiator == entity ? positionBefore : PositionOf(initiator))
			{
				Locator::reactionSystem::value().Create({.initiator = initiator,
				                                         .type = Reaction::ReactToObjectCrushed,
				                                         .player = owner.value_or(PlayerNames::NEUTRAL),
				                                         .position = *position,
				                                         .playerless = !owner.has_value(),
				                                         .onCast = true});
			}
		}
		// Harm done to a town's people or buildings counts as an attack on the town by whoever did it, when something
		// applied it. A villager that died and a building that went no longer have a town.
		if (outcome.aggression && !appliedByNothing)
		{
			const auto target = stays ? ecs::world_objects::TownAttackTargetOf(entity) : keptTown;
			entt::entity applyingCreature = entt::null;
			if (source.appliedBy.has_value() && registry.Valid(*source.appliedBy) &&
			    registry.AllOf<Creature>(*source.appliedBy))
			{
				applyingCreature = *source.appliedBy;
			}
			// The town keeps the values of the effect itself, before the defence of what it struck
			const ecs::town_aggression::Harm harm {.burn = values[magic::EffectKind::Burn],
			                                       .crush = values[magic::EffectKind::Crush],
			                                       .hit = values[magic::EffectKind::Hit]};
			if (target.has_value())
			{
				ecs::world_objects::AttackTown(*target, outcome.damaged + outcome.burnt, source.player, harm, applyingCreature,
				                               cry);
			}
		}
		// A player's effect, not a creature's, is remembered against them by the player whose object it harmed
		if (!byCreature && !source.playerless && owner.has_value())
		{
			AddDamageFrom(*owner, source.player, outcome.damaged + outcome.burnt);
		}
		// The alignment moves last, by the life read after the destruction. A creature that cannot die has all of its
		// life again, and an unbuilt building that a script keeps has full life.
		const auto& info = Locator::infoConstants::value();
		const auto recounted = magic::WithLifeAfter(outcome, values, ecs::world_objects::LifeOf(entity), info.alignment,
		                                            info.player.applyEffectAlignmentChangeAddition, AlignmentOf(source));
		AddEffectAlignment(recounted, source, actingCreature);
		// The alignment history of the effect's player keeps each kind of change before the damping, also for the effect of
		// a creature
		if (!source.playerless && Locator::alignmentSystem::has_value())
		{
			for (const float term : recounted.alignmentTerms)
			{
				Locator::alignmentSystem::value().AddToHistory(source.player, AlignmentCause::Effect, term);
			}
		}
	}
}

void GameMagicWorld::AddEffectAlignment(const magic::EffectOutcome& outcome, const magic::EffectSource& source,
                                        entt::entity actingCreature)
{
	// No change of life moves no alignment
	if (outcome.alignmentTerms == magic::EffectAlignmentTerms {})
	{
		return;
	}
	// What a creature did moves its own alignment, what a player did the player's, over the turns to come. The waiting
	// alignment takes the change of each kind of effect one at a time, in the order of the outcome.
	if (actingCreature != entt::null)
	{
		auto& pending = EntityRegistry().Get<Creature>(actingCreature).pendingAlignment;
		pending = magic::AddEffectAlignmentChanges(pending, outcome.alignmentDamped);
	}
	else if (!source.playerless && source.player != PlayerNames::NEUTRAL && Locator::alignmentSystem::has_value())
	{
		for (const float change : outcome.alignmentDamped)
		{
			Locator::alignmentSystem::value().AddPendingAlignment(source.player, change);
		}
	}
}

void GameMagicWorld::Water(const magic::WaterDrop& drop)
{
	auto& registry = EntityRegistry();
	// The objects whose edge the drop falls within reach of across the ground take the water: trees grow, fields are
	// sown and ripen, and the people come to see a fire put out. They are looked for in the nine cells round the drop's,
	// in a spiral out from it; one spanning several of them takes the water in each.
	if (Locator::entitiesMap::has_value())
	{
		const auto& map = Locator::entitiesMap::value();
		const auto start = glm::ivec2(ecs::MapInterface::GetGridCell(drop.position));
		glm::ivec2 offset {0, 0};
		map_coords::Spiral spiral;
		constexpr int k_DropCells = 9;
		std::vector<entt::entity> objects;
		for (int step = 0; step < k_DropCells; ++step)
		{
			const auto cell = start + offset;
			if (map_coords::InBounds(cell))
			{
				const auto id = ecs::MapInterface::CellId(cell);
				objects.assign(map.GetFixedInGridCell(id).begin(), map.GetFixedInGridCell(id).end());
				objects.insert(objects.end(), map.GetMobileInGridCell(id).begin(), map.GetMobileInGridCell(id).end());
				for (const auto object : objects)
				{
					const auto* transform = registry.Valid(object) ? registry.TryGet<const Transform>(object) : nullptr;
					if (transform == nullptr)
					{
						continue;
					}
					// Each kind of thing counts by its own radius on the ground: a field is 5 m across whatever its size,
					// a creature counts by its body and a food pile by what it holds
					const float across = gutils::GetDistance(transform->position, drop.position);
					const float radius = object_measures::TwoDRadius(registry, object);
					if (across - radius < drop.reach)
					{
						WaterObject(object, drop);
					}
				}
			}
			const auto& next = spiral.Next();
			offset += glm::ivec2(next.x, next.z);
		}
	}
	if (drop.ringGrowth.has_value() && Locator::gameRandom::has_value())
	{
		// The colour and the angle of the ring come from the synced numbers. Every computer draws them, also a computer
		// that does not show the rings.
		auto& random = Locator::gameRandom::value();
		const auto colour = k_RippleColours.at(random.GameRand(static_cast<uint32_t>(k_RippleColours.size())));
		const auto angle = random.GameFloatRand(k_TwoPi);
		if (Locator::waterRingSystem::has_value())
		{
			Locator::waterRingSystem::value().Add(
			    {.position = drop.position, .growth = *drop.ringGrowth, .angle = angle, .argb = colour});
		}
	}
}

void GameMagicWorld::WaterObject(entt::entity object, const magic::WaterDrop& drop)
{
	auto& registry = EntityRegistry();
	// Something burning: the people come to watch the water put it out, one reaction for each miracle at a time
	if (Locator::fireSystem::has_value() && Locator::fireSystem::value().IsOnFire(object) &&
	    Locator::reactionSystem::has_value() && drop.spell != entt::null)
	{
		auto& reactions = Locator::reactionSystem::value();
		auto& watching = _puttingOutFire[drop.spell];
		if (watching == 0 || !reactions.IsActive(watching))
		{
			watching = reactions.Create({.initiator = drop.spell,
			                             .type = Reaction::ReactToMagicWaterPuttingOutFire,
			                             .player = drop.source.player,
			                             .position = drop.position,
			                             .onCast = true});
			SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Water: the people come to watch #{}'s fire put out, reaction {}",
			                    entt::to_integral(object), watching);
		}
	}
	const auto& info = Locator::infoConstants::value();
	if (auto* field = registry.TryGet<Field>(object))
	{
		// The caster's creature may learn to water the crops from watching. It sees the plain water miracle, whichever
		// water was cast.
		if (drop.source.player != PlayerNames::NEUTRAL && Locator::creatureMindSystem::has_value())
		{
			Locator::creatureMindSystem::value().PlayerCastDeed(magic::k_DeedCastWaterOnCrops, drop.position, object,
			                                                    drop.source.player, MagicType::Water);
		}
		if (Locator::fireSystem::has_value() && Locator::fireSystem::value().IsOnFire(object))
		{
			return;
		}
		const auto& type = info.fieldType.at(static_cast<size_t>(field->type));
		magic::WaterCrop crop {.timesSown = field->crop.timesSown, .age = field->crop.age, .food = field->crop.food};
		magic::WaterField(crop, {.timesToSow = type.timesToSow,
		                         .ageRipe = type.ageRecolt,
		                         .totalFood = type.totalFoodInField,
		                         .effectOfWater = type.effectOfWaterSpell});
		field->crop.timesSown = crop.timesSown;
		field->crop.age = crop.age;
		field->crop.food = crop.food;
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Water: field #{} sown {} times, age {:.0f}, food {:.1f}",
		                    entt::to_integral(object), crop.timesSown, crop.age, crop.food);
		return;
	}
	if (auto* tree = registry.TryGet<Tree>(object))
	{
		const auto& type = info.tree.at(static_cast<size_t>(tree->type));
		auto& transform = registry.Get<Transform>(object);
		const auto grown = magic::WaterTree(transform.scale.y, tree->maxSize,
		                                    {.growthAmount = type.growthAmount,
		                                     .waterAccelerator = type.waterSpellAcceleratorMultiplier,
		                                     .madeToGrow = tree->madeToGrow},
		                                    drop.extreme);
		if (grown.scale != transform.scale.y)
		{
			const float ratio = grown.scale / std::max(transform.scale.y, 1e-4f);
			transform.scale = glm::vec3(grown.scale);
			if (auto* fixed = registry.TryGet<Fixed>(object))
			{
				fixed->boundingRadius *= ratio;
			}
			tree->maxSize = grown.target;
			registry.SetDirty();
			// A tree in the map's cells that the water grows comes out of them and goes in again
			ecs::thing_map_place::Reinsert(object);
			// It rustles as it grows
			if (Locator::sounds::has_value() && Locator::gameRandom::has_value())
			{
				const auto sample =
				    k_TreeGrowSounds.at(Locator::gameRandom::value().LocalRand(static_cast<int32_t>(k_TreeGrowSounds.size())));
				audio::StartGameSoundEffect(static_cast<entt::id_type>(sample),
				                            {.position = transform.position, .owner = object});
			}
		}
		else if (!grown.canGrow && !drop.extreme)
		{
			PlantNear(object, drop);
		}
		else
		{
			tree->maxSize = grown.target;
		}
	}
}

bool GameMagicWorld::CanBeDestroyedBySpell(entt::entity object, entt::entity spell) const
{
	return ecs::world_objects::CanBeDestroyedBySpell(object, spell);
}

void GameMagicWorld::PlantNear(entt::entity tree, const magic::WaterDrop& drop)
{
	if (!Locator::forestSystem::has_value())
	{
		return;
	}
	// The tree's forest gains a young tree beside it, when it is not too soon after the last any forest gained
	const auto planted = Locator::forestSystem::value().AddTreeNear(tree);
	// In a network game the player of the water counts a tree grown, for the winning condition
	if (planted.has_value() && drop.source.player != PlayerNames::NEUTRAL && Locator::gameEndSystem::has_value())
	{
		Locator::gameEndSystem::value().AddToCondition(drop.source.player, game_end::Condition::TreesGrown, 1);
	}
	// Planting a tree is good. The alignment history of the player keeps the change.
	if (planted.has_value() && drop.source.player != PlayerNames::NEUTRAL && Locator::alignmentSystem::has_value())
	{
		auto& alignment = Locator::alignmentSystem::value();
		const float change = magic::DampAlignmentChange(Locator::infoConstants::value().player.treePullPutAlignmentChange,
		                                                alignment.GetPlayerAlignment(drop.source.player));
		alignment.AddToHistory(drop.source.player, AlignmentCause::Tree, change);
		alignment.AddPendingAlignment(drop.source.player, change);
	}
}

void GameMagicWorld::PlayerAffected(entt::entity object, MagicType type, PlayerNames player)
{
	auto& registry = EntityRegistry();
	if (!registry.Valid(object) || !ReceiverOf(object).has_value() || !Locator::creatureMindSystem::has_value())
	{
		return;
	}
	const auto* transform = registry.TryGet<const Transform>(object);
	if (transform == nullptr)
	{
		return;
	}
	// The town nearest what was reached, within reach, by the game's distance; the first found wins a tie
	const auto here = map_coords::FromMetres({transform->position.x, transform->position.z});
	float best = magic::k_DeedTownReach;
	const ecs::components::Town* nearest = nullptr;
	registry.Each<const ecs::components::Town, const Transform>(
	    [&](entt::entity, const ecs::components::Town& town, const Transform& at) {
		    const float distance = gutils::GetDistanceInMetres(here, map_coords::FromMetres({at.position.x, at.position.z}));
		    if (distance < best)
		    {
			    best = distance;
			    nearest = &town;
		    }
	    });
	const auto& effect = magic::GetMagicEffectInfo(Locator::infoConstants::value(), type);
	const bool onFire = Locator::fireSystem::has_value() && Locator::fireSystem::value().IsOnFire(object);
	const auto deed = magic::MiracleDeed(type, effect.perceivedPlayerDesire.at(0) == CreatureDesires::Anger,
	                                     nearest != nullptr && nearest->owner != player,
	                                     {.field = registry.AllOf<Field>(object),
	                                      .onFire = onFire,
	                                      .storagePit = registry.AllOf<StoragePit>(object),
	                                      .worshipSite = registry.AllOf<WorshipSite>(object),
	                                      .buildingBeingBuilt = ecs::construction::IsBeingBuilt(object)});
	if (deed != magic::k_NoDeed)
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Magic: miracle {} by player {} shows deed {} on #{}", static_cast<int>(type),
		                    static_cast<int>(player), deed, entt::to_integral(object));
		Locator::creatureMindSystem::value().PlayerCastDeed(deed, transform->position, object, player, type);
	}
}

std::vector<entt::entity> GameMagicWorld::HealTargets(glm::vec3 point, float radius, size_t maximum) const
{
	return magic_living::HealTargets(point, radius, maximum);
}

void GameMagicWorld::CurePoison(entt::entity object)
{
	magic_living::CurePoison(object);
}

void GameMagicWorld::ProcessTurn()
{
	// The miracles that have gone no longer keep their watchers
	std::erase_if(_puttingOutFire, [](const auto& entry) { return !EntityRegistry().Valid(entry.first); });
}

void GameMagicWorld::Reset()
{
	_puttingOutFire.clear();
}
