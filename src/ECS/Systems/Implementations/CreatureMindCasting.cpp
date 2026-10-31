/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A creature's mind casting miracles: whether it may try one, how it goes near, gets away from and turns to face what it
// casts at, and its try at the miracle. A try short of all but one of the sightings it needs fizzles; it counts as one
// more sighting, and the creature shows its embarrassment once it chooses what to do next, and may be sad about it too.
// A miracle it can't cast, too tired or at something it can't be cast at, leaves it frustrated: hungry above all else.

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>

#include <algorithm>
#include <limits>

#include <glm/geometric.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/mat2x2.hpp>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameHeading.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureCastAgenda.h"
#include "Creature/CreatureCastMoves.h"
#include "Creature/CreatureGiving.h"
#include "Creature/CreatureHelp.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/CreatureRoute.h"
#include "Creature/CreatureSpellCasting.h"
#include "Creature/CreatureSpellMind.h"
#include "Creature/CreatureThrow.h"
#include "Creature/CreatureWatching.h"
#include "Creature/LeashRules.h"
#include "CreatureMindSystem.h"
#include "CreatureMindSystemDetail.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureCasting.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureGestureDrawing.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/Vortex.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/CreatureObstacleThings.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "ObjectMeasures.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
namespace cast_moves = openblack::creature_cast_moves;
namespace obstacle_things = openblack::ecs::creature_obstacle_things;
using creature_desires::Desire;
using Kind = creature_mind::Movement::Kind;

constexpr float k_TurnsPerSecond = 10.0f;
/// The miracles a creature casts are numbered from the first to the last magic type
constexpr uint32_t k_LastCreatureMagic = 41;
/// A creature tries a power-up miracle only once grown past this stage
constexpr uint32_t k_PowerUpPhase = 7;

ecs::Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

glm::vec2 Flat(const glm::vec3& point)
{
	return {point.x, point.z};
}

/// The heading of a creature: the heading that its movement keeps. A creature that has not moved yet has the heading of
/// the way that its model faces.
GameHeading HeadingOf(const ecs::Registry& registry, entt::entity creature, const Transform& transform)
{
	if (const auto* moving = registry.TryGet<const CreatureLocomotion>(creature); moving != nullptr && moving->started)
	{
		return moving->heading;
	}
	const auto ahead = -(transform.rotation * glm::vec3(0.0f, 0.0f, 1.0f));
	return GameHeading::Of(ahead.x, ahead.z);
}

/// The map position of a point across the land
glm::vec2 MapPosition(glm::vec2 point)
{
	return map_coords::ToMetres(map_coords::FromMetres(point));
}

/// The map position of a point as a point, with the height of the land under it
glm::vec3 MapPoint(glm::vec2 point)
{
	return map_coords::ToWorld(Locator::terrainSystem::value(), map_coords::FromMetres(point));
}

/// The balls a thing in a cell keeps creatures off, as the cell test looks at them: a tree a small one at its trunk, a
/// worship site its own ten, anything else fixed the outline of its model's box; nothing for a forest as a whole, for
/// the temple (it keeps the cell from being clear in any case), or for what moves about
std::vector<cast_moves::CollideSphere> CollideSpheres(const ecs::Registry& registry, entt::entity entity,
                                                      const creature_obstacles::Thing& thing)
{
	const auto* transform = registry.TryGet<const Transform>(entity);
	if (transform == nullptr)
	{
		return {};
	}
	if (registry.AllOf<Tree>(entity))
	{
		return {{.centre = {transform->position.x, 0.0f, transform->position.z}, .radius = cast_moves::k_TreeCollideRadius}};
	}
	if (registry.AllOf<WorshipSite>(entity))
	{
		const auto spheres = cast_moves::WorshipSiteSpheres(
		    transform->position, creature_obstacles::YAngleOf(transform->rotation), transform->scale.x);
		return {spheres.begin(), spheres.end()};
	}
	if (!registry.AnyOf<Abode, Feature, AnimatedStatic, Flowers, SpellDispenser, TeleportStone, WorshipAltar, Vortex,
	                    MobileStatic, DeadTree, MagicShield, ShieldDome, ScriptHighlight, FishFarm>(entity))
	{
		return {};
	}
	// The outline of its model's box, at the size it is kept at; a shield's is its solid dome's
	const glm::mat2 turn(transform->rotation[0].x, transform->rotation[0].z, transform->rotation[2].x,
	                     transform->rotation[2].z);
	const auto centre = thing.position + turn * (glm::vec2(thing.boxCentre.x, thing.boxCentre.z) * thing.scale);
	const auto half = glm::vec2(thing.boxHalfSize.x, thing.boxHalfSize.z) * thing.scale;
	std::vector<cast_moves::CollideSphere> spheres;
	for (const auto& circle : cast_moves::CollideCircles(half, centre, turn))
	{
		spheres.push_back({.centre = {circle.centre.x, 0.0f, circle.centre.y}, .radius = circle.radius});
	}
	return spheres;
}

/// Whether a thing standing in a cell of the land keeps it from being clear for a creature to go to: the temple always;
/// a tree or a field by the balls it keeps creatures off; anything else by its balls when the creature must go round it
/// by the rule of its routes; and, when asked, anything living, the creature itself included. Things that move about
/// keep no balls, so only the temple and, when asked, the living among them count.
bool CellBlocked(const ecs::Registry& registry, entt::entity creature, int32_t x, int32_t z, bool livingBlocks = false)
{
	if (!Locator::entitiesMap::has_value() || x < 0 || z < 0)
	{
		return false;
	}
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* at = registry.TryGet<const Transform>(creature);
	if (body == nullptr || at == nullptr)
	{
		return false;
	}
	const auto* locomotion = registry.TryGet<const CreatureLocomotion>(creature);
	const auto walker = obstacle_things::WalkerOf(registry, creature, *body, locomotion != nullptr ? locomotion->radius : 0.0f,
	                                              Flat(at->position));
	const auto& map = Locator::entitiesMap::value();
	const ecs::MapInterface::CellId cell {static_cast<uint16_t>(x), static_cast<uint16_t>(z)};
	const auto blocks = [&](entt::entity entity) {
		if (!registry.Valid(entity))
		{
			return false;
		}
		cast_moves::ThingInCell inCell {.temple = registry.AllOf<Temple>(entity),
		                                .living = registry.AnyOf<Villager, Creature, Animal>(entity),
		                                .tested = false,
		                                .spheres = {}};
		if (!inCell.temple)
		{
			if (const auto thing = obstacle_things::ThingOf(registry, entity))
			{
				inCell.tested = registry.AnyOf<Tree, Field>(entity) || creature_obstacles::MustAvoid(*thing, walker);
				if (inCell.tested)
				{
					inCell.spheres = CollideSpheres(registry, entity, *thing);
				}
			}
		}
		return cast_moves::BlocksCell(inCell, x, z, livingBlocks);
	};
	return std::ranges::any_of(map.GetFixedInGridCell(cell), blocks) ||
	       std::ranges::any_of(map.GetMobileInGridCell(cell), blocks);
}

/// How many times a creature must see a miracle to learn it, and how often it has
struct Sightings
{
	float seen;
	float needed;
	bool known;
};
std::optional<Sightings> SightingsOf(const CreatureMindState& mind, const creature_mind_tables::Tables& tables,
                                     CreatureType species, uint32_t magicType)
{
	if (!mind.learnt.has_value() || magicType >= tables.miracles.size())
	{
		return std::nullopt;
	}
	const auto& knowledge = mind.learnt->knowledge;
	if (magicType >= knowledge.miraclesSeen.size() || magicType >= knowledge.miraclesKnown.size())
	{
		return std::nullopt;
	}
	return Sightings {.seen = static_cast<float>(knowledge.miraclesSeen[magicType].count),
	                  .needed =
	                      creature_watching::TimesNeeded(tables.miracles[magicType].timesToSee,
	                                                     creature_mind_tables::MiracleMultiplier(creature::InfoRow(species))),
	                  .known = knowledge.miraclesKnown[magicType]};
}

/// Whether the creature's body can pay for making a miracle
bool CanPayFor(entt::entity creature, MagicType type)
{
	const auto& registry = Entities();
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* needs = registry.TryGet<const CreatureNeeds>(creature);
	const auto& info = Locator::infoConstants::value();
	if (body == nullptr || needs == nullptr || creature::InfoRow(body->species) >= info.creature.size())
	{
		return false;
	}
	const auto& species = info.creature.at(creature::InfoRow(body->species));
	return creature_spell_casting::CanCast({.size = ShownSize(*body),
	                                        .strength = body->strength,
	                                        .energy = needs->needs.energy,
	                                        .exhaustion = needs->needs.exhaustion},
	                                       {.chantsPerEnergy = species.chantsPerEnergy,
	                                        .energyFloor = species.spellEnergyFloor,
	                                        .sizeFactor = species.spellSizeFactor},
	                                       magic::GetMagicEffectInfo(info, type).costToCreate);
}

/// A move of a cast goes on at the speed that the creature got last, unless the move asks for the run speed
CreatureLocomotionSystemInterface::Pace KeptOrRun(const CreatureCasting& casting)
{
	return casting.run ? CreatureLocomotionSystemInterface::Pace::Run : CreatureLocomotionSystemInterface::Pace::Keep;
}
} // namespace

std::optional<uint32_t> CreatureMindSystem::CastMagicOf(uint32_t action)
{
	const auto& info = Locator::infoConstants::value();
	if (action >= info.creatureAction.size())
	{
		return std::nullopt;
	}
	const auto magic = info.creatureAction.at(action).magicType;
	if (magic == 0 || magic > k_LastCreatureMagic)
	{
		return std::nullopt;
	}
	return magic;
}

bool mind_detail::CellClearFor(const ecs::Registry& registry, entt::entity creature, int32_t x, int32_t z, bool livingBlocks)
{
	if (!Locator::creatureLocomotionSystem::has_value())
	{
		return false;
	}
	const auto& land = Locator::creatureLocomotionSystem::value().GetWalkableLand();
	return land.At(x, z) == creature_route::Ground::Open && !CellBlocked(registry, creature, x, z, livingBlocks);
}

bool CreatureMindSystem::IsCellClearFor(entt::entity creature, int32_t x, int32_t z, bool livingBlocks) const
{
	return mind_detail::CellClearFor(Entities(), creature, x, z, livingBlocks);
}

std::optional<glm::vec2> CreatureMindSystem::FindClearAreaFor(entt::entity creature, glm::vec2 point, float width,
                                                              bool livingBlocks) const
{
	return mind_detail::ClearAreaFor(Entities(), creature, point, width, livingBlocks);
}

std::optional<glm::vec2> mind_detail::ClearAreaFor(const ecs::Registry& registry, entt::entity creature, glm::vec2 point,
                                                   float width, bool livingBlocks)
{
	if (!Locator::creatureLocomotionSystem::has_value())
	{
		return std::nullopt;
	}
	const auto found = cast_moves::FindClearArea(
	    point, width, [&](int32_t x, int32_t z) { return CellClearFor(registry, creature, x, z, livingBlocks); });
	// Kept within an area, a place outside it is no use
	const auto* leashes = registry.TryGet<const CreatureLeash>(creature);
	if (found.has_value() && leashes != nullptr)
	{
		const bool leashed = leashes->worn.has_value();
		if (creature_leash::IsConfined(leashes->confinementRadius, leashed, !leashed || leashes->worn->works) &&
		    gutils::GetDistanceInMetres(*found, Flat(leashes->confinementCentre)) > leashes->confinementRadius)
		{
			return std::nullopt;
		}
	}
	return found;
}

std::optional<std::pair<float, float>> mind_detail::MiracleSightings(const CreatureMindState& mind,
                                                                     const creature_mind_tables::Tables& tables,
                                                                     CreatureType species, MagicType magic)
{
	const auto sightings = SightingsOf(mind, tables, species, static_cast<uint32_t>(magic));
	if (!sightings.has_value())
	{
		return std::nullopt;
	}
	return std::pair {sightings->seen, sightings->needed};
}

std::optional<std::pair<float, float>> CreatureMindSystem::MiracleSightings(entt::entity creature, MagicType type)
{
	const auto* tables = GetTables();
	const auto& registry = Entities();
	const auto* mind = registry.TryGet<const CreatureMindState>(creature);
	const auto* body = registry.TryGet<const Creature>(creature);
	if (tables == nullptr || mind == nullptr || body == nullptr)
	{
		return std::nullopt;
	}
	return mind_detail::MiracleSightings(*mind, *tables, body->species, type);
}

bool CreatureMindSystem::MayCast(entt::entity creature, const CreatureMindState& mind, uint32_t action, bool powerUp)
{
	const auto* tables = GetTables();
	const auto* body = Entities().TryGet<const Creature>(creature);
	const auto* executor = tables != nullptr && action < tables->actions.size()
	                           ? creature_plan_actions::For(tables->actions[action].name)
	                           : nullptr;
	if (tables == nullptr || body == nullptr)
	{
		return false;
	}
	// To impress, it looks at all the miracles that impress, not at the action's own miracle
	if (executor != nullptr && executor->build == creature_plan_actions::Build::CastToImpress)
	{
		std::array<creature_mind::ImpressiveMiracleState, creature_mind::k_ImpressMiraclesItMayKnow.size()> states {};
		std::ranges::transform(creature_mind::k_ImpressMiraclesItMayKnow, states.begin(), [&](uint32_t type) {
			const auto sightings = SightingsOf(mind, *tables, body->species, type);
			return creature_mind::ImpressiveMiracleState {
			    .known = sightings.has_value() && sightings->known,
			    .canPay = CanPayFor(creature, static_cast<MagicType>(type)),
			    .seen = sightings.has_value() ? sightings->seen : 0.0f,
			    .needed = sightings.has_value() ? sightings->needed : 0.0f,
			};
		});
		return creature_mind::CanCastToImpress(states);
	}
	// To amuse, it needs a spell on creatures that it knows and can pay for
	if (executor != nullptr && executor->build == creature_plan_actions::Build::CastAmusing)
	{
		return !AmusingSpells(creature, mind).empty();
	}
	// It casts water on itself only while it burns
	if (executor != nullptr && executor->build == creature_plan_actions::Build::CastWaterOnItself &&
	    !(Locator::fireSystem::has_value() && Locator::fireSystem::value().IsOnFire(creature)))
	{
		return false;
	}
	const auto magic = CastMagicOf(action);
	if (!magic.has_value() || (powerUp && mind.developmentPhase <= k_PowerUpPhase))
	{
		return false;
	}
	const auto sightings = SightingsOf(mind, *tables, body->species, *magic);
	// The game goes by the sightings alone
	if (!sightings.has_value() || !creature_spell_casting::MayTry(sightings->seen, sightings->needed))
	{
		return false;
	}
	return CanPayFor(creature, static_cast<MagicType>(*magic));
}

std::optional<uint32_t> CreatureMindSystem::PickImpressiveMiracle(entt::entity creature)
{
	auto& registry = Entities();
	const auto* tables = GetTables();
	const auto* mind = registry.TryGet<const CreatureMindState>(creature);
	const auto* body = registry.TryGet<const Creature>(creature);
	if (tables == nullptr || mind == nullptr || body == nullptr)
	{
		return std::nullopt;
	}
	std::array<bool, creature_mind::k_ImpressiveMiracles.size()> known {};
	std::ranges::transform(creature_mind::k_ImpressiveMiracles, known.begin(), [&](uint32_t type) {
		const auto sightings = SightingsOf(*mind, *tables, body->species, type);
		return sightings.has_value() && sightings->known;
	});
	auto& castTurn = registry.Context().impressiveMiracleCastTurn;
	const auto picked = creature_mind::PickImpressiveMiracle(known, castTurn);
	if (!picked.has_value())
	{
		return std::nullopt;
	}
	// Record the turn: the next creature that impresses picks a miracle that no creature cast for a longer time
	castTurn.at(*picked) = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
	return creature_mind::k_ImpressiveMiracles.at(*picked);
}

std::vector<creature_mind::AmusingSpell> CreatureMindSystem::AmusingSpells(entt::entity creature, const CreatureMindState& mind)
{
	std::vector<creature_mind::AmusingSpell> spells;
	const auto* tables = GetTables();
	const auto* body = Entities().TryGet<const Creature>(creature);
	if (tables == nullptr || body == nullptr || !Locator::infoConstants::has_value())
	{
		return spells;
	}
	const auto& info = Locator::infoConstants::value();
	for (auto type = creature_mind::k_FirstAmusingSpell; type <= creature_mind::k_LastAmusingSpell; ++type)
	{
		const auto sightings = SightingsOf(mind, *tables, body->species, type);
		const auto magic = static_cast<MagicType>(type);
		if (sightings.has_value() && sightings->known && CanPayFor(creature, magic))
		{
			spells.push_back(
			    {.magicType = type, .gesture = static_cast<uint32_t>(magic::GetMagicInfo(info, magic).gestureType)});
		}
	}
	return spells;
}

bool CreatureMindSystem::SeemsFriendly(entt::entity creature, entt::entity other)
{
	const auto& registry = Entities();
	const auto* mine = registry.Valid(creature) ? registry.TryGet<const CreatureMindState>(creature) : nullptr;
	const auto* theirs = registry.Valid(other) ? registry.TryGet<const CreatureMindState>(other) : nullptr;
	if (mine == nullptr || theirs == nullptr)
	{
		return false;
	}
	return creature_mind::SeemsFriendly(DesireAboutCreature(creature, *mine, other, {}),
	                                    DesireAboutCreature(other, *theirs, creature, {}));
}

std::optional<creature_plan_actions::CastInfo> CreatureMindSystem::CastInfoFor(entt::entity creature, uint32_t action,
                                                                               std::optional<entt::entity> target)
{
	const auto& registry = Entities();
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* tables = GetTables();
	const auto* executor = tables != nullptr && action < tables->actions.size()
	                           ? creature_plan_actions::For(tables->actions[action].name)
	                           : nullptr;
	if (body == nullptr)
	{
		return std::nullopt;
	}
	// To impress, it picks the miracle as it builds the agenda, and casts none when it knows none
	if (executor != nullptr && executor->build == creature_plan_actions::Build::CastToImpress)
	{
		return creature_plan_actions::CastInfo {.magicType = PickImpressiveMiracle(creature).value_or(0),
		                                        .height = cast_moves::k_HeightOfSizeOne * ShownSize(*body)};
	}
	// To amuse, it picks the spell as it builds the agenda
	if (executor != nullptr && executor->build == creature_plan_actions::Build::CastAmusing)
	{
		const auto* mind = registry.TryGet<const CreatureMindState>(creature);
		return creature_plan_actions::CastInfo {.height = cast_moves::k_HeightOfSizeOne * ShownSize(*body),
		                                        .amusingSpells = mind != nullptr ? AmusingSpells(creature, *mind)
		                                                                         : std::vector<creature_mind::AmusingSpell> {}};
	}
	auto magic = CastMagicOf(action);
	if (!magic.has_value() && executor != nullptr)
	{
		magic = executor->castMagic;
	}
	if (!magic.has_value())
	{
		return std::nullopt;
	}
	const auto& info = Locator::infoConstants::value();
	const auto type = static_cast<MagicType>(*magic);
	// The gesture of the miracle: a power-up has the gesture that powers up its seed, a plain miracle has none
	const auto gesture = magic::GetMagicInfo(info, type).gestureType;
	creature_plan_actions::CastInfo cast {
	    .magicType = *magic,
	    .gesture = static_cast<uint32_t>(gesture),
	    .height = cast_moves::k_HeightOfSizeOne * ShownSize(*body),
	    .targetLiving = target.has_value() && registry.Valid(*target) && Locator::creatureObjectActionSystem::has_value() &&
	                    creature_object_actions::IsLiving(Locator::creatureObjectActionSystem::value().KindOf(*target).thing)};
	// The creature stops its own fire from a clear area near it. The area is 1.7 times as wide as the creature is
	// tall. Living things in the area do not block it.
	if (executor != nullptr && executor->build == creature_plan_actions::Build::CastWaterOnItself)
	{
		if (const auto at = object_measures::PositionOf(registry, creature))
		{
			cast.clearArea = mind_detail::ClearAreaFor(registry, creature, Flat(*at),
			                                           creature_mind::k_WaterOnItselfClearHeights * cast.height, false);
		}
	}
	// The creature casts a fireball from a clear area near it, as wide as five times its radius. Living things in the
	// area do not block it.
	if (executor != nullptr && executor->build == creature_plan_actions::Build::CastFireball)
	{
		if (const auto at = object_measures::PositionOf(registry, creature))
		{
			cast.clearArea = mind_detail::ClearAreaFor(
			    registry, creature, Flat(*at),
			    creature_mind::k_FireballClearRadii * object_measures::TwoDRadius(registry, creature), false);
		}
	}
	// The creature casts a forest at the place where the thing is now
	if (executor != nullptr && executor->build == creature_plan_actions::Build::CastForest && target.has_value())
	{
		cast.place = object_measures::PositionOf(registry, *target);
	}
	return cast;
}

bool CreatureMindSystem::StartTry(entt::entity creature, MagicType type, std::optional<entt::entity> target)
{
	auto& registry = Entities();
	auto* mind = registry.TryGet<CreatureMindState>(creature);
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* tables = GetTables();
	if (mind == nullptr || body == nullptr || tables == nullptr || !Locator::magicSystem::has_value())
	{
		return false;
	}
	// A try short of all but one of the sightings it needs fizzles, and counts as one more
	const auto magicType = static_cast<uint32_t>(type);
	const auto sightings = SightingsOf(*mind, *tables, body->species, magicType);
	if (sightings.has_value())
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} tries miracle {}: seen {} of {}{}", entt::to_integral(creature),
		                    magicType, sightings->seen, sightings->needed, sightings->known ? ", known" : "");
	}
	if (sightings.has_value() && !creature_spell_casting::TrySucceeds(sightings->seen, sightings->needed))
	{
		auto* casting = registry.TryGet<CreatureCasting>(creature);
		auto& state = casting != nullptr ? *casting : registry.Assign<CreatureCasting>(creature);
		// A try at a place keeps the thing of the last try at a thing that fizzled
		if (target.has_value())
		{
			state.lastFizzleTarget = *target;
		}
		state.fizzle = CreatureCasting::Fizzle {.target = state.lastFizzleTarget, .magicType = type};
		if (Locator::creatureHelpSystem::has_value())
		{
			Locator::creatureHelpSystem::value().Raise(
			    creature, {.kind = creature_help::Kind::MessedUpAction, .parameter = creature_help::k_FumbledMiracle});
		}
		++mind->learnt->knowledge.miraclesSeen[magicType].count;
		return false;
	}
	return true;
}

void CreatureMindSystem::FrustrateAfterCast(entt::entity creature, [[maybe_unused]] MagicType type)
{
	auto& registry = Entities();
	auto* mind = registry.TryGet<CreatureMindState>(creature);
	const auto* body = registry.TryGet<const Creature>(creature);
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} couldn't cast miracle {}", entt::to_integral(creature),
	                    static_cast<uint32_t>(type));
	const auto& info = Locator::infoConstants::value();
	// Frustrated, it wants food above all
	if (mind != nullptr && body != nullptr && mind->desires.has_value() &&
	    creature::InfoRow(body->species) < info.creature.size())
	{
		creature_spell_mind::MakeFullyDominantOverOthers(*mind->desires, Desire::Hunger,
		                                                 info.creature.at(creature::InfoRow(body->species)).desireFloor);
	}
}

bool CreatureMindSystem::TryMiracle(entt::entity creature, MagicType type, entt::entity target)
{
	auto& registry = Entities();
	if (!registry.AllOf<CreatureMindState, Creature>(creature) || GetTables() == nullptr || !Locator::magicSystem::has_value())
	{
		return false;
	}
	auto& magic = Locator::magicSystem::value();
	magic.ReleaseCreatureCast(creature);
	// In a fight every miracle costs stamina, and it can't cast one it hasn't the stamina for
	if (auto* fighting = registry.TryGet<CreatureFighting>(creature))
	{
		const float cost =
		    creature_spell_casting::StaminaCost(magic::GetMagicEffectInfo(Locator::infoConstants::value(), type).costToCreate);
		auto& stamina = fighting->fighter.stamina;
		if (stamina < cost)
		{
			return false;
		}
		stamina = std::clamp(stamina - cost, 0.0f, 1.0f);
	}
	if (!StartTry(creature, type, target))
	{
		return false;
	}
	// It is frustrated only when it hadn't the energy for the miracle or the miracle couldn't be cast at the target
	if (magic.CastByCreature(creature, type, target))
	{
		// A cast that a script ordered shows in every build, so that a check of the story can find it
		if (registry.AllOf<ScriptControlled>(creature))
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {} cast miracle {} at {} for a script",
			                   entt::to_integral(creature), static_cast<uint32_t>(type), entt::to_integral(target));
		}
		else
		{
			SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} cast miracle {}", entt::to_integral(creature),
			                    static_cast<uint32_t>(type));
		}
		return true;
	}
	FrustrateAfterCast(creature, type);
	return false;
}

bool CreatureMindSystem::TryMiracleAt(entt::entity creature, MagicType type, glm::vec3 place)
{
	// A try at a place that fizzles keeps the miracle in the hand. Only a try that does not fizzle lets go of it.
	if (!StartTry(creature, type, std::nullopt))
	{
		return false;
	}
	Locator::magicSystem::value().ReleaseCreatureCast(creature);
	// It is frustrated only when it does not have the energy for the miracle, or the miracle cannot go to the place
	if (Locator::magicSystem::value().CastByCreatureAt(creature, type, place))
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} cast miracle {} at ({:.1f}, {:.1f})", entt::to_integral(creature),
		                    static_cast<uint32_t>(type), place.x, place.z);
		return true;
	}
	FrustrateAfterCast(creature, type);
	return false;
}

bool CreatureMindSystem::CastGivenGlobe(entt::entity creature, CreatureMindState& mind, entt::entity globe)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* tables = GetTables();
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* seed = registry.TryGet<const OneOffSpellSeed>(globe);
	const auto* at = registry.TryGet<const Transform>(creature);
	if (tables == nullptr || body == nullptr || seed == nullptr || at == nullptr || !Locator::infoConstants::has_value())
	{
		return false;
	}
	// The globe casts the first miracle of its seed, whatever its power-up
	const auto miracle = magic::GetSpellSeedInfo(Locator::infoConstants::value(), seed->seedType).magicTypes[0];
	const auto index = static_cast<size_t>(miracle);
	if (index >= tables->miracles.size())
	{
		return false;
	}
	const auto cast = creature_giving::CastGlobe(mind.developmentPhase, miracle, tables->miracles[index].desire);
	const auto action = cast.has_value() ? creature_mind_tables::FindAction(*tables, cast->action) : std::nullopt;
	if (!action.has_value() || cast->desire >= creature_desires::k_DesireCount)
	{
		return false;
	}
	const auto desire = static_cast<Desire>(cast->desire);
	std::optional<entt::entity> target;
	std::optional<entt::entity> whole;
	switch (cast->target)
	{
	case creature_giving::GlobeTarget::Itself:
		target = creature;
		whole = creature;
		break;
	case creature_giving::GlobeTarget::NearestCreature:
	{
		// The nearest other creature that it believes in, at any distance
		auto nearest = std::numeric_limits<float>::max();
		for (const auto& belief : mind.beliefs.InListOrder())
		{
			const auto* other = registry.Valid(belief.thing) && belief.thing != creature
			                        ? registry.TryGet<const Transform>(belief.thing)
			                        : nullptr;
			if (other == nullptr || !registry.AllOf<Creature>(belief.thing))
			{
				continue;
			}
			const auto distance = gutils::GetDistanceInMetres(Flat(at->position), Flat(other->position));
			if (distance < nearest)
			{
				nearest = distance;
				target = belief.thing;
			}
		}
		whole = target;
		break;
	}
	case creature_giving::GlobeTarget::MostUseful:
	{
		// Of the things that it believes in, the thing that is most useful for the desire. The action can exclude the
		// thing that it holds, which is the globe.
		const bool notHeld = tables->actions[*action].notOnWhatItHolds;
		const auto found =
		    BelievedThingsToActOn(creature, mind, desire, [&](entt::entity thing) { return !(notHeld && thing == globe); });
		float best = 0.0f;
		for (const auto& thing : found)
		{
			if (thing.usefulness > best)
			{
				best = thing.usefulness;
				target = static_cast<entt::entity>(thing.id);
			}
		}
		// A creature is its own whole thing
		if (target.has_value())
		{
			whole = registry.AllOf<Creature>(*target) ? target : WholeOfShownThing(creature, mind, *target);
		}
		break;
	}
	}
	// Without a target, or without the whole thing that the desire must be about, it uses the globe as any other gift
	if (!target.has_value() || (tables->rules.at(cast->desire).aboutSomething && !whole.has_value()))
	{
		return false;
	}
	// The globe counts as a sighting of its miracle, then the creature casts it
	CountMiracleSighting(creature, *body, mind, index);
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} casts the globe {} that it was given at {}",
	                    entt::to_integral(creature), entt::to_integral(globe), entt::to_integral(*target));
	ForceActivity(
	    creature,
	    {.desire = desire, .activityObject = whole, .action = cast->action, .actionObject = *target, .ordered = true});
	return true;
}

void CreatureMindSystem::ShowFizzle(entt::entity creature, CreatureMindState& mind)
{
	auto* casting = Entities().TryGet<CreatureCasting>(creature);
	if (casting == nullptr || !casting->fizzle.has_value() || !Locator::gameRandom::has_value())
	{
		return;
	}
	const auto fizzle = *casting->fizzle;
	casting->fizzle.reset();
	// Embarrassed, then one time in two sad as well, and sadder for it
	// The reaction draws from the random numbers of the game
	auto& gameRandom = Locator::gameRandom::value();
	auto reaction = creature_mind::ReactToFizzle([&gameRandom](uint32_t range) { return gameRandom.GameRand(range); },
	                                             [&gameRandom](float most) { return gameRandom.GameFloatRand(most); });
	if (reaction.sadness.has_value() && mind.desires.has_value())
	{
		creature_desires::ChangeSource(*mind.desires, creature_desires::sources::k_Sadness, *reaction.sadness);
	}
	if (!Replan(creature, creature_mind::Activity::Planned, std::move(reaction.agenda)))
	{
		return;
	}
	// It is showing how it feels about what it tried
	if (const auto* tables = GetTables())
	{
		if (const auto action = creature_mind_tables::FindAction(*tables, "CommunicateState"))
		{
			mind.planner.current = creature_planner::Plan {
			    .desire = Desire::ManifestState,
			    .action = *action,
			    .object = Entities().Valid(fizzle.target) ? std::optional(entt::to_integral(fizzle.target)) : std::nullopt};
			mind.planActive = true;
			mind.planSerial = mind.idle.serial;
			mind.agendaSeen = mind.idle.serial;
		}
	}
}

void CreatureMindSystem::StartSubMove(entt::entity creature, const creature_mind::Movement& movement, float seconds)
{
	auto& registry = Entities();
	auto* existing = registry.TryGet<CreatureCasting>(creature);
	auto& casting = existing != nullptr ? *existing : registry.Assign<CreatureCasting>(creature);
	casting.target = movement.object.has_value() ? static_cast<entt::entity>(*movement.object) : entt::null;
	casting.keep = movement.maxDistance;
	casting.settleSeconds = seconds;
	casting.run = movement.run;
	casting.phase = 0;
	casting.turns = 0;
	casting.holdTurns = 0;
	casting.destination.reset();
	casting.outcome = CreatureCasting::Outcome::Running;
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {}: {} {} keeping {:.1f}, its radius {:.2f}, the target's {:.2f}",
	                    entt::to_integral(creature), static_cast<int>(movement.kind), entt::to_integral(casting.target),
	                    casting.keep, object_measures::TwoDRadius(registry, creature),
	                    registry.Valid(casting.target) ? object_measures::RoutePlanRadius(registry, casting.target, creature)
	                                                   : 0.0f);
	switch (movement.kind)
	{
	case Kind::GoNearObject:
		casting.move = CreatureCasting::Move::GoNear;
		break;
	case Kind::GetAwayFromObject:
		casting.move = CreatureCasting::Move::GetAway;
		break;
	case Kind::TurnToFaceObject:
		casting.move = CreatureCasting::Move::TurnToFace;
		break;
	case Kind::TurnToFaceAwayFromObject:
		casting.move = CreatureCasting::Move::TurnAway;
		break;
	case Kind::ToThrowPosition:
		casting.move = CreatureCasting::Move::ToThrowPosition;
		break;
	default:
		casting.move = CreatureCasting::Move::None;
		return;
	}
	const auto* animation = registry.TryGet<const CreatureAnimation>(creature);
	StepSubMove(creature, animation != nullptr && creature_layers::IsPlaying(animation->body));
}

std::optional<creature_cast_moves::WalkStart> CreatureMindSystem::GoNear(entt::entity creature, CreatureCasting& casting,
                                                                         bool reissue)
{
	auto& registry = Entities();
	const auto at = object_measures::PositionOf(registry, casting.target);
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (!at.has_value() || transform == nullptr || !Locator::creatureLocomotionSystem::has_value())
	{
		return std::nullopt;
	}
	// It stops short of the thing by what it keeps clear of and its radius on its walks, and no further than the distance
	// more. In a temple's pen, this radius is of its own size, as the circle that its route goes round.
	const float clear =
	    object_measures::RoutePlanRadius(registry, casting.target, creature) + object_measures::NavRadius(registry, creature);
	const auto target = Flat(*at);
	if (reissue && casting.destination.has_value() && cast_moves::SameDestination(*casting.destination, target))
	{
		return creature_cast_moves::WalkStart::Walking;
	}
	const float distance = glm::distance(Flat(transform->position), target);
	const float near = std::min(clear, distance - 0.05f);
	const float far = std::max(casting.keep + clear, near + 0.001f);
	const auto result = Locator::creatureLocomotionSystem::value().MoveTo(creature, target, KeptOrRun(casting), near, far);
	using MoveResult = CreatureLocomotionSystemInterface::MoveResult;
	const auto start =
	    creature_cast_moves::WalkStartOf(result == MoveResult::Started, result != MoveResult::InvalidDestination);
	if (start == creature_cast_moves::WalkStart::Walking)
	{
		casting.destination = target;
	}
	return start;
}

void CreatureMindSystem::StepSubMove(entt::entity creature, bool animating)
{
	auto& registry = Entities();
	auto* casting = registry.TryGet<CreatureCasting>(creature);
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (casting == nullptr || casting->move == CreatureCasting::Move::None ||
	    casting->outcome != CreatureCasting::Outcome::Running || transform == nullptr ||
	    !Locator::creatureLocomotionSystem::has_value())
	{
		return;
	}
	auto& locomotion = Locator::creatureLocomotionSystem::value();
	const auto at = object_measures::PositionOf(registry, casting->target);
	const auto here = Flat(transform->position);
	// Playing an action, walking or turning: the game counts all as its body doing something
	const bool bodyBusy = animating || locomotion.IsMoving(creature);
	const auto fail = [&] { casting->outcome = CreatureCasting::Outcome::Failed; };
	const auto done = [&] { casting->outcome = CreatureCasting::Outcome::Done; };
	const bool start = casting->phase == 0;
	switch (casting->move)
	{
	case CreatureCasting::Move::GoNear:
	{
		const bool itself = at.has_value() && object_measures::WalkedUpToItself(registry, casting->target);
		if (start)
		{
			// A building or a tree it walks up to by its walk alone
			if (itself && GoNear(creature, *casting, false) != creature_cast_moves::WalkStart::Walking)
			{
				casting->destination.reset();
			}
			casting->stuckCheck = here;
			casting->phase = 1;
			casting->turns = 0;
			break;
		}
		++casting->turns;
		if (itself && !bodyBusy)
		{
			done();
			break;
		}
		const float routeRadius = at.has_value() ? object_measures::RoutePlanRadius(registry, casting->target, creature)
		                                         : object_measures::TwoDRadius(registry, creature);
		const auto target = at.has_value() ? Flat(*at) : here;
		const float selfRadius = object_measures::TwoDRadius(registry, creature);
		if (cast_moves::Arrived(gutils::GetDistanceInMetres(here, target), selfRadius, routeRadius, casting->keep))
		{
			locomotion.Stop(creature);
			done();
			break;
		}
		if (!locomotion.IsMoving(creature) || !cast_moves::HeadsFor(here, target, HeadingOf(registry, creature, *transform)))
		{
			const auto walk = GoNear(creature, *casting, locomotion.IsMoving(creature));
			// Where it cannot stand at the thing, it does not choose the thing again for a time. When it cannot walk
			// now, it asks again on the next turn.
			if (walk == creature_cast_moves::WalkStart::NoWay)
			{
				if (auto* mindState = registry.TryGet<CreatureMindState>(creature))
				{
					mindState->beliefs.MarkBlocked(casting->target, mindState->turn);
				}
			}
			if (!walk.has_value() || walk == creature_cast_moves::WalkStart::NoWay)
			{
				fail();
				break;
			}
		}
		// It gives up when it has got nowhere for a while, and wants what it went for less for a while. A creature
		// under the control of a script does not give up.
		auto* mindState = registry.TryGet<CreatureMindState>(creature);
		const uint32_t count = mindState != nullptr ? mindState->stepTurns : casting->turns;
		const auto stall =
		    cast_moves::CheckStall(count,
		                           gutils::GetDistanceInMetres(glm::vec3(here.x, 0.0f, here.y),
		                                                       glm::vec3(casting->stuckCheck.x, 0.0f, casting->stuckCheck.y)),
		                           registry.AllOf<ScriptControlled>(creature));
		if (stall == cast_moves::Stall::Note)
		{
			casting->stuckCheck = here;
		}
		else if (stall == cast_moves::Stall::GiveUp)
		{
			if (mindState != nullptr && mindState->desires.has_value() && mindState->planActive &&
			    mindState->planner.current.has_value())
			{
				creature_desires::Suppress(*mindState->desires, mindState->planner.current->desire,
				                           cast_moves::k_StallHoldBackSeconds, k_TurnsPerSecond);
			}
			fail();
		}
		break;
	}
	case CreatureCasting::Move::GetAway:
	{
		if (casting->phase == 0)
		{
			// Not while it plays an action or turns on the spot, only standing or walking
			const auto* moves = registry.TryGet<const CreatureLocomotion>(creature);
			if (animating || (moves != nullptr && moves->motion == CreatureLocomotion::Motion::Turning))
			{
				break;
			}
			if (!at.has_value())
			{
				done();
				break;
			}
			const float far =
			    cast_moves::GetAwayDistance(casting->keep, object_measures::TwoDRadius(registry, casting->target));
			// It gets away from where it believes the thing is: where it last came across the thing. Without a belief
			// about the thing, it uses where the thing is now.
			auto from = Flat(*at);
			if (const auto* mindState = registry.TryGet<const CreatureMindState>(creature); mindState != nullptr)
			{
				if (const auto* belief = mindState->beliefs.Find(casting->target); belief != nullptr)
				{
					from = map_coords::ToMetres(belief->place);
				}
			}
			const float apart = gutils::GetDistanceInMetres(from, here);
			if (!(apart < far))
			{
				done();
				break;
			}
			// The game works from the map positions of both, and the area it clears is about a map position too
			const auto away = cast_moves::GetAwayPoint(MapPoint(here), MapPoint(from), apart, far);
			const auto clear =
			    cast_moves::FindClearArea(map_coords::ToMetres(map_coords::FromMetres(Flat(away))),
			                              object_measures::Height(registry, creature), [&](int32_t x, int32_t z) {
				                              return locomotion.GetWalkableLand().At(x, z) == creature_route::Ground::Open &&
				                                     !CellBlocked(registry, creature, x, z);
			                              });
			if (!clear.has_value() || locomotion.MoveTo(creature, *clear, KeptOrRun(*casting), 0.0f, 1.0f) !=
			                              CreatureLocomotionSystemInterface::MoveResult::Started)
			{
				fail();
				break;
			}
			casting->phase = 1;
			break;
		}
		if (!bodyBusy)
		{
			done();
		}
		break;
	}
	case CreatureCasting::Move::ToThrowPosition:
	{
		if (casting->phase == 1)
		{
			if (!locomotion.IsMoving(creature))
			{
				done();
			}
			break;
		}
		if (!at.has_value())
		{
			fail();
			break;
		}
		// The game works from the map positions of the creature and of the thing
		const auto stand = creature_throw::WhereToThrowFrom(
		    MapPosition(here), MapPosition(Flat(*at)), object_measures::Height(registry, creature),
		    object_measures::TwoDRadius(registry, casting->target), casting->keep);
		if (!stand.has_value())
		{
			fail();
			break;
		}
		if (stand->kind == creature_throw::ThrowStand::Kind::There)
		{
			done();
			break;
		}
		auto point = stand->point;
		// Backing away, where it can't stand it goes to the nearest place it can, if near enough
		if (stand->kind == creature_throw::ThrowStand::Kind::BackOff)
		{
			const auto standPoint = creature_throw::BackOffPoint(locomotion.GetWalkableLand(), point);
			if (!standPoint.has_value())
			{
				fail();
				break;
			}
			point = *standPoint;
		}
		switch (locomotion.MoveTo(creature, point, KeptOrRun(*casting), stand->minDistance, stand->maxDistance))
		{
		case CreatureLocomotionSystemInterface::MoveResult::Started:
			casting->phase = 1;
			break;
		case CreatureLocomotionSystemInterface::MoveResult::Busy:
			// It tries again next turn
			break;
		case CreatureLocomotionSystemInterface::MoveResult::InvalidDestination:
			fail();
			break;
		}
		break;
	}
	case CreatureCasting::Move::TurnToFace:
	{
		if (!at.has_value())
		{
			done();
			break;
		}
		const auto target = Flat(*at);
		const auto heading = HeadingOf(registry, creature, *transform);
		const bool onTop = cast_moves::MapPointDistance(target, here) < cast_moves::k_OnTopDistance;
		if (casting->phase == 0)
		{
			// Right on top of it there is nothing to turn to, but it still settles as it would
			if (onTop)
			{
				casting->phase = 1;
				break;
			}
			// It stops and turns to the heading from its map position to the thing's
			locomotion.Stop(creature);
			if (locomotion.TurnToHeading(creature, cast_moves::HeadingTo(here, target)))
			{
				casting->phase = 1;
			}
			break;
		}
		if (bodyBusy)
		{
			break;
		}
		if (onTop && casting->phase == 1)
		{
			casting->phase = 2;
			break;
		}
		if (onTop)
		{
			done();
			break;
		}
		const bool facing = cast_moves::Facing(here, heading, target);
		if (casting->phase == 1)
		{
			if (!facing)
			{
				locomotion.TurnToHeading(creature, cast_moves::HeadingTo(here, target));
				break;
			}
			casting->holdTurns = static_cast<uint32_t>(casting->settleSeconds * k_TurnsPerSecond);
			casting->phase = 2;
			break;
		}
		if (casting->holdTurns == 0)
		{
			done();
			break;
		}
		--casting->holdTurns;
		if (!facing)
		{
			locomotion.TurnToHeading(creature, cast_moves::HeadingTo(here, target));
		}
		break;
	}
	case CreatureCasting::Move::TurnAway:
	{
		if (casting->phase == 0)
		{
			// Right on top of the thing, or with the thing gone, it does not turn
			if (!at.has_value() || cast_moves::MapPointDistance(Flat(*at), here) < cast_moves::k_OnTopDistance)
			{
				casting->phase = 1;
				break;
			}
			// It stops and starts to turn. If it cannot turn yet, it tries again on the next turn
			locomotion.Stop(creature);
			if (locomotion.TurnToHeading(creature, cast_moves::HeadingAwayFrom(here, Flat(*at))))
			{
				casting->phase = 1;
			}
			break;
		}
		// It is done when its body does nothing more
		if (!bodyBusy)
		{
			done();
		}
		break;
	}
	case CreatureCasting::Move::None:
		break;
	}
}

creature_mind::SubMove CreatureMindSystem::SubMoveOf(entt::entity creature)
{
	// A step that draws a gesture goes as the drawing goes
	const auto* mind = Entities().TryGet<const CreatureMindState>(creature);
	const auto* drawing = Entities().TryGet<const CreatureGestureDrawing>(creature);
	if (mind != nullptr && drawing != nullptr && !drawing->inDance && mind->idle.step < mind->idle.agenda.size() &&
	    mind->idle.agenda[mind->idle.step].kind == creature_mind::Step::Kind::Gesture)
	{
		switch (drawing->outcome)
		{
		case CreatureGestureDrawing::Outcome::Done:
			return creature_mind::SubMove::Done;
		case CreatureGestureDrawing::Outcome::Failed:
			return creature_mind::SubMove::Failed;
		case CreatureGestureDrawing::Outcome::Running:
			break;
		}
		return creature_mind::SubMove::Running;
	}
	const auto* casting = Entities().TryGet<const CreatureCasting>(creature);
	if (casting == nullptr || casting->move == CreatureCasting::Move::None)
	{
		return creature_mind::SubMove::Running;
	}
	switch (casting->outcome)
	{
	case CreatureCasting::Outcome::Done:
		return creature_mind::SubMove::Done;
	case CreatureCasting::Outcome::Failed:
		return creature_mind::SubMove::Failed;
	case CreatureCasting::Outcome::Running:
		break;
	}
	return creature_mind::SubMove::Running;
}

void CreatureMindSystem::KnowMiracle(entt::entity creature, size_t miracle)
{
	auto* mind = Entities().TryGet<CreatureMindState>(creature);
	if (mind == nullptr || !mind->learnt.has_value() || miracle >= mind->learnt->knowledge.miraclesKnown.size())
	{
		return;
	}
	// As if it had seen it as often as it needs to learn it
	const auto* body = Entities().TryGet<const Creature>(creature);
	const auto* tables = GetTables();
	if (body != nullptr && tables != nullptr && miracle < tables->miracles.size())
	{
		const auto needed = creature_watching::TimesNeeded(
		    tables->miracles[miracle].timesToSee, creature_mind_tables::MiracleMultiplier(creature::InfoRow(body->species)));
		auto& seen = mind->learnt->knowledge.miraclesSeen[miracle].count;
		seen = std::max(seen, static_cast<uint32_t>(std::ceil(needed)));
	}
	mind->learnt->knowledge.miraclesKnown[miracle] = true;
}

void CreatureMindSystem::SetKnowsAction(entt::entity creature, CreatureActionLearningType type, uint32_t action, bool knows)
{
	auto* mind = Entities().TryGet<CreatureMindState>(creature);
	if (mind == nullptr)
	{
		return;
	}
	// Taught before its learning is set up, or with a mind file still to take up, it is taught once it has those
	if (!mind->learnt.has_value() || mind->pendingFile != nullptr)
	{
		mind->pendingTeaching.push_back({.type = type, .action = action, .knows = knows});
		return;
	}
	if (type != CreatureActionLearningType::Normal && type != CreatureActionLearningType::Magic)
	{
		return;
	}
	const auto list =
	    type == CreatureActionLearningType::Magic ? creature_watching::KnownList::Miracle : creature_watching::KnownList::Skill;
	uint32_t sightings = 0;
	const auto* body = Entities().TryGet<const Creature>(creature);
	const auto* tables = GetTables();
	if (list == creature_watching::KnownList::Miracle && body != nullptr && tables != nullptr &&
	    action < tables->miracles.size())
	{
		sightings = creature_watching::TaughtSightings(
		    tables->miracles[action].timesToSee, creature_mind_tables::MiracleMultiplier(creature::InfoRow(body->species)));
	}
	const bool newlyKnown = creature_watching::SetKnown(mind->learnt->knowledge, list, action, knows, sightings);
	// A skill or miracle it did not know before shows the player the creature's lesson
	if (knows && newlyKnown && Locator::creatureHelpSystem::has_value())
	{
		const auto kind = list == creature_watching::KnownList::Miracle ? creature_help::Kind::LessonMagicAction
		                                                                : creature_help::Kind::LessonNormalAction;
		Locator::creatureHelpSystem::value().Raise(creature, {.kind = kind, .parameter = action});
	}
}

void CreatureMindSystem::LearnAction(entt::entity creature, CreatureActionLearningType type, uint32_t action)
{
	auto* mind = Entities().TryGet<CreatureMindState>(creature);
	if (mind == nullptr || !mind->learnt.has_value() ||
	    (type != CreatureActionLearningType::Normal && type != CreatureActionLearningType::Magic))
	{
		return;
	}
	auto& known =
	    type == CreatureActionLearningType::Magic ? mind->learnt->knowledge.miraclesKnown : mind->learnt->knowledge.skillsKnown;
	if (action >= known.size() || known[action])
	{
		return;
	}
	known[action] = true;
	if (Locator::creatureHelpSystem::has_value())
	{
		const auto kind = type == CreatureActionLearningType::Magic ? creature_help::Kind::LessonMagicAction
		                                                            : creature_help::Kind::LessonNormalAction;
		Locator::creatureHelpSystem::value().Raise(creature, {.kind = kind, .parameter = action});
	}
}

bool CreatureMindSystem::KnowsAction(entt::entity creature, CreatureActionLearningType type, uint32_t action) const
{
	const auto* mind = Entities().TryGet<const CreatureMindState>(creature);
	if (mind == nullptr || !mind->learnt.has_value() ||
	    (type != CreatureActionLearningType::Normal && type != CreatureActionLearningType::Magic))
	{
		return false;
	}
	const auto list =
	    type == CreatureActionLearningType::Magic ? creature_watching::KnownList::Miracle : creature_watching::KnownList::Skill;
	return creature_watching::IsKnown(mind->learnt->knowledge, list, action);
}

bool CreatureMindSystem::TellCast(entt::entity creature, MagicType type, entt::entity target)
{
	auto& registry = Entities();
	auto* mind = registry.TryGet<CreatureMindState>(creature);
	const auto* tables = GetTables();
	if (mind == nullptr || tables == nullptr || !registry.Valid(target))
	{
		return false;
	}
	for (uint32_t action = 0; action < tables->actions.size(); ++action)
	{
		const auto* executor = creature_plan_actions::For(tables->actions[action].name);
		if (executor == nullptr || !creature_plan_actions::IsCast(*executor) ||
		    CastMagicOf(action) != static_cast<uint32_t>(type) ||
		    tables->actions[action].desire >= creature_desires::k_DesireCount)
		{
			continue;
		}
		// Told by the debug tools, it goes about the casting as it would by itself, without weighing other plans
		auto cast = CastInfoFor(creature, action, target);
		auto agenda = creature_plan_actions::Agenda(
		    *executor, entt::to_integral(target),
		    object_measures::PositionOf(registry, target).has_value() ? Flat(*object_measures::PositionOf(registry, target))
		                                                              : glm::vec2(0.0f),
		    creature_plan_actions::Situation {.activity = entt::to_integral(target), .self = entt::to_integral(creature)},
		    [this](uint32_t range) { return Random(range); }, cast);
		return agenda.has_value() && Replan(creature, creature_mind::Activity::Told, std::move(*agenda));
	}
	return false;
}

bool mind_detail::CanPayForMiracle(entt::entity creature, MagicType type)
{
	return CanPayFor(creature, type);
}
