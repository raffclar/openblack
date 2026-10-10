/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CreatureFightSystem.h"

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <chrono>
#include <limits>
#include <vector>

#include <LNDFile.h>
#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <glm/mat3x3.hpp>

#include "3D/CreatureBody.h"
#include "3D/L3DMesh.h"
#include "3D/LandAvoid.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Audio/AudioManagerInterface.h"
#include "Camera/Camera.h"
#include "Camera/CameraModel.h"
#include "Camera/FightCameraModel.h"
#include "Creature/CreatureArena.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureFizz.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLocomotion.h"
#include "Creature/CreatureMarks.h"
#include "Creature/CreatureMode.h"
#include "Creature/CreatureRig.h"
#include "ECS/Archetypes/ArenaArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureArena.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreatureHome.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureFizzSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Particles/LightSheet.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using openblack::creature::CreatureRig;
namespace fight = openblack::creature_fight;
namespace feedback = openblack::creature_feedback;
using Pace = CreatureLocomotionSystemInterface::Pace;

namespace
{
constexpr float k_TurnSeconds = std::chrono::duration<float>(TimeSystemInterface::k_TurnDuration).count();
constexpr float k_TurnMs = k_TurnSeconds * 1000.0f;
constexpr auto k_TurnMilliseconds =
    static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(TimeSystemInterface::k_TurnDuration).count());
/// Fighters turn to face their opponents this fast, in radians a second
constexpr float k_FaceTurnRate = 3.0f;
/// A blow lands on anything within this many units of its size of where it reaches, twice that for the special move
constexpr float k_BlowReachPerSize = 0.2f * 15.0f;
/// Each blow is measured at this many moments through it, for its two striking bones, the hand and the foot
constexpr int k_MeasureSamples = 24;
/// A face is due again once its wait is within this much of over
constexpr float k_FaceDue = 1e-3f;
/// The stages before the duel give up waiting after this long
constexpr float k_ApproachSeconds = 30.0f;
constexpr float k_PooApproachSeconds = 20.0f;
/// A press on the ground counts as a step within this many of the arena's radii of its middle
constexpr float k_GroundClickRadii = 1.5f;
/// A blow leaves a trail of this many drops of blood down the skin
constexpr int k_BloodDrops = 12;
/// Wounds are placed at random on the skin this far from its edges
constexpr int k_WoundEdge = 16;

float HeightOf(float size)
{
	return feedback::k_HeightAtSizeOne * std::max(size, 0.05f);
}

glm::vec2 Flat(const glm::vec3& point)
{
	return {point.x, point.z};
}

float GroundAt(glm::vec2 point)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(point) : 0.0f;
}

float LifeOf(const ecs::Registry& registry, entt::entity creature)
{
	const auto* needs = registry.TryGet<const CreatureNeeds>(creature);
	return needs != nullptr ? needs->needs.life : 1.0f;
}

const CreatureRig* RigOf(CreatureType species)
{
	auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto id = creature::GetRigId(species);
	return rigs.Contains(id) ? &*rigs.Handle(id) : nullptr;
}

/// How far one of the species' animations carries the root, in its mesh's units, ahead being -z
glm::vec2 DisplacementOf(CreatureType species, size_t animation)
{
	const auto* rig = RigOf(species);
	const auto* played = rig != nullptr ? rig->GetAnimation(CreatureRig::Mesh::Base, animation) : nullptr;
	return played != nullptr && !played->frames.empty() ? glm::vec2(played->displacement.x, played->displacement.z)
	                                                    : glm::vec2(0.0f);
}

float DurationOf(entt::entity creature, size_t animation)
{
	if (!Locator::creatureAnimationSystem::has_value())
	{
		return 0.0f;
	}
	return Locator::creatureAnimationSystem::value().AnimationDuration(creature, animation).value_or(0.0f);
}

bool Has(entt::entity creature, size_t animation)
{
	return DurationOf(creature, animation) > 0.0f;
}

/// A point in the mesh's space of a creature facing a heading, in the world's directions
glm::vec2 MeshToWorld(glm::vec2 mesh, float heading)
{
	const auto turned = glm::mat3(glm::eulerAngleY(heading)) * glm::vec3(mesh.x, 0.0f, mesh.y);
	return {turned.x, turned.z};
}

/// A direction in the world as seen by a creature facing a heading: x to its right, y ahead
glm::vec2 WorldToLocal(glm::vec2 world, float heading)
{
	const auto ahead = creature_locomotion::DirectionOf(heading);
	const glm::vec2 right {-ahead.y, ahead.x};
	return {glm::dot(world, right), glm::dot(world, ahead)};
}

/// Moves a creature where it stands, both where it is drawn from and to this turn
void Shift(CreatureLocomotion& locomotion, glm::vec2 by)
{
	for (auto* point : {&locomotion.fromPosition, &locomotion.toPosition})
	{
		point->x += by.x;
		point->z += by.y;
		point->y = GroundAt(Flat(*point));
	}
}

void PlaceAt(CreatureLocomotion& locomotion, Transform& transform, glm::vec3 at)
{
	at.y = GroundAt(Flat(at));
	locomotion.fromPosition = at;
	locomotion.toPosition = at;
	transform.position = at;
}

/// Turns a creature towards a heading by at most an angle, at once
void TurnTowards(CreatureLocomotion& locomotion, float heading, float maxRadians)
{
	const auto difference = creature_locomotion::WrapAngle(heading - locomotion.heading);
	const auto turned = locomotion.heading + std::clamp(difference, -maxRadians, maxRadians);
	locomotion.heading = turned;
	locomotion.targetHeading = turned;
	locomotion.fromHeading = turned;
	locomotion.toHeading = turned;
}

/// The body plays a fight animation at the fighter's time, looping until the fight moves it on. The fight alone moves
/// the time on, at the move's own speed, so that the body shows exactly where the move is: blows land and sound as drawn,
/// whatever the frame rate.
void Show(CreatureAnimation& animation, const fight::Fighter& fighter, bool poweringUp, float durationMs)
{
	const auto played = fighter.state == fight::State::Stance && poweringUp ? fight::animations::k_PowerUp : fighter.animation;
	animation.body = {
	    .kind = creature_layers::BodyAction::Kind::Sequence,
	    .phase = creature_layers::BodyAction::Phase::Loop,
	    .animations = {played, played, played},
	    .timeMs = std::clamp(fighter.timeMs, 0.0f, std::max(durationMs - 1.0f, 0.0f)),
	    .mirrored = fighter.mirrored,
	    .holdLoop = fighter.state == fight::State::Lying,
	    .timedByPlayer = true,
	};
	animation.slots.clear();
}

void PlayOnce(CreatureAnimation& animation, size_t played)
{
	animation.body = {};
	if (const auto once = creature_layers::PlayOnce(animation.body, played, false))
	{
		animation.body = *once;
	}
}

void SetEyes(ecs::Registry& registry, entt::entity creature, creature_eyes::Mode mode)
{
	if (auto* eyes = registry.TryGet<CreatureEyes>(creature))
	{
		eyes->mode = mode;
	}
}

std::vector<feedback::Capsule> BodyOf(const ecs::Registry& registry, entt::entity creature)
{
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* animation = registry.TryGet<const CreatureAnimation>(creature);
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (body == nullptr || animation == nullptr || transform == nullptr)
	{
		return {};
	}
	return feedback::BodyCapsules(animation->skeleton.parents, animation->boneMatrices,
	                              creature::PlacementMatrix(transform->position, transform->rotation, transform->scale),
	                              feedback::k_BodyRadiusShare * HeightOf(body->size));
}

/// Where a creature is carried when its player has no temple and it passed out outside a fight, as on the testbed: the
/// middle of the land, where the testbed puts its creatures
std::optional<glm::vec3> NoPen()
{
	if (!Locator::terrainSystem::has_value())
	{
		return std::nullopt;
	}
	const auto& land = Locator::terrainSystem::value();
	const auto middle = (land.GetExtent().minimum + land.GetExtent().maximum) * 0.5f;
	return glm::vec3(middle.x, land.GetHeightAt(middle), middle.y);
}

/// Sets a creature fizzing out of sight or back in
void SetFizz(entt::entity creature, float target, float seconds)
{
	if (Locator::creatureFizzSystem::has_value())
	{
		Locator::creatureFizzSystem::value().SetFizz(creature, target, seconds, false);
	}
}

/// Carried home and stopped part way, a creature is back in full sight at once
void StopCarryingHome(entt::entity creature, const CreatureKnockedOut& knockedOut)
{
	if (knockedOut.stage == CreatureKnockedOut::Stage::FadingOut || knockedOut.stage == CreatureKnockedOut::Stage::FadingIn)
	{
		SetFizz(creature, 0.0f, 0.0f);
	}
}

/// Whether a fighter faces its opponent closely enough to make a move
bool FacesOpponent(const ecs::Registry& registry, entt::entity creature, entt::entity opponent)
{
	const auto* locomotion = registry.TryGet<const CreatureLocomotion>(creature);
	if (locomotion == nullptr || !registry.Valid(opponent) || !registry.AllOf<Transform>(opponent))
	{
		return true;
	}
	const auto towards = Flat(registry.Get<const Transform>(opponent).position) - Flat(locomotion->toPosition);
	return glm::length(towards) <= 0.0f ||
	       std::abs(creature_locomotion::WrapAngle(creature_locomotion::HeadingOf(towards) - locomotion->heading)) <=
	           fight::k_FacingTolerance;
}

/// Who is in a duel with whom, both of them still at it
/// The player at this computer
constexpr PlayerNames k_LocalPlayer = PlayerNames::PLAYER_ONE;

/// A player's creature for every purpose of the game: the first one they got. Creatures are made as they are got, so
/// it is the earliest made of theirs. This stands in for the game-wide lookup of a player's creature until there is one.
std::optional<entt::entity> PrimaryCreatureOf(const ecs::Registry& registry, PlayerNames player)
{
	std::optional<entt::entity> first;
	registry.Each<const Creature>([&first, player](entt::entity entity, const Creature& creature) {
		if (creature.owner == player && (!first.has_value() || entt::to_entity(entity) < entt::to_entity(*first)))
		{
			first = entity;
		}
	});
	return first;
}

bool InDuel(const ecs::Registry& registry, entt::entity creature)
{
	const auto* fighting = registry.TryGet<const CreatureFighting>(creature);
	return fighting != nullptr && fighting->stage == CreatureFighting::Stage::Duel;
}

namespace arena = openblack::creature_arena;

/// The sound of a fight taking its arena
constexpr auto k_ArenaDrawnSound = entt::hashed_string("InGame.sad/176");

/// The species' running share of its top speed, as the game's tables have it
float RunShareOf(CreatureType species)
{
	if (Locator::infoConstants::has_value())
	{
		const auto& creatures = Locator::infoConstants::value().creature;
		const auto row = creature::InfoRow(species);
		if (row < creatures.size())
		{
			return creatures.at(row).runSpeed;
		}
	}
	return 0.0f;
}

/// Where creatures may go on the land, worked out the first time it is asked for on a land
const land_avoid::Map& LandAvoidOf(ecs::Registry& registry)
{
	auto& context = registry.Context();
	if (!context.landAvoid.has_value())
	{
		if (!Locator::terrainSystem::has_value())
		{
			context.landAvoid.emplace();
			return *context.landAvoid;
		}
		const auto& island = Locator::terrainSystem::value();
		const auto find = [&island](glm::ivec2 cell) {
			return island.FindCell({static_cast<uint16_t>(cell.x), static_cast<uint16_t>(cell.y)});
		};
		context.landAvoid = land_avoid::Build(
		    LandIslandInterface::k_MapCellsPerSide,
		    [&find](glm::ivec2 corner) {
			    const auto* cell = find(corner);
			    return cell != nullptr ? cell->altitude : uint8_t {0};
		    },
		    [&find](glm::ivec2 cell) {
			    const auto* found = find(cell);
			    return found == nullptr || found->properties.hasWater != 0;
		    });
	}
	return *context.landAvoid;
}

/// Whether an arena may lie over a land cell: one on the map, of land without water, that creatures can walk to
bool UsableForArena(glm::ivec2 cell, const land_avoid::Map& avoid)
{
	constexpr auto k_Side = LandIslandInterface::k_MapCellsPerSide;
	if (cell.x < 0 || cell.y < 0 || cell.x >= k_Side || cell.y >= k_Side || !Locator::terrainSystem::has_value())
	{
		return false;
	}
	const auto* land = Locator::terrainSystem::value().FindCell({static_cast<uint16_t>(cell.x), static_cast<uint16_t>(cell.y)});
	if (land == nullptr || land->properties.hasWater != 0)
	{
		return false;
	}
	// TODO(creature-fights): the things standing in the cell, as the game checks them against the creature
	return avoid.At(cell) == land_avoid::k_Land;
}

/// The arena two creatures about to fight take: the nearest one near enough to the middle between them, shrunk to the
/// size wanted, or a new one for this fight where there is room
std::optional<entt::entity> TakeArena(ecs::Registry& registry, entt::entity creature, entt::entity opponent)
{
	const auto& body = registry.Get<const Creature>(creature);
	const auto& from = registry.Get<const Transform>(creature).position;
	const auto& to = registry.Get<const Transform>(opponent).position;
	// The middle in map units, as the game works it out without rounding on the way
	const auto middle = [](float a, float b) {
		return static_cast<int32_t>((static_cast<double>(a) + static_cast<double>(b)) * 0.5 *
		                            static_cast<double>(map_coords::k_FixedPerMetre));
	};
	const glm::ivec2 start {middle(from.x, to.x), middle(from.z, to.z)};
	const float wanted = fight::ArenaRadius(body.size, registry.Get<const Creature>(opponent).size);

	std::vector<entt::entity> entities;
	std::vector<glm::ivec2> places;
	registry.Each<const CreatureArena>([&](entt::entity entity, const CreatureArena& found) {
		entities.push_back(entity);
		places.push_back(found.place);
	});
	if (const auto nearest = arena::Nearest(places, start, arena::ReuseDistance(RunShareOf(body.species), body.size)))
	{
		const auto entity = entities.at(*nearest);
		auto& taken = registry.Get<CreatureArena>(entity);
		taken.radius = std::min(taken.radius, wanted);
		taken.temporary = false;
		return entity;
	}
	const auto& avoid = LandAvoidOf(registry);
	const auto placed = arena::Place(start, wanted, [&avoid](glm::ivec2 cell) { return UsableForArena(cell, avoid); });
	if (!placed.has_value())
	{
		return std::nullopt;
	}
	return ecs::archetypes::ArenaArchetype::Create(placed->centre, placed->radius, true);
}

/// A fight is on in the arena: its ring of light stands and its sound plays
void StartArenaFight(ecs::Registry& registry, entt::entity entity, entt::entity first, entt::entity second)
{
	auto& taken = registry.Get<CreatureArena>(entity);
	taken.fightOn = true;
	taken.first = first;
	taken.second = second;
	if (taken.ring != nullptr)
	{
		taken.ring->SetHidden(false);
	}
	if (Locator::audio::has_value())
	{
		Locator::audio::value().PlaySoundEffect(k_ArenaDrawnSound.value(), registry.Get<const Transform>(entity).position);
	}
}

/// The creature that took an arena leaves its fight: an arena made for the fight goes, any other is free again
void ReleaseArena(ecs::Registry& registry, entt::entity entity)
{
	if (!registry.Valid(entity))
	{
		return;
	}
	auto* taken = registry.TryGet<CreatureArena>(entity);
	if (taken == nullptr)
	{
		return;
	}
	if (taken->temporary)
	{
		registry.Destroy(entity);
		return;
	}
	taken->fightOn = false;
	taken->first = entt::null;
	taken->second = entt::null;
	if (taken->ring != nullptr)
	{
		taken->ring->SetHidden(true);
	}
}
} // namespace

CreatureFightSystemInterface::StartResult CreatureFightSystem::StartFight(entt::entity creature, entt::entity opponent)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto ready = [&registry](entt::entity entity) {
		return registry.Valid(entity) && registry.AllOf<Creature, Transform, CreatureAnimation, CreatureLocomotion>(entity);
	};
	if (creature == opponent || !ready(creature) || !ready(opponent))
	{
		return StartResult::NoOpponent;
	}
	if (registry.AnyOf<CreatureFighting, CreatureKnockedOut>(creature) ||
	    registry.AnyOf<CreatureFighting, CreatureKnockedOut>(opponent))
	{
		return StartResult::Busy;
	}
	if (!fight::HealthyEnoughToFight(LifeOf(registry, creature)))
	{
		return StartResult::TooWeak;
	}
	const auto arenaEntity = TakeArena(registry, creature, opponent);
	if (!arenaEntity.has_value())
	{
		return StartResult::NoArena;
	}
	const auto& taken = registry.Get<const CreatureArena>(*arenaEntity);
	const fight::Arena arena {.centre = {map_coords::ToMetres(taken.place.x), map_coords::ToMetres(taken.place.y)},
	                          .radius = taken.radius};

	for (const auto& [self, other] : {std::pair(creature, opponent), std::pair(opponent, creature)})
	{
		const auto& body = registry.Get<const Creature>(self);
		if (Locator::creatureLocomotionSystem::has_value())
		{
			Locator::creatureLocomotionSystem::value().Stop(self);
		}
		if (Locator::creatureObjectActionSystem::has_value())
		{
			Locator::creatureObjectActionSystem::value().Cancel(self);
			Locator::creatureObjectActionSystem::value().Drop(self);
		}
		registry.Get<CreatureAnimation>(self).body = {};

		auto& record = registry.AllOf<CreatureFightRecord>(self) ? registry.Get<CreatureFightRecord>(self)
		                                                         : registry.Assign<CreatureFightRecord>(self);
		++record.fights;
		const auto* needs = registry.TryGet<const CreatureNeeds>(self);
		const auto bodyNeeds = needs != nullptr ? needs->needs : creature_physiology::Needs {};

		CreatureFighting fighting {
		    .stage = CreatureFighting::Stage::Approach,
		    .opponent = other,
		    .arena = arena,
		    .madeArena = self == creature,
		    .arenaEntity = *arenaEntity,
		    .startPosition = registry.Get<const Transform>(self).position,
		};
		auto& fighter = fighting.fighter;
		fighter.health = fight::FightHealthAtStart(bodyNeeds.life);
		fighter.stamina = fight::StaminaAtStart(bodyNeeds.energy, bodyNeeds.exhaustion);
		// The human player directs their own creature; any other fights by itself
		fighter.control = body.owner == k_LocalPlayer ? fight::Control::Player : fight::Control::Computer;
		fighter.computerWaitMs = fight::k_ComputerWaitsAtStartMs;
		fighter.tendency = record.foughtBefore ? record.tendency : fight::FirstTendency(body.alignment);
		registry.AssignOrReplace<CreatureFighting>(self, std::move(fighting));
	}
	StartArenaFight(registry, *arenaEntity, creature, opponent);

	return StartResult::Started;
}

namespace
{
/// A fight's arena is on from when the fight starts until the duel is over
bool ArenaOn(const ecs::Registry& registry, entt::entity creature)
{
	const auto* fighting = registry.Valid(creature) ? registry.TryGet<const CreatureFighting>(creature) : nullptr;
	return fighting != nullptr && fighting->stage <= CreatureFighting::Stage::Duel;
}

fight_view::Fighter ViewedFighter(const ecs::Registry& registry, entt::entity creature)
{
	const auto* locomotion = registry.TryGet<const CreatureLocomotion>(creature);
	return {.position = registry.Get<const Transform>(creature).position,
	        .radius = locomotion != nullptr ? locomotion->radius : 5.0f,
	        .height = HeightOf(registry.Get<const Creature>(creature).size)};
}
} // namespace

void CreatureFightSystem::Reset()
{
	_pressed.reset();
	EndView();
	_lookSeconds = 0.0f;
	_fightExit = true;
}

void CreatureFightSystem::UpdateView(float seconds)
{
	if (!Locator::camera::has_value())
	{
		return;
	}
	auto& camera = Locator::camera::value();
	const auto& registry = Locator::entitiesRegistry::value();
	if (_view.has_value())
	{
		auto* model = dynamic_cast<FightCameraModel*>(&camera.GetModel());
		if (model == nullptr)
		{
			_view.reset();
			return;
		}
		_lookSeconds = 0.0f;
		if (_view->lingerSeconds.has_value())
		{
			*_view->lingerSeconds -= seconds;
			if (*_view->lingerSeconds < 0.0f)
			{
				EndView();
			}
			return;
		}
		if (ArenaOn(registry, _view->first) && ArenaOn(registry, _view->second))
		{
			model->SetFighters(ViewedFighter(registry, _view->first), ViewedFighter(registry, _view->second));
			// Zoomed far enough out, the player leaves the fight, if they may
			if (model->ZoomedOut() && _fightExit)
			{
				EndView();
			}
			return;
		}
		// Once the fight is over the view lingers a little, if it may end by itself
		if (_fightExit)
		{
			_view->lingerSeconds = fight_view::k_EndSoonSeconds;
		}
		return;
	}
	if (!_cameraWatches)
	{
		return;
	}
	// Looking at an arena with a fight on from within it for a while starts the fight view
	const auto eye = camera.GetOrigin(Camera::Interpolation::Target);
	const auto looking = camera.GetFocus(Camera::Interpolation::Target);
	std::optional<std::pair<entt::entity, entt::entity>> found;
	fight::Arena foundArena;
	registry.Each<const CreatureFighting>([&](entt::entity entity, const CreatureFighting& fighting) {
		if (found.has_value() || !fighting.madeArena || !ArenaOn(registry, entity) || !ArenaOn(registry, fighting.opponent))
		{
			return;
		}
		if (fight_view::WithinArena({eye.x, eye.z}, {looking.x, looking.z}, fighting.arena.centre, fighting.arena.radius))
		{
			found = std::pair(entity, fighting.opponent);
			foundArena = fighting.arena;
		}
	});
	if (!found.has_value())
	{
		return;
	}
	_lookSeconds += seconds;
	if (_lookSeconds > fight_view::k_LookSeconds)
	{
		TryStartView(found->first, found->second, foundArena);
	}
}

void CreatureFightSystem::TryStartView(entt::entity first, entt::entity second, const fight::Arena& arena)
{
	auto& camera = Locator::camera::value();
	const auto& registry = Locator::entitiesRegistry::value();
	const auto own = PrimaryCreatureOf(registry, k_LocalPlayer);
	const bool involved = own == first || own == second;
	const bool scripted = registry.AnyOf<ScriptControlled>(first) || registry.AnyOf<ScriptControlled>(second);
	const auto eye = camera.GetOrigin(Camera::Interpolation::Target);
	const auto looking = camera.GetFocus(Camera::Interpolation::Target);
	// The player's creature's fight under a script's control is always watched; any other only from near enough
	if (!(involved && scripted) && fight_view::TooFar({eye.x, eye.z}, {looking.x, looking.z}, arena.centre, arena.radius, 1.0f))
	{
		// TODO(advisors): for the player's creature, the advisors say one of their lines about its fight instead
		return;
	}
	const auto ground = GroundAt(arena.centre);
	const auto normal = Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetNormalAt(arena.centre)
	                                                        : glm::vec3(0.0f, 1.0f, 0.0f);
	const auto start = fight_view::Start(arena.centre, arena.radius, ground, GroundAt, normal);
	auto model = std::make_unique<FightCameraModel>(nullptr, start);
	auto* view = model.get();
	view->SetPlayerModel(camera.SetModel(std::move(model)));
	_view = Watched {.first = first, .second = second, .lingerSeconds = std::nullopt};
	_lookSeconds = 0.0f;
}

void CreatureFightSystem::EndView()
{
	_view.reset();
	if (!Locator::camera::has_value())
	{
		return;
	}
	auto& camera = Locator::camera::value();
	if (auto* model = dynamic_cast<FightCameraModel*>(&camera.GetModel()))
	{
		if (auto player = model->TakePlayerModel())
		{
			camera.SetModel(std::move(player));
		}
	}
}

void CreatureFightSystem::AbortFight(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* fighting = registry.TryGet<CreatureFighting>(creature);
	if (fighting == nullptr)
	{
		return;
	}
	const auto opponent = fighting->opponent;
	for (const auto self : {creature, opponent})
	{
		auto* side = registry.Valid(self) ? registry.TryGet<CreatureFighting>(self) : nullptr;
		if (side == nullptr)
		{
			continue;
		}
		const auto* other = registry.Valid(side->opponent) ? registry.TryGet<const CreatureFighting>(side->opponent) : nullptr;
		if (side->stage == CreatureFighting::Stage::Duel)
		{
			EndFightFor(self, other == nullptr || side->fighter.health >= other->fighter.health);
			side->stage = CreatureFighting::Stage::Respond;
			side->played = false;
			fight::Enter(side->fighter, fight::State::Finish);
		}
		else if (side->stage == CreatureFighting::Stage::Approach || side->stage == CreatureFighting::Stage::Taunt ||
		         side->stage == CreatureFighting::Stage::Ready)
		{
			Leave(self);
		}
	}
}

void CreatureFightSystem::Withdraw(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* fighting = registry.TryGet<const CreatureFighting>(creature);
	if (fighting == nullptr)
	{
		return;
	}
	const auto opponent = fighting->opponent;
	const auto arena = fighting->arenaEntity;
	AbortFight(creature);
	// It leaves at once, rather than finishing and standing, and lets go of an arena it made
	Leave(creature);
	// What it fought and where forget it: the opponent plays out the end of its fight alone
	if (auto* other = registry.Valid(opponent) ? registry.TryGet<CreatureFighting>(opponent) : nullptr;
	    other != nullptr && other->opponent == creature)
	{
		other->opponent = entt::null;
	}
	if (auto* taken = registry.Valid(arena) ? registry.TryGet<CreatureArena>(arena) : nullptr; taken != nullptr)
	{
		taken->first = taken->first == creature ? entt::null : taken->first;
		taken->second = taken->second == creature ? entt::null : taken->second;
	}
	if (_view.has_value() && (_view->first == creature || _view->second == creature))
	{
		EndView();
	}
}

bool CreatureFightSystem::IsFighting(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(creature) && registry.AllOf<CreatureFighting>(creature);
}

std::optional<entt::entity> CreatureFightSystem::OpponentOf(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* fighting = registry.Valid(creature) ? registry.TryGet<const CreatureFighting>(creature) : nullptr;
	return fighting != nullptr ? std::optional(fighting->opponent) : std::nullopt;
}

bool CreatureFightSystem::QueueMove(entt::entity creature, const fight::Move& move, bool replace)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* fighting = registry.Valid(creature) ? registry.TryGet<CreatureFighting>(creature) : nullptr;
	if (fighting == nullptr)
	{
		return false;
	}
	const bool queued = fight::PlayerMove(fighting->fighter, move, replace);
	if (fighting->stage == CreatureFighting::Stage::Duel)
	{
		CheckQueue(creature);
	}
	return queued;
}

void CreatureFightSystem::ReleaseCharge(entt::entity creature, float heldMs)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* fighting = registry.Valid(creature) ? registry.TryGet<CreatureFighting>(creature) : nullptr)
	{
		fighting->fighter.queue.Release(heldMs);
		if (fighting->stage == CreatureFighting::Stage::Duel)
		{
			CheckQueue(creature);
		}
	}
}

void CreatureFightSystem::SetAutoFighting(entt::entity creature, bool autoFight)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* fighting = registry.Valid(creature) ? registry.TryGet<CreatureFighting>(creature) : nullptr)
	{
		fighting->fighter.autoFight = autoFight;
		if (autoFight)
		{
			fighting->fighter.control = fight::Control::Computer;
		}
	}
}

bool CreatureFightSystem::IsAutoFighting(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* fighting = registry.Valid(creature) ? registry.TryGet<const CreatureFighting>(creature) : nullptr;
	return fighting != nullptr && (fighting->fighter.autoFight || fighting->fighter.control == fight::Control::Computer);
}

std::optional<entt::entity> CreatureFightSystem::PlayersFighter() const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto creature = PrimaryCreatureOf(registry, k_LocalPlayer);
	if (!creature.has_value() || !InDuel(registry, *creature))
	{
		return std::nullopt;
	}
	const auto opponent = registry.Get<const CreatureFighting>(*creature).opponent;
	return registry.Valid(opponent) ? creature : std::nullopt;
}

bool CreatureFightSystem::Press(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, fight::Button button,
                                uint32_t milliseconds, uint32_t turn)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto self = PlayersFighter();
	if (!self.has_value())
	{
		return false;
	}
	const auto& fighting = registry.Get<const CreatureFighting>(*self);
	// Nothing more is taken while twelve moves wait
	if (fighting.fighter.queue.Size() >= fight::MoveQueue::k_Capacity)
	{
		return false;
	}
	const auto opponent = fighting.opponent;
	const auto& selfAt = registry.Get<const Transform>(*self).position;
	const auto& opponentAt = registry.Get<const Transform>(opponent).position;
	const auto opponentHeight = HeightOf(registry.Get<const Creature>(opponent).size);
	const auto onOpponent = feedback::RayHit(rayOrigin, rayDirection, BodyOf(registry, opponent));
	const auto onSelf = feedback::RayHit(rayOrigin, rayDirection, BodyOf(registry, *self));

	fight::Move move;
	if (onOpponent.has_value() && (!onSelf.has_value() || *onOpponent <= *onSelf))
	{
		// High, in the middle or low by where on the opponent's body the press lands
		const auto point = rayOrigin + (rayDirection * *onOpponent);
		move = fight::AttackMove(fight::BandOf((point.y - opponentAt.y) / opponentHeight));
	}
	else if (onSelf.has_value())
	{
		move = fight::BlockMove();
	}
	else
	{
		// The arena's ground: a step towards where it was pressed
		if (rayDirection.y >= 0.0f)
		{
			return false;
		}
		const auto along = (selfAt.y - rayOrigin.y) / rayDirection.y;
		const auto point = Flat(rayOrigin + (rayDirection * along));
		if (!fight::GroundPressCounts(fighting.arena, point))
		{
			return false;
		}
		const auto heading = registry.Get<const CreatureLocomotion>(*self).heading;
		move = fight::StepMove(fight::StepTowards(WorldToLocal(point - Flat(selfAt), heading)));
	}
	QueueMove(*self, move, fight::ReplacesQueue(button));
	_pressed = Pressed {.creature = *self, .milliseconds = milliseconds, .turn = turn};
	return true;
}

void CreatureFightSystem::Release(uint32_t milliseconds, uint32_t turn)
{
	if (!_pressed.has_value())
	{
		return;
	}
	const auto pressed = *_pressed;
	_pressed.reset();
	const auto realMs = milliseconds >= pressed.milliseconds ? milliseconds - pressed.milliseconds : 0u;
	const auto turns = turn >= pressed.turn ? turn - pressed.turn : 0u;
	ReleaseCharge(pressed.creature, fight::HeldMs(static_cast<float>(realMs), turns));
}

std::optional<fight::Tip> CreatureFightSystem::HandTip(std::optional<entt::entity> under) const
{
	const auto self = PlayersFighter();
	if (!self.has_value())
	{
		return std::nullopt;
	}
	const auto opponent = Locator::entitiesRegistry::value().Get<const CreatureFighting>(*self).opponent;
	return fight::TipOver(under == self, under == opponent);
}

bool CreatureFightSystem::SeesOpponent(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto opponent = registry.Get<const CreatureFighting>(creature).opponent;
	const auto from = registry.Get<const Transform>(creature).position;
	const auto to = registry.Get<const Transform>(opponent).position;
	const auto length = glm::distance(from, to);
	if (length <= 0.0f)
	{
		return false;
	}
	const auto hit = feedback::RayHit(from, (to - from) / length, BodyOf(registry, opponent));
	return hit.has_value() && *hit <= length;
}

bool CreatureFightSystem::GestureSpecialMove()
{
	const auto self = PlayersFighter();
	auto& registry = Locator::entitiesRegistry::value();
	if (!self.has_value() || registry.Get<const CreatureFighting>(*self).fighter.queue.Size() >= fight::MoveQueue::k_Capacity ||
	    !SeesOpponent(*self))
	{
		return false;
	}
	QueueMove(*self, {.kind = fight::Move::Kind::Special}, false);
	return true;
}

bool CreatureFightSystem::GestureSpell(MagicType type)
{
	const auto self = PlayersFighter();
	auto& registry = Locator::entitiesRegistry::value();
	if (!self.has_value() || registry.Get<const CreatureFighting>(*self).fighter.queue.Size() >= fight::MoveQueue::k_Capacity ||
	    !SeesOpponent(*self))
	{
		return false;
	}
	QueueMove(*self, {.kind = fight::Move::Kind::Spell, .value = static_cast<uint32_t>(type)}, false);
	return true;
}

bool CreatureFightSystem::IsBlocking(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* fighting = registry.Valid(creature) ? registry.TryGet<const CreatureFighting>(creature) : nullptr;
	return fighting != nullptr && fight::IsBlocking(fighting->fighter.state, fighting->fighter.timeMs,
	                                                DurationOf(creature, fight::animations::k_StartBlock));
}

void CreatureFightSystem::Recoil(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* fighting = registry.Valid(creature) ? registry.TryGet<CreatureFighting>(creature) : nullptr;
	if (fighting == nullptr)
	{
		return;
	}
	auto& fighter = fighting->fighter;
	// Reeling already from a blow, recoiling in its block or fainting, it takes no more notice
	using namespace fight::animations;
	const bool reeling = fighter.animation >= k_RecoilHigh && fighter.animation < k_WobbleHighSide;
	if (reeling || fighter.state == fight::State::BlockRecoil || fighter.state == fight::State::Faint)
	{
		return;
	}
	if (IsBlocking(creature))
	{
		fight::Enter(fighter, fight::State::BlockRecoil);
	}
	else if (fighter.state == fight::State::Stance || fighter.state == fight::State::Action ||
	         fighter.state == fight::State::CastStart || fighter.state == fight::State::Cast ||
	         fighter.state == fight::State::CastEnd)
	{
		fight::Enter(fighter, fight::State::Action, k_RecoilMid);
	}
}

void CreatureFightSystem::KnockOut(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !registry.AllOf<Creature, CreatureAnimation>(creature) ||
	    registry.AllOf<CreatureKnockedOut>(creature))
	{
		return;
	}
	// In a duel, its opponent wins it
	std::optional<glm::vec3> start;
	if (auto* fighting = registry.TryGet<CreatureFighting>(creature))
	{
		if (fighting->stage == CreatureFighting::Stage::Duel && InDuel(registry, fighting->opponent))
		{
			Win(fighting->opponent, creature);
			return;
		}
		start = fighting->startPosition;
		Leave(creature);
	}
	Faint(creature, start);
}

void CreatureFightSystem::ForceFaint(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !registry.AllOf<Creature, CreatureAnimation>(creature) ||
	    registry.AllOf<CreatureKnockedOut>(creature))
	{
		return;
	}
	std::optional<glm::vec3> start;
	if (const auto* fighting = registry.TryGet<const CreatureFighting>(creature))
	{
		start = fighting->startPosition;
		Leave(creature);
	}
	Faint(creature, start);
}

void CreatureFightSystem::Faint(entt::entity creature, std::optional<glm::vec3> start)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (Locator::creatureLocomotionSystem::has_value())
	{
		Locator::creatureLocomotionSystem::value().Stop(creature);
	}
	if (Locator::creatureObjectActionSystem::has_value())
	{
		Locator::creatureObjectActionSystem::value().Cancel(creature);
		Locator::creatureObjectActionSystem::value().Drop(creature);
	}
	// Fainting drops the leash
	if (Locator::leashSystem::has_value() && Locator::leashSystem::value().IsLeashed(creature))
	{
		Locator::leashSystem::value().TakeOff(creature);
	}
	auto& animation = registry.Get<CreatureAnimation>(creature);
	animation.body = {
	    .kind = creature_layers::BodyAction::Kind::Sequence,
	    .phase = creature_layers::BodyAction::Phase::Start,
	    .animations = {fight::animations::k_Faint, fight::animations::k_Faint, fight::animations::k_GetUp},
	    .mirrored = std::bernoulli_distribution(0.5)(_random),
	    .holdLoop = true,
	};
	animation.face = creature_layers::RelaxFace(animation.face);
	SetEyes(registry, creature, creature_eyes::Mode::Closed);
	// Whether it will rest to get better once it has lain long enough is decided now, by how it is as it faints
	bool rest = false;
	if (auto* needs = registry.TryGet<CreatureNeeds>(creature))
	{
		needs->rest = CreatureNeeds::Rest::Unconscious;
		rest = fight::NeedsRest(needs->needs.life, needs->needs.exhaustion);
	}
	// The spells on it are brought to their end
	if (auto* spells = registry.TryGet<CreatureSpells>(creature))
	{
		creature_spells::TryFinishAll(spells->spells);
	}
	// It is taken to its pen, or else back to where it stood as its fight started; passing out outside a fight with no
	// temple for a pen, to the middle of the land
	const auto here = registry.Get<const Transform>(creature).position;
	const auto fallback = start.has_value() ? start : NoPen();
	registry.AssignOrReplace<CreatureKnockedOut>(
	    creature,
	    CreatureKnockedOut {.rest = rest, .home = creature_home::HomeOf(registry, creature).value_or(fallback.value_or(here))});
}

void CreatureFightSystem::KillPermanently(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature))
	{
		return;
	}
	if (registry.AllOf<CreatureFighting>(creature))
	{
		AbortFight(creature);
		Leave(creature);
	}
	KnockOut(creature);
	if (auto* knockedOut = registry.TryGet<CreatureKnockedOut>(creature))
	{
		StopCarryingHome(creature, *knockedOut);
		knockedOut->permanent = true;
		knockedOut->stage = CreatureKnockedOut::Stage::Lying;
	}
	if (auto* needs = registry.TryGet<CreatureNeeds>(creature))
	{
		needs->needs.life = 0.0f;
	}
}

void CreatureFightSystem::Resurrect(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* knockedOut = registry.Valid(creature) ? registry.TryGet<CreatureKnockedOut>(creature) : nullptr;
	if (knockedOut == nullptr)
	{
		return;
	}
	if (auto* needs = registry.TryGet<CreatureNeeds>(creature))
	{
		needs->needs.life = fight::k_GetUpLife;
	}
	StopCarryingHome(creature, *knockedOut);
	knockedOut->permanent = false;
	knockedOut->stage = CreatureKnockedOut::Stage::GettingUp;
	knockedOut->seconds = 0.0f;
	auto& animation = registry.Get<CreatureAnimation>(creature);
	animation.body = creature_layers::EndLoop(animation.body);
	SetEyes(registry, creature, creature_eyes::Mode::Calm);
}

bool CreatureFightSystem::IsKnockedOut(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(creature) && registry.AllOf<CreatureKnockedOut>(creature);
}

std::optional<creature_fight_hud::Values> CreatureFightSystem::GetPanel() const
{
	// Shown while the camera watches a fight, the creature that made the arena first
	const auto& registry = Locator::entitiesRegistry::value();
	if (!_view.has_value() || _view->lingerSeconds.has_value() || !registry.Valid(_view->first) ||
	    !registry.Valid(_view->second))
	{
		return std::nullopt;
	}
	const auto shown = std::optional(_view->first);
	const auto opponent = _view->second;
	creature_fight_hud::Values values;
	for (size_t i = 0; i < values.sides.size(); ++i)
	{
		const auto entity = i == 0 ? *shown : opponent;
		auto& side = values.sides.at(i);
		side.name = creature_fight_hud::SpeciesName(registry.Get<const Creature>(entity).species);
		if (const auto* fighting = registry.TryGet<const CreatureFighting>(entity))
		{
			side.health = fighting->fighter.health;
			side.stamina = fighting->fighter.stamina;
		}
		else
		{
			side.health = 0.0f;
			side.stamina = 0.0f;
		}
	}
	return values;
}

void CreatureFightSystem::ProcessTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Each<CreatureFightRecord>([](CreatureFightRecord& record) { record.secondsSinceFight += k_TurnSeconds; });
	StartFightsFromMinds();
	ProcessStages();
	ProcessDuels();
	ProcessKnockedOut();
}

void CreatureFightSystem::StartFightsFromMinds()
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto isCreature = [&registry](entt::entity entity) {
		return registry.Valid(entity) && registry.AllOf<Creature, Transform>(entity);
	};
	// Leashed to another creature, the player wants it fought
	std::vector<std::pair<entt::entity, entt::entity>> wanted;
	registry.Each<CreatureMindState>([&](entt::entity entity, CreatureMindState& mind) {
		auto& actOn = mind.leash.actOn;
		for (auto it = actOn.begin(); it != actOn.end();)
		{
			const auto target = static_cast<entt::entity>(*it);
			if (target != entity && isCreature(target))
			{
				wanted.emplace_back(entity, target);
				it = actOn.erase(it);
			}
			else
			{
				++it;
			}
		}
	});

	// A creature angry enough picks a fight with the nearest creature in reach
	if (_angerStartsFights)
	{
		registry.Each<const CreatureMindState, const Creature, const Transform>(
		    [&](entt::entity entity, const CreatureMindState& mind, const Creature& creature, const Transform& transform) {
			    if (!mind.desires.has_value() || mind.paused || registry.AnyOf<CreatureFighting, CreatureKnockedOut>(entity))
			    {
				    return;
			    }
			    const auto& anger = (*mind.desires)[creature_desires::Desire::Anger];
			    if (!anger.activated || anger.value < fight::k_AngerToFight)
			    {
				    return;
			    }
			    std::optional<entt::entity> nearest;
			    float best = std::numeric_limits<float>::max();
			    registry.Each<const Creature, const Transform>(
			        [&](entt::entity other, const Creature& /*body*/, const Transform& at) {
				        if (other == entity || registry.AnyOf<CreatureFighting, CreatureKnockedOut>(other))
				        {
					        return;
				        }
				        const auto distance = glm::distance(Flat(at.position), Flat(transform.position));
				        if (distance < best)
				        {
					        best = distance;
					        nearest = other;
				        }
			        });
			    const auto* record = registry.TryGet<const CreatureFightRecord>(entity);
			    const auto since = record != nullptr ? record->secondsSinceFight : fight::k_SecondsBetweenFights;
			    if (nearest.has_value() &&
			        fight::WantsToFight(anger.value, LifeOf(registry, entity), best, creature.size, since))
			    {
				    wanted.emplace_back(entity, *nearest);
			    }
		    });
	}
	for (const auto& [creature, opponent] : wanted)
	{
		StartFight(creature, opponent);
	}
}

void CreatureFightSystem::ProcessStages()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> fighters;
	registry.Each<CreatureFighting>(
	    [&fighters](entt::entity entity, CreatureFighting& /*fighting*/) { fighters.push_back(entity); });
	auto* locomotion = Locator::creatureLocomotionSystem::has_value() ? &Locator::creatureLocomotionSystem::value() : nullptr;
	const auto moving = [locomotion](entt::entity entity) { return locomotion != nullptr && locomotion->IsMoving(entity); };

	for (const auto entity : fighters)
	{
		auto* fighting = registry.TryGet<CreatureFighting>(entity);
		if (fighting == nullptr)
		{
			continue;
		}
		fighting->stageSeconds += k_TurnSeconds;
		// Squaring up to the opponent and fighting it, it shows its anger as it starts and every few seconds after
		if (fighting->stage <= CreatureFighting::Stage::Duel && Locator::creatureMindSystem::has_value())
		{
			fighting->faceSeconds -= k_TurnSeconds;
			if (fighting->faceSeconds < k_FaceDue)
			{
				Locator::creatureMindSystem::value().ShowFeeling(entity, creature_face::Cue::Anger);
				fighting->faceSeconds = creature_mind::k_FaceRepeatSeconds;
			}
		}
		const auto opponent = fighting->opponent;
		const bool opponentHere = registry.Valid(opponent) && registry.AllOf<Creature, Transform>(opponent);
		const auto& body = registry.Get<const Creature>(entity);
		auto& animation = registry.Get<CreatureAnimation>(entity);
		const auto next = [fighting](CreatureFighting::Stage stage) {
			fighting->stage = stage;
			fighting->stageSeconds = 0.0f;
			fighting->played = false;
		};

		switch (fighting->stage)
		{
		case CreatureFighting::Stage::Approach:
			if (!opponentHere || !registry.AllOf<CreatureFighting>(opponent))
			{
				Leave(entity);
				break;
			}
			if (!fighting->played)
			{
				fighting->played = true;
				if (locomotion != nullptr)
				{
					const auto spot = fight::ArenaSpot(fighting->arena, body.size, fighting->madeArena);
					locomotion->MoveTo(entity, spot, Pace::Walk, 0.0f, fight::ArrivalDistance(body.size));
				}
			}
			else if (!moving(entity) || fighting->stageSeconds > k_ApproachSeconds)
			{
				next(CreatureFighting::Stage::Taunt);
			}
			break;
		case CreatureFighting::Stage::Taunt:
			if (!opponentHere)
			{
				Leave(entity);
				break;
			}
			if (!fighting->played)
			{
				fighting->played = true;
				if (locomotion != nullptr)
				{
					locomotion->Stop(entity);
					locomotion->TurnToFace(entity, Flat(registry.Get<const Transform>(opponent).position));
				}
			}
			else if (!fighting->taunted && (!moving(entity) || fighting->stageSeconds > k_ApproachSeconds))
			{
				// The game picks between a taunt and an action past the end of its list, which plays nothing, half and half
				fighting->taunted = true;
				if (std::bernoulli_distribution(0.5)(_random))
				{
					PlayOnce(animation, fight::animations::k_Taunt);
				}
			}
			else if (fighting->taunted && !creature_layers::IsPlaying(animation.body))
			{
				next(CreatureFighting::Stage::Ready);
			}
			break;
		case CreatureFighting::Stage::Ready:
		{
			const auto* other = opponentHere ? registry.TryGet<const CreatureFighting>(opponent) : nullptr;
			if (other == nullptr || other->opponent != entity)
			{
				Leave(entity);
			}
			else if (other->stage == CreatureFighting::Stage::Ready || other->stage == CreatureFighting::Stage::Duel)
			{
				// Both start together, so neither finds the other not yet duelling
				const bool otherReady = other->stage == CreatureFighting::Stage::Ready;
				BeginDuel(entity);
				if (otherReady)
				{
					BeginDuel(opponent);
				}
			}
			else if (fighting->stageSeconds > k_ApproachSeconds)
			{
				AbortFight(entity);
			}
			break;
		}
		case CreatureFighting::Stage::Duel:
			break;
		case CreatureFighting::Stage::Celebrate:
			if (fighting->fighter.state != fight::State::Idle)
			{
				break;
			}
			if (!fighting->played)
			{
				fighting->played = true;
				// Evil winners have a poo on the loser; the rest show off
				if (fight::WinnerPoos(body.alignment) && opponentHere)
				{
					next(CreatureFighting::Stage::PooOnLoser);
				}
				else
				{
					PlayOnce(animation, fight::animations::k_Impress);
				}
			}
			else if (!creature_layers::IsPlaying(animation.body))
			{
				Leave(entity);
			}
			break;
		case CreatureFighting::Stage::PooOnLoser:
			if (!fighting->played)
			{
				fighting->played = true;
				if (locomotion != nullptr && opponentHere)
				{
					locomotion->MoveToObject(entity, opponent, Pace::Walk, 0.0f);
				}
			}
			else if (!moving(entity) || fighting->stageSeconds > k_PooApproachSeconds)
			{
				Leave(entity);
				if (Locator::creatureMindSystem::has_value())
				{
					Locator::creatureMindSystem::value().Poo(entity);
				}
			}
			break;
		case CreatureFighting::Stage::Respond:
			if (fighting->fighter.state != fight::State::Idle)
			{
				break;
			}
			if (!fighting->played)
			{
				fighting->played = true;
				const auto life = LifeOf(registry, entity);
				const auto otherLife = opponentHere ? LifeOf(registry, opponent) : 0.0f;
				PlayOnce(animation, fight::Response(life, otherLife));
			}
			else if (!creature_layers::IsPlaying(animation.body))
			{
				Leave(entity);
			}
			break;
		}
	}
}

void CreatureFightSystem::BeginDuel(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& fighting = registry.Get<CreatureFighting>(creature);
	fighting.stage = CreatureFighting::Stage::Duel;
	fighting.stageSeconds = 0.0f;
	if (Locator::creatureLocomotionSystem::has_value())
	{
		Locator::creatureLocomotionSystem::value().Stop(creature);
	}
	MeasureBlows(creature);
	fight::Enter(fighting.fighter, fight::State::Start);
}

void CreatureFightSystem::MeasureBlows(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& fighting = registry.Get<CreatureFighting>(creature);
	if (fighting.measured || !Locator::creatureAnimationSystem::has_value())
	{
		return;
	}
	const auto& body = registry.Get<const Creature>(creature);
	const auto* rig = RigOf(body.species);
	if (rig == nullptr || !rig->actionPoints.has_value())
	{
		return;
	}
	auto& animations = Locator::creatureAnimationSystem::value();
	const auto scale = std::abs(registry.Get<const Transform>(creature).scale.x);
	const std::array bones {rig->actionPoints->rightHand, rig->actionPoints->rightFoot};
	fighting.reaches.clear();
	for (size_t i = 0; i < fight::animations::k_AttackCount; ++i)
	{
		const auto animation = fight::animations::k_FirstAttack + i;
		const auto duration = DurationOf(creature, animation);
		if (duration <= 0.0f)
		{
			continue;
		}
		// The blow lands where its hand or foot reaches furthest ahead, the body carried along by its root as it goes
		const auto rootAhead = -DisplacementOf(body.species, animation).y * scale;
		std::optional<fight::Reach> best;
		float bestTime = 0.0f;
		for (int sample = 0; sample <= k_MeasureSamples; ++sample)
		{
			const auto time = duration * static_cast<float>(sample) / static_cast<float>(k_MeasureSamples);
			for (const auto bone : bones)
			{
				const auto point = animations.BoneInAnimation(creature, animation, time, bone, false);
				if (!point.has_value())
				{
					continue;
				}
				const auto reach = (-point->z * scale) + (rootAhead * time / duration);
				if (!best.has_value() || reach > best->reach)
				{
					best = fight::Reach {.animation = animation, .reach = reach, .height = point->y * scale};
					bestTime = time;
				}
			}
		}
		if (best.has_value())
		{
			fighting.reaches.push_back(*best);
			fighting.hitTimesMs.at(i) = bestTime;
		}
	}
	fighting.measured = true;
}

void CreatureFightSystem::ProcessDuels()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> duelling;
	registry.Each<CreatureFighting>([&duelling](entt::entity entity, CreatureFighting& fighting) {
		if (fighting.stage == CreatureFighting::Stage::Duel)
		{
			duelling.push_back(entity);
		}
	});
	const auto random = [this](uint32_t range) {
		return range == 0 ? 0u : std::uniform_int_distribution<uint32_t>(0, range - 1)(_random);
	};
	for (const auto entity : duelling)
	{
		auto* fighting = registry.TryGet<CreatureFighting>(entity);
		if (fighting == nullptr || fighting->stage != CreatureFighting::Stage::Duel)
		{
			continue;
		}
		// Its opponent gone or out of the fight, it finishes
		const auto opponent = fighting->opponent;
		if (!registry.Valid(opponent) || !InDuel(registry, opponent) ||
		    registry.Get<const CreatureFighting>(opponent).opponent != entity)
		{
			EndFightFor(entity, true);
			fighting->stage = CreatureFighting::Stage::Respond;
			fighting->played = false;
			fight::Enter(fighting->fighter, fight::State::Finish);
			continue;
		}
		auto& fighter = fighting->fighter;
		fighter.stamina = fight::RegainStamina(fighter.stamina);
		const auto& other = registry.Get<const CreatureFighting>(opponent).fighter;

		if (fighter.state == fight::State::Stance && fighter.control == fight::Control::Player)
		{
			// Left alone long enough, the computer takes over the player's creature
			fighter.computerWaitMs -= k_TurnMs;
			if (fighter.computerWaitMs < 0.0f)
			{
				fighter.control = fight::Control::Computer;
			}
		}
		const bool computer = fighter.autoFight || fighter.control == fight::Control::Computer;
		if (!FacesOpponent(registry, entity, opponent))
		{
			// It turns to its opponent first
		}
		else if (!fighter.queue.Empty())
		{
			CheckQueue(entity);
		}
		else if (computer && fighter.state == fight::State::Stance)
		{
			const auto choice =
			    fight::ChooseMove(fight::TierOf(fighter.tendency), fight::OpponentOf(other.state, other.animation), random);
			switch (choice.kind)
			{
			case fight::Choice::Kind::Blow:
				AttemptBlow(entity, choice.band, fight::k_AiBlowSpeed);
				break;
			case fight::Choice::Kind::Block:
				fight::Enter(fighter, fight::State::BlockStart);
				break;
			case fight::Choice::Kind::Step:
				if (fight::CanStep(fighting->arena, Flat(registry.Get<const Transform>(entity).position), choice.step))
				{
					fight::Enter(fighter, fight::State::Action, fight::StepAnimation(choice.step));
				}
				break;
			case fight::Choice::Kind::None:
				break;
			}
		}
		else if (computer && fighter.state == fight::State::Block && fight::ComputerEndsBlock(random))
		{
			fight::Enter(fighter, fight::State::BlockEnd);
		}
	}
}

void CreatureFightSystem::CheckQueue(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& fighting = registry.Get<CreatureFighting>(creature);
	if (fighting.stage != CreatureFighting::Stage::Duel || !registry.Valid(fighting.opponent))
	{
		return;
	}
	if (fighting.fighter.state == fight::State::Stance && !FacesOpponent(registry, creature, fighting.opponent))
	{
		return;
	}
	const auto opponentAt = Flat(registry.Get<const Transform>(fighting.opponent).position);
	auto& fighter = fighting.fighter;
	const auto front = fighter.queue.Front();
	const auto order = fight::TakeOrder(fighter, fight::WithinRange(fighting.arena, opponentAt));
	if (!order.has_value())
	{
		return;
	}
	// A human player's creature learns from each move it makes, whoever asked for it
	if (order->kind != fight::Order::Kind::EndBlock && front.has_value() &&
	    registry.Get<const Creature>(creature).owner == k_LocalPlayer)
	{
		fighter.tendency = fight::LearnTendency(fighter.tendency, front->move.kind);
	}
	switch (order->kind)
	{
	case fight::Order::Kind::EndBlock:
		fight::Enter(fighter, fight::State::BlockEnd);
		break;
	case fight::Order::Kind::Animation:
		if (const auto step = fight::StepOf(order->value))
		{
			if (fight::CanStep(fighting.arena, Flat(registry.Get<const Transform>(creature).position), *step))
			{
				fight::Enter(fighter, fight::State::Action, order->value);
			}
		}
		else if (Has(creature, order->value))
		{
			fight::Enter(fighter, fight::State::Action, order->value);
		}
		break;
	case fight::Order::Kind::Cast:
		// The spell is cast as the cast's start ends
		fight::Enter(fighter, fight::State::CastStart);
		fighter.spell = order->value;
		break;
	case fight::Order::Kind::Block:
		fight::Enter(fighter, fight::State::BlockStart);
		break;
	case fight::Order::Kind::Special:
		fight::Enter(fighter, fight::State::Action, fight::animations::k_AttackSpecial, order->speed);
		break;
	case fight::Order::Kind::Blow:
		AttemptBlow(creature, order->band, order->speed);
		break;
	}
}

void CreatureFightSystem::CastFightSpell(entt::entity creature, MagicType type, entt::entity opponent)
{
	if (!Locator::creatureMindSystem::has_value() || static_cast<size_t>(type) >= magic::k_MagicTypeCount)
	{
		return;
	}
	const auto& effect = magic::GetMagicEffectInfo(Locator::infoConstants::value(), type);
	if (effect.isAggressiveSpellWhichIsUsedInCreatureFightArena != 0)
	{
		if (opponent != entt::null)
		{
			Locator::creatureMindSystem::value().TryMiracle(creature, type, opponent);
		}
	}
	else if (effect.isDefensiveSpellWhichIsUsedInCreatureFightArena != 0)
	{
		Locator::creatureMindSystem::value().TryMiracle(creature, type, creature);
	}
}

void CreatureFightSystem::AttemptBlow(entt::entity creature, fight::Band band, float speed)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& fighting = registry.Get<CreatureFighting>(creature);
	MeasureBlows(creature);
	const auto& body = registry.Get<const Creature>(creature);
	const auto opponent = fighting.opponent;
	const auto selfAt = Flat(registry.Get<const Transform>(creature).position);
	const auto opponentAt = Flat(registry.Get<const Transform>(opponent).position);
	const auto* opponentMoves = registry.TryGet<const CreatureLocomotion>(opponent);
	const auto opponentRadius = opponentMoves != nullptr ? opponentMoves->radius : 5.0f;
	const auto gap = glm::distance(selfAt, opponentAt) - opponentRadius;
	const auto scale = std::abs(registry.Get<const Transform>(creature).scale.x);
	const auto stepLength = glm::length(DisplacementOf(body.species, fight::animations::k_StepForward)) * scale;
	const auto choice = fight::ChooseAttack(fighting.reaches, band, gap, stepLength, body.size,
	                                        HeightOf(registry.Get<const Creature>(opponent).size));
	if (choice.animation.has_value())
	{
		fight::Enter(fighting.fighter, fight::State::Action, *choice.animation, speed);
	}
	else if (choice.step.has_value() && fight::CanStep(fighting.arena, selfAt, *choice.step))
	{
		fight::Enter(fighting.fighter, fight::State::Action, fight::StepAnimation(*choice.step));
	}
}

void CreatureFightSystem::Update(std::chrono::duration<float, std::milli> gameTime)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto milliseconds = gameTime.count();
	UpdateView(milliseconds / 1000.0f);
	std::vector<entt::entity> fighters;
	registry.Each<CreatureFighting>([&fighters](entt::entity entity, CreatureFighting& fighting) {
		if (fighting.fighter.state != fight::State::Idle)
		{
			fighters.push_back(entity);
		}
	});
	bool moved = false;
	for (const auto entity : fighters)
	{
		auto* fighting = registry.TryGet<CreatureFighting>(entity);
		auto* locomotion = registry.TryGet<CreatureLocomotion>(entity);
		auto* animation = registry.TryGet<CreatureAnimation>(entity);
		if (fighting == nullptr || locomotion == nullptr || animation == nullptr ||
		    fighting->fighter.state == fight::State::Idle)
		{
			continue;
		}
		auto& fighter = fighting->fighter;
		const auto& body = registry.Get<const Creature>(entity);
		const bool hasOpponent = fighting->stage == CreatureFighting::Stage::Duel && InDuel(registry, fighting->opponent);
		const auto duration = DurationOf(entity, fighter.animation);
		const auto before = fighter.timeMs;
		fighter.timeMs += milliseconds * fight::PlaybackSpeed(fighter.animation, fighter.speed);

		// An action carries the creature as far as its animation's root moves, back or sideways only within the range
		if ((fighter.state == fight::State::Action || fighter.state == fight::State::BlockRecoil) && duration > 0.0f)
		{
			const auto share = std::min(fighter.timeMs / duration, 1.0f);
			const auto scale = std::abs(registry.Get<const Transform>(entity).scale.x);
			auto by = MeshToWorld(DisplacementOf(body.species, fighter.animation) * scale, locomotion->heading) *
			          (share - fighting->movedShare);
			fighting->movedShare = share;
			const auto at = Flat(locomotion->toPosition);
			auto to = fighter.state == fight::State::BlockRecoil || fighter.animation != fight::animations::k_StepForward
			              ? fight::ClampToRange(fighting->arena, at + by)
			              : at + by;
			// Never through the opponent
			if (const auto* other = registry.TryGet<const CreatureLocomotion>(fighting->opponent))
			{
				to = fight::KeepApart(at, to, Flat(other->toPosition), fight::ClosestApart(locomotion->radius, other->radius));
			}
			by = to - at;
			if (glm::length(by) > 0.0f)
			{
				Shift(*locomotion, by);
				moved = true;
			}
		}
		// A blow lands as it reaches furthest
		if (fighter.state == fight::State::Action && fight::IsAttack(fighter.animation) && !fighter.landed)
		{
			const auto hitTime = fighting->hitTimesMs.at(fighter.animation - fight::animations::k_FirstAttack);
			if (before <= hitTime && fighter.timeMs > hitTime)
			{
				fighter.landed = true;
				TestHit(entity);
				// The hit may have ended the fight
				fighting = registry.TryGet<CreatureFighting>(entity);
				if (fighting == nullptr)
				{
					continue;
				}
			}
		}
		auto& current = fighting->fighter;
		if (hasOpponent && fight::FacesOpponent(current.state) && registry.Valid(fighting->opponent))
		{
			const auto towards =
			    Flat(registry.Get<const Transform>(fighting->opponent).position) - Flat(locomotion->toPosition);
			if (glm::length(towards) > 0.0f)
			{
				TurnTowards(*locomotion, creature_locomotion::HeadingOf(towards), k_FaceTurnRate * milliseconds / 1000.0f);
				moved = true;
			}
		}
		if (duration <= 0.0f || current.timeMs >= duration)
		{
			// As the start of a cast ends, the spell is cast: an attacking one at the opponent, a defending one on itself
			if (current.state == fight::State::CastStart && current.spell.has_value())
			{
				CastFightSpell(entity, static_cast<MagicType>(*current.spell), hasOpponent ? fighting->opponent : entt::null);
				current.spell.reset();
				fighting = registry.TryGet<CreatureFighting>(entity);
				if (fighting == nullptr)
				{
					continue;
				}
			}
			const auto next = fight::AfterAnimation(current.state, hasOpponent);
			if (next == current.state && fight::Loops(current.state))
			{
				current.timeMs =
				    current.state == fight::State::Lying || duration <= 0.0f ? duration : std::fmod(current.timeMs, duration);
			}
			else
			{
				fight::Enter(current, next);
				fighting->movedShare = 0.0f;
				if (next == fight::State::Idle)
				{
					animation->body = {};
					continue;
				}
			}
		}
		Show(*animation, current, current.queue.HasWaiting(), DurationOf(entity, current.animation));
	}
	if (moved)
	{
		registry.SetDirty();
	}
}

void CreatureFightSystem::TestHit(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& fighting = registry.Get<CreatureFighting>(creature);
	const auto opponent = fighting.opponent;
	if (!InDuel(registry, opponent))
	{
		return;
	}
	const auto& body = registry.Get<const Creature>(creature);
	const auto& opponentBody = registry.Get<const Creature>(opponent);
	const auto& transform = registry.Get<const Transform>(creature);
	const auto& opponentTransform = registry.Get<const Transform>(opponent);
	auto& opponentFighting = registry.Get<CreatureFighting>(opponent);
	auto& victim = opponentFighting.fighter;
	auto& attacker = fighting.fighter;

	// Where the blow reaches, ahead of the attacker and up
	const auto found = std::ranges::find_if(
	    fighting.reaches, [&attacker](const fight::Reach& reach) { return reach.animation == attacker.animation; });
	if (found == fighting.reaches.end())
	{
		return;
	}
	const auto heading = registry.Get<const CreatureLocomotion>(creature).heading;
	const auto ahead = creature_locomotion::DirectionOf(heading);
	// The root has already carried the body part of the way the blow reaches
	const auto scale = std::abs(transform.scale.x);
	const auto rootAhead = -DisplacementOf(body.species, attacker.animation).y * scale;
	const auto reach = found->reach - (rootAhead * fighting.movedShare);
	const auto& at = registry.Get<const CreatureLocomotion>(creature).toPosition;
	const auto point = glm::vec3(at.x + (ahead.x * reach), at.y + found->height, at.z + (ahead.y * reach));
	const auto tolerance = k_BlowReachPerSize * body.size * (attacker.special ? 2.0f : 1.0f);
	const auto opponentHeight = HeightOf(opponentBody.size);
	const auto capsules = BodyOf(registry, opponent);
	bool hit = false;
	if (!capsules.empty())
	{
		hit = feedback::DistanceOutside(point, capsules) <= tolerance;
	}
	else
	{
		const auto* moves = registry.TryGet<const CreatureLocomotion>(opponent);
		const auto radius = moves != nullptr ? moves->radius : 5.0f;
		hit = glm::distance(Flat(point), Flat(opponentTransform.position)) <= radius + tolerance &&
		      point.y - opponentTransform.position.y <= opponentHeight;
	}
	if (!hit)
	{
		return;
	}

	const bool blocked = fight::IsBlocking(victim.state, victim.timeMs, DurationOf(opponent, fight::animations::k_StartBlock));
	const auto heightShare = std::clamp((point.y - opponentTransform.position.y) / opponentHeight, 0.0f, 1.0f);
	// Where it lands from the middle of the victim's body
	const auto* opponentMoves = registry.TryGet<const CreatureLocomotion>(opponent);
	const auto victimRadius = opponentMoves != nullptr ? std::max(opponentMoves->radius, 0.1f) : 5.0f;
	const auto local =
	    WorldToLocal(Flat(point) - Flat(opponentTransform.position), opponentMoves != nullptr ? opponentMoves->heading : 0.0f);
	const auto direction = fight::RecoilDirectionOf({local.x / victimRadius, (heightShare - 0.5f) * 2.0f});
	const auto result = fight::ResolveBlow(
	    {
	        .attackerSize = body.size,
	        .defenderSize = opponentBody.size,
	        .attackerStrength = body.strength,
	        .defenderStrength = opponentBody.strength,
	        .speed = attacker.speed,
	        .special = attacker.special,
	        .heightShare = heightShare,
	        .blocked = blocked,
	    },
	    direction);
	victim.health = std::max(victim.health - result.damage, 0.0f);
	// Getting hit takes away a charged blow still waiting
	victim.queue.CancelWaiting();

	auto& victimAnimation = registry.Get<CreatureAnimation>(opponent);
	victimAnimation.wobble = {
	    .animation = fight::WobbleAnimation(result.band, direction == fight::RecoilDirection::Right ||
	                                                         direction == fight::RecoilDirection::Left),
	    .timeMs = 0.0f,
	};
	// An unblocked blow wounds the skin, and some wounds bleed. Where on the skin isn't found from the body yet.
	if (!blocked && Locator::creatureSkinSystem::has_value())
	{
		const auto random = [this](uint32_t range) {
			return range == 0 ? 0u : std::uniform_int_distribution<uint32_t>(0, range - 1)(_random);
		};
		auto& skins = Locator::creatureSkinSystem::value();
		std::uniform_int_distribution<int> texel(k_WoundEdge, static_cast<int>(creature_marks::k_AtlasSize) - k_WoundEdge - 1);
		creature_marks::Mark wound {
		    .u = static_cast<uint8_t>(texel(_random)),
		    .v = static_cast<uint8_t>(texel(_random)),
		    .skin = 0,
		    .age = 0,
		    .type = fight::WoundKind(fight::BlowWoundType(attacker.animation), random),
		    .column = static_cast<uint8_t>(random(creature_marks::k_CellsPerRow)),
		};
		skins.AddWound(opponent, wound);
		if (fight::Bleeds(wound.type))
		{
			auto drop = wound;
			drop.type = 0;
			drop.column = 0;
			for (int i = 0; i < k_BloodDrops && drop.v < 255; ++i, ++drop.v)
			{
				skins.AddBlood(opponent, drop);
			}
		}
	}

	// Too close, the attacker is pushed back by half of how far they overlap
	if (auto* moves = registry.TryGet<CreatureLocomotion>(creature); moves != nullptr && opponentMoves != nullptr)
	{
		const auto between = Flat(moves->toPosition) - Flat(opponentMoves->toPosition);
		const auto distance = glm::length(between);
		const auto closest = (moves->radius * 0.5f) + opponentMoves->radius;
		if (distance < closest && distance > 0.0f)
		{
			Shift(*moves, between * (((closest - distance) * 0.5f) / distance));
		}
	}

	if (victim.health <= 0.0f)
	{
		Win(creature, opponent);
		return;
	}
	if (blocked)
	{
		fight::Enter(victim, fight::State::BlockRecoil);
		opponentFighting.movedShare = 0.0f;
	}
	else if (victim.state == fight::State::Stance || victim.state == fight::State::Action ||
	         victim.state == fight::State::CastStart || victim.state == fight::State::Cast ||
	         victim.state == fight::State::CastEnd)
	{
		fight::Enter(victim, fight::State::Action, Has(opponent, result.recoil) ? result.recoil : result.plainRecoil);
		opponentFighting.movedShare = 0.0f;
	}
}

void CreatureFightSystem::Win(entt::entity winner, entt::entity loser)
{
	auto& registry = Locator::entitiesRegistry::value();
	EndFightFor(winner, true);
	EndFightFor(loser, false);
	if (auto* record = registry.TryGet<CreatureFightRecord>(winner))
	{
		++record->wins;
	}
	const auto* lost = registry.TryGet<const CreatureFighting>(loser);
	const auto start = lost != nullptr ? std::optional(lost->startPosition) : std::nullopt;
	Leave(loser);
	Faint(loser, start);
	if (auto* fighting = registry.TryGet<CreatureFighting>(winner))
	{
		fighting->stage = CreatureFighting::Stage::Celebrate;
		fighting->stageSeconds = 0.0f;
		fighting->played = false;
		fight::Enter(fighting->fighter, fight::State::Finish);
		auto& won = registry.Get<CreatureAnimation>(winner);
		won.face = creature_layers::RelaxFace(won.face);
	}
}

void CreatureFightSystem::EndFightFor(entt::entity creature, bool won)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* fighting = registry.TryGet<CreatureFighting>(creature);
	if (fighting == nullptr || fighting->ended)
	{
		return;
	}
	fighting->ended = true;
	// A quarter of the fight health it lost comes off its life
	if (auto* needs = registry.TryGet<CreatureNeeds>(creature))
	{
		needs->needs.life = fight::LifeAfterFight(needs->needs.life, fighting->fighter.health);
	}
	auto& record = registry.AllOf<CreatureFightRecord>(creature) ? registry.Get<CreatureFightRecord>(creature)
	                                                             : registry.Assign<CreatureFightRecord>(creature);
	record.tendency = fighting->fighter.tendency;
	record.foughtBefore = true;
	record.secondsSinceFight = 0.0f;
	if (Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().FoughtFight(creature, won);
	}
	if (_pressed.has_value() && _pressed->creature == creature)
	{
		_pressed.reset();
	}
}

void CreatureFightSystem::Leave(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* fighting = registry.TryGet<CreatureFighting>(creature);
	if (fighting == nullptr)
	{
		return;
	}
	if (!fighting->ended)
	{
		if (auto* record = registry.TryGet<CreatureFightRecord>(creature))
		{
			record->secondsSinceFight = 0.0f;
		}
	}
	if (auto* animation = registry.TryGet<CreatureAnimation>(creature))
	{
		if (fighting->fighter.state != fight::State::Idle)
		{
			animation->body = {};
		}
		animation->face = creature_layers::RelaxFace(animation->face);
	}
	if (_pressed.has_value() && _pressed->creature == creature)
	{
		_pressed.reset();
	}
	if (fighting->madeArena)
	{
		ReleaseArena(registry, fighting->arenaEntity);
	}
	registry.Remove<CreatureFighting>(creature);
}

void CreatureFightSystem::ProcessKnockedOut()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> lying;
	registry.Each<CreatureKnockedOut>(
	    [&lying](entt::entity entity, CreatureKnockedOut& /*knockedOut*/) { lying.push_back(entity); });
	for (const auto entity : lying)
	{
		auto& knockedOut = registry.Get<CreatureKnockedOut>(entity);
		auto* needs = registry.TryGet<CreatureNeeds>(entity);
		auto& animation = registry.Get<CreatureAnimation>(entity);
		const auto& body = registry.Get<const Creature>(entity);
		knockedOut.seconds += k_TurnSeconds;
		const auto next = [&knockedOut](CreatureKnockedOut::Stage stage) {
			knockedOut.stage = stage;
			knockedOut.seconds = 0.0f;
		};
		const auto getUp = [&] {
			next(CreatureKnockedOut::Stage::GettingUp);
			animation.body = creature_layers::EndLoop(animation.body);
			SetEyes(registry, entity, creature_eyes::Mode::Calm);
		};

		switch (knockedOut.stage)
		{
		case CreatureKnockedOut::Stage::Lying:
			if (!knockedOut.permanent && knockedOut.seconds >= fight::FaintSeconds(body.size))
			{
				// A player's creature is taken home; any other comes round where it lies
				if (body.owner != PlayerNames::NEUTRAL)
				{
					// It fizzes out of sight where it lies, with the teleport's sound
					next(CreatureKnockedOut::Stage::FadingOut);
					knockedOut.fizzTurns = creature_fizz::TurnsOf(creature_fizz::k_CarryHomeSeconds, k_TurnMilliseconds);
					SetFizz(entity, 1.0f, creature_fizz::k_CarryHomeSeconds);
				}
				else
				{
					next(CreatureKnockedOut::Stage::Waiting);
				}
			}
			break;
		case CreatureKnockedOut::Stage::FadingOut:
			knockedOut.fizzTurns = knockedOut.fizzTurns > 0 ? knockedOut.fizzTurns - 1 : 0;
			if (knockedOut.fizzTurns == 0)
			{
				// Out of sight, it is moved home and fizzes back into sight there
				auto* locomotion = registry.TryGet<CreatureLocomotion>(entity);
				if (knockedOut.home.has_value() && locomotion != nullptr)
				{
					// The temple's pen is taken again now, as the home follows the temple every turn
					PlaceAt(*locomotion, registry.Get<Transform>(entity),
					        creature_home::HomeOf(registry, entity).value_or(*knockedOut.home));
					registry.SetDirty();
				}
				SetFizz(entity, 0.0f, creature_fizz::k_CarryHomeSeconds);
				// Home, it is no more exhausted or thirsty than it can bear, with a little energy
				if (Locator::creaturePhysiologySystem::has_value())
				{
					Locator::creaturePhysiologySystem::value().WakeFromFaint(entity);
				}
				next(CreatureKnockedOut::Stage::FadingIn);
				knockedOut.fizzTurns = creature_fizz::TurnsOf(creature_fizz::k_CarryHomeSeconds, k_TurnMilliseconds);
			}
			break;
		case CreatureKnockedOut::Stage::FadingIn:
			knockedOut.fizzTurns = knockedOut.fizzTurns > 0 ? knockedOut.fizzTurns - 1 : 0;
			if (knockedOut.fizzTurns == 0)
			{
				next(CreatureKnockedOut::Stage::Waiting);
			}
			break;
		case CreatureKnockedOut::Stage::Waiting:
			if (knockedOut.seconds >= fight::k_WaitAfterHomeSeconds)
			{
				next(CreatureKnockedOut::Stage::WaitingForSpells);
			}
			break;
		case CreatureKnockedOut::Stage::WaitingForSpells:
		{
			// The spells on it are brought to their end each turn until none is left
			auto* spells = registry.TryGet<CreatureSpells>(entity);
			if (spells == nullptr || creature_spells::TryFinishAll(spells->spells))
			{
				if (knockedOut.rest)
				{
					next(CreatureKnockedOut::Stage::Resting);
				}
				else
				{
					getUp();
				}
			}
			break;
		}
		case CreatureKnockedOut::Stage::Resting:
			if (needs == nullptr || !fight::NeedsRest(needs->needs.life, needs->needs.exhaustion))
			{
				getUp();
			}
			break;
		case CreatureKnockedOut::Stage::GettingUp:
			if (!creature_layers::IsPlaying(animation.body))
			{
				if (needs != nullptr)
				{
					needs->rest = CreatureNeeds::Rest::Awake;
				}
				registry.Remove<CreatureKnockedOut>(entity);
				continue;
			}
			break;
		}
		if (needs != nullptr)
		{
			// Resting, its body heals; otherwise it is out cold until it gets up
			needs->rest = knockedOut.stage == CreatureKnockedOut::Stage::Resting     ? CreatureNeeds::Rest::Resting
			              : knockedOut.stage == CreatureKnockedOut::Stage::GettingUp ? CreatureNeeds::Rest::Awake
			                                                                         : CreatureNeeds::Rest::Unconscious;
		}
	}
}
