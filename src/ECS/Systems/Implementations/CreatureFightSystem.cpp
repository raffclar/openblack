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
#include <array>
#include <chrono>
#include <iterator>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>
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
#include "Audio/GameSoundEffects.h"
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
#include "Creature/CreatureRestAndHome.h"
#include "Creature/CreatureRig.h"
#include "Creature/FightMarks.h"
#include "ECS/Archetypes/ArenaArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureArena.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/CreatureHome.h"
#include "ECS/CreatureScars.h"
#include "ECS/Map.h"
#include "ECS/PosedModel.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureFizzSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "ECS/Systems/FightTranscriptNote.h"
#include "ECS/Systems/GuidanceSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/ThingCircles.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Input/GameActionMapInterface.h"
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
namespace marks = openblack::creature_fight::marks;
namespace posed_model = openblack::ecs::posed_model;
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

std::string_view Name(CreatureFighting::Stage stage)
{
	static constexpr std::array<std::string_view, 9> k_Names {"face_off", "taunt", "point",     "approach", "ready",
	                                                          "square",   "duel",  "celebrate", "respond"};
	const auto index = static_cast<size_t>(stage);
	return index < k_Names.size() ? k_Names.at(index) : std::string_view("unknown");
}

std::string_view Name(CreatureKnockedOut::Stage stage)
{
	static constexpr std::array<std::string_view, 7> k_Names {
	    "lying", "fading_out", "fading_in", "waiting", "waiting_for_spells", "resting", "getting_up"};
	const auto index = static_cast<size_t>(stage);
	return index < k_Names.size() ? k_Names.at(index) : std::string_view("unknown");
}

/// The levels of a creature for the fight transcript: its fight health and stamina while it fights, its life, its
/// strength, its exhaustion and its energy
fight_transcript::Fields Levels(const ecs::Registry& registry, entt::entity creature)
{
	fight_transcript::Fields fields {{"creature", fight_transcript::Id(creature)}};
	if (const auto* fighting = registry.TryGet<const CreatureFighting>(creature))
	{
		fields.emplace_back("health", fighting->fighter.health);
		fields.emplace_back("stamina", fighting->fighter.stamina);
		fields.emplace_back("control", fight::Name(fighting->fighter.control));
		fields.emplace_back("tendency", fighting->fighter.tendency);
		fields.emplace_back("queue", static_cast<uint32_t>(fighting->fighter.queue.Size()));
	}
	if (const auto* needs = registry.TryGet<const CreatureNeeds>(creature))
	{
		fields.emplace_back("life", needs->needs.life);
		fields.emplace_back("exhaustion", needs->needs.exhaustion);
		fields.emplace_back("energy", needs->needs.energy);
	}
	if (const auto* body = registry.TryGet<const Creature>(creature))
	{
		fields.emplace_back("strength", body->strength);
		fields.emplace_back("size", ShownSize(*body));
		fields.emplace_back("alignment", body->alignment);
	}
	return fields;
}

/// A change of one level of a creature for the fight transcript, with what caused it
fight_transcript::Fields LevelChange(entt::entity creature, std::string_view level, float before, float after,
                                     std::string_view why)
{
	return {{"creature", fight_transcript::Id(creature)},
	        {"level", level},
	        {"before", before},
	        {"after", after},
	        {"change", after - before},
	        {"why", why}};
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
	                              feedback::k_BodyRadiusShare * HeightOf(ShownSize(*body)));
}

/// The creature's mesh and the matrix it is drawn with, if it has a loaded mesh
std::optional<std::pair<const graphics::L3DMesh*, glm::mat4>> DrawnMeshOf(const ecs::Registry& registry, entt::entity creature)
{
	const auto* mesh = registry.Valid(creature) ? registry.TryGet<const Mesh>(creature) : nullptr;
	const auto* transform = registry.Valid(creature) ? registry.TryGet<const Transform>(creature) : nullptr;
	if (mesh == nullptr || transform == nullptr || !Locator::resources::has_value())
	{
		return std::nullopt;
	}
	auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(mesh->id))
	{
		return std::nullopt;
	}
	return std::pair {&*meshes.Handle(mesh->id),
	                  creature::PlacementMatrix(transform->position, transform->rotation, transform->scale)};
}

/// Where a line first meets a creature's skin as it is drawn, tested against every triangle of its nearest level of
/// detail as posed this frame
std::optional<posed_model::SkinHit> SkinHitOn(const ecs::Registry& registry, entt::entity creature, const glm::vec3& origin,
                                              const glm::vec3& direction)
{
	const auto drawn = DrawnMeshOf(registry, creature);
	if (!drawn.has_value())
	{
		return std::nullopt;
	}
	return posed_model::NearestSkinHit(registry, creature, *drawn->first, drawn->second, origin, direction);
}

/// A sparkle sitting where a line met a creature's skin, kept on that triangle so that it moves with the skin
marks::Mark MarkOnSkin(const posed_model::SkinHit& hit, marks::Anchor anchor)
{
	return {.anchor = anchor, .subMesh = hit.subMesh, .firstIndex = hit.hit.firstIndex, .s = hit.hit.s, .t = hit.hit.t};
}

/// Where a sparkle sits now, if what it sits on is still there
std::optional<glm::vec3> MarkPosition(const ecs::Registry& registry, entt::entity self, entt::entity opponent,
                                      const marks::Mark& mark)
{
	if (mark.anchor == marks::Anchor::Ground)
	{
		const auto* locomotion = registry.TryGet<const CreatureLocomotion>(self);
		const auto& transform = registry.Get<const Transform>(self);
		return marks::ToWorld(marks::GroundFrame(transform.position, locomotion != nullptr ? locomotion->heading : 0.0f),
		                      mark.local);
	}
	const auto body = mark.anchor == marks::Anchor::Opponent ? opponent : self;
	const auto drawn = DrawnMeshOf(registry, body);
	if (!drawn.has_value())
	{
		return std::nullopt;
	}
	return posed_model::PointOnSkin(registry, body, *drawn->first, drawn->second,
	                                {.subMesh = mark.subMesh, .firstIndex = mark.firstIndex, .s = mark.s, .t = mark.t});
}

/// Whether the hand of the player holds nothing: no thing and no miracle. A hand that only starts to take a thing holds
/// nothing yet.
bool HandHoldsNothing(const ecs::Registry& registry)
{
	if (Locator::magicSystem::has_value() && Locator::magicSystem::value().GetHeldSeed().has_value())
	{
		return false;
	}
	if (!Locator::handSystem::has_value())
	{
		return true;
	}
	const auto hand = Locator::handSystem::value().GetPlayerHands()[static_cast<size_t>(HandSystemInterface::Side::Left)];
	const auto* grab = registry.Valid(hand) ? registry.TryGet<const HandGrab>(hand) : nullptr;
	return grab == nullptr || grab->state == HandGrab::State::Empty || grab->state == HandGrab::State::Grabbing;
}

/// The creature the hand is over, as the interface picked it when the last frame was drawn
std::optional<entt::entity> CreatureUnderHand(const ecs::Registry& registry)
{
	if (!Locator::pickingSystem::has_value())
	{
		return std::nullopt;
	}
	const auto& picked = Locator::pickingSystem::value().GetPick().object;
	if (!picked.has_value() || !registry.Valid(*picked) || !registry.AllOf<Creature>(*picked))
	{
		return std::nullopt;
	}
	return picked;
}

/// The sound of the hand directing a fight: heard the same wherever the click was
constexpr auto k_DirectSound = entt::hashed_string("InGame.sad/1");

void PlayDirectSound()
{
	if (Locator::audio::has_value())
	{
		audio::PlayGameSoundEffect(k_DirectSound.value(), std::nullopt);
	}
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

/// What a thing standing fixed in a land cell is, as an arena looking for room sees it
arena::ThingInCell ThingInCellOf(const ecs::Registry& registry, entt::entity thing)
{
	arena::ThingInCell seen {.kind = arena::ThingKind::Fixed,
	                         .living = registry.AnyOf<Villager, Creature, Animal>(thing),
	                         .hasModel = registry.AllOf<Mesh>(thing),
	                         .height = ecs::world_objects::SizeOf(thing).height,
	                         .circles = {}};
	if (registry.AllOf<Temple>(thing))
	{
		seen.kind = arena::ThingKind::Temple;
	}
	else if (const auto* info = ecs::world_objects::AbodeInfoOf(thing);
	         info != nullptr && (info->abodeType == AbodeType::StoragePit || info->abodeType == AbodeType::TownCentre))
	{
		seen.kind = arena::ThingKind::StoragePitOrTownCentre;
	}
	else if (registry.AnyOf<Abode, Field, SpellDispenser>(thing))
	{
		seen.kind = arena::ThingKind::Building;
	}
	else if (registry.AnyOf<Tree, DeadTree, MobileStatic>(thing))
	{
		// Trees, dead trees and statics can all be thrown about
		seen.kind = arena::ThingKind::Movable;
	}
	for (const auto& circle : ecs::thing_circles::CirclesOf(registry, thing))
	{
		seen.circles.push_back({.centre = circle.centre, .radius = circle.radius});
	}
	return seen;
}

/// Whether an arena for a creature of a size may lie over a land cell: one on the map, of land without water, with
/// nothing standing in the way, that creatures can walk to
bool UsableForArena(const ecs::Registry& registry, glm::ivec2 cell, const land_avoid::Map& avoid, float creatureSize)
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
	// Only things standing fixed cover circles on the ground; things that move about never keep an arena off
	if (Locator::entitiesMap::has_value())
	{
		const auto things = Locator::entitiesMap::value().GetFixedInGridCell(
		    ecs::MapInterface::CellId(static_cast<uint16_t>(cell.x), static_cast<uint16_t>(cell.y)));
		const bool blocked = std::ranges::any_of(things, [&](entt::entity thing) {
			return registry.Valid(thing) && arena::KeepsArenaOff(ThingInCellOf(registry, thing), creatureSize, cell);
		});
		if (blocked)
		{
			return false;
		}
	}
	return avoid.At(cell) == land_avoid::k_Land;
}

/// The nearest arena strictly closer than a distance in metres to a point in map units, if any
std::optional<entt::entity> NearestArena(const ecs::Registry& registry, glm::ivec2 point, float within)
{
	std::vector<entt::entity> entities;
	std::vector<glm::ivec2> places;
	registry.Each<const CreatureArena>([&](entt::entity entity, const CreatureArena& found) {
		entities.push_back(entity);
		places.push_back(found.place);
	});
	if (const auto nearest = arena::Nearest(places, point, within))
	{
		return entities.at(*nearest);
	}
	return std::nullopt;
}

/// A new arena of a radius wanted, or smaller, at the nearest place clear for a creature of a size round a point in map
/// units; none without room. It lasts only for the fight that takes it.
std::optional<entt::entity> MakeArena(ecs::Registry& registry, glm::ivec2 start, float wanted, float creatureSize)
{
	const auto& avoid = LandAvoidOf(registry);
	const auto placed = arena::Place(start, wanted, [&registry, &avoid, creatureSize](glm::ivec2 cell) {
		return UsableForArena(registry, cell, avoid, creatureSize);
	});
	if (!placed.has_value())
	{
		return std::nullopt;
	}
	return ecs::archetypes::ArenaArchetype::Create(placed->centre, placed->radius, true);
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
	const float wanted = fight::ArenaRadius(ShownSize(body), ShownSize(registry.Get<const Creature>(opponent)));

	if (const auto nearest = NearestArena(registry, start, arena::ReuseDistance(RunShareOf(body.species), ShownSize(body))))
	{
		auto& taken = registry.Get<CreatureArena>(*nearest);
		taken.radius = std::min(taken.radius, wanted);
		taken.temporary = false;
		return nearest;
	}
	return MakeArena(registry, start, wanted, ShownSize(body));
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
		audio::PlayGameSoundEffect(k_ArenaDrawnSound.value(), registry.Get<const Transform>(entity).position);
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
		if (Locator::reactionSystem::has_value())
		{
			Locator::reactionSystem::value().RemoveFrom(entity);
		}
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

/// The creature that started a fight takes its arena after the face-off: the arena appears for both fighters. Without
/// an arena near enough, or room for one, it takes none.
bool DrawArenaFor(ecs::Registry& registry, entt::entity creature, entt::entity opponent)
{
	const auto arenaEntity = TakeArena(registry, creature, opponent);
	if (!arenaEntity.has_value())
	{
		return false;
	}
	const auto& taken = registry.Get<const CreatureArena>(*arenaEntity);
	const fight::Arena arena {.centre = {map_coords::ToMetres(taken.place.x), map_coords::ToMetres(taken.place.y)},
	                          .radius = taken.radius};
	for (const auto self : {creature, opponent})
	{
		auto& fighting = registry.Get<CreatureFighting>(self);
		fighting.arena = arena;
		fighting.arenaEntity = *arenaEntity;
	}
	StartArenaFight(registry, *arenaEntity, creature, opponent);
	fight_transcript::Note(creature, "arena", [&] {
		using fight_transcript::Id;
		return fight_transcript::Fields {{"arena", Id(*arenaEntity)},
		                                 {"arena_x", arena.centre.x},
		                                 {"arena_z", arena.centre.y},
		                                 {"arena_radius", arena.radius},
		                                 {"arena_made_by", Id(creature)}};
	});
	return true;
}

/// How far the posed body of a creature reaches in front of its feet, in the direction that it faces
float ForwardExtentOf(const ecs::Registry& registry, entt::entity creature)
{
	const auto* animation = registry.TryGet<const CreatureAnimation>(creature);
	const auto* transform = registry.TryGet<const Transform>(creature);
	const auto* locomotion = registry.TryGet<const CreatureLocomotion>(creature);
	if (animation == nullptr || transform == nullptr || locomotion == nullptr)
	{
		return 0.0f;
	}
	const auto placement = creature::PlacementMatrix(transform->position, transform->rotation, transform->scale);
	std::vector<glm::vec2> bones;
	bones.reserve(animation->boneMatrices.size());
	for (const auto& bone : animation->boneMatrices)
	{
		bones.push_back(Flat(glm::vec3(placement * bone[3])) - Flat(transform->position));
	}
	return fight::ForwardExtent(bones, creature_locomotion::DirectionOf(locomotion->heading));
}

/// When the bodies of two fighters overlap, both move apart by half the overlap, along the direction that the first one
/// faces. Nothing moves when the last action that the opponent started is a blow.
void PullApartFighters(ecs::Registry& registry, entt::entity creature, const CreatureFighting& fighting)
{
	const auto opponent = fighting.opponent;
	const auto* other = registry.TryGet<const CreatureFighting>(opponent);
	auto* self = registry.TryGet<CreatureLocomotion>(creature);
	auto* otherBody = registry.TryGet<CreatureLocomotion>(opponent);
	if (other == nullptr || self == nullptr || otherBody == nullptr || fight::IsAttack(other->fighter.lastAction))
	{
		return;
	}
	const auto at = Flat(self->toPosition);
	const auto otherAt = Flat(otherBody->toPosition);
	const auto extents = ForwardExtentOf(registry, creature) + ForwardExtentOf(registry, opponent);
	const auto pulled =
	    fight::PullApartOf(fighting.arena, at, otherAt, creature_locomotion::DirectionOf(self->heading), extents);
	if (pulled.first.has_value())
	{
		Shift(*self, *pulled.first - at);
	}
	if (pulled.second.has_value())
	{
		Shift(*otherBody, *pulled.second - otherAt);
	}
	if (pulled.first.has_value() || pulled.second.has_value())
	{
		registry.SetDirty();
		fight_transcript::Note(creature, "pull_apart", [&] {
			return fight_transcript::Fields {{"creature", fight_transcript::Id(creature)},
			                                 {"apart", glm::distance(at, otherAt)},
			                                 {"self_from_middle", glm::distance(at, fighting.arena.centre)},
			                                 {"opponent_from_middle", glm::distance(otherAt, fighting.arena.centre)},
			                                 {"extents", extents},
			                                 {"moved_self", pulled.first.has_value()},
			                                 {"moved_opponent", pulled.second.has_value()}};
		});
	}
}

/// When either fighter is not inside the range of the arena, the fighter makes no moves. In its stance, it steps back to
/// the middle when it is farther from the middle than its body radius. A block stops. Returns whether either fighter is
/// not inside the range.
bool KeepInArena(ecs::Registry& registry, entt::entity creature, CreatureFighting& fighting)
{
	const auto& arena = fighting.arena;
	const auto selfAt = registry.Get<const Transform>(creature).position;
	const auto otherAt = registry.Get<const Transform>(fighting.opponent).position;
	if (!fight::OutsideRange(arena, Flat(otherAt)) && !fight::OutsideRange(arena, Flat(selfAt)))
	{
		return false;
	}
	auto& fighter = fighting.fighter;
	std::optional<fight::Step> step;
	if (fighter.state == fight::State::Block)
	{
		fight::Enter(fighter, fight::State::BlockEnd);
	}
	else if (const auto* locomotion = registry.TryGet<const CreatureLocomotion>(creature);
	         fighter.state == fight::State::Stance && locomotion != nullptr)
	{
		const glm::vec3 middle {arena.centre.x, GroundAt(arena.centre), arena.centre.y};
		step = fight::StepToMiddle(WorldToLocal(arena.centre - Flat(selfAt), locomotion->heading),
		                           glm::distance(middle, selfAt), locomotion->shownRadius);
		if (step.has_value())
		{
			fight::Enter(fighter, fight::State::Action, fight::StepAnimation(*step));
		}
	}
	fight_transcript::Note(creature, "keep_in_arena", [&] {
		return fight_transcript::Fields {{"creature", fight_transcript::Id(creature)},
		                                 {"state", fight::Name(fighter.state)},
		                                 {"step", step.has_value() ? fight::Name(*step) : std::string_view("none")}};
	});
	return true;
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
	// Every fight starts by asking the other creature, which may refuse
	const bool dead = Locator::creatureMindSystem::has_value() && Locator::creatureMindSystem::value().IsLyingDead(opponent);
	if (!fight::AgreesToFight(HeightOf(ShownSize(registry.Get<const Creature>(opponent))),
	                          HeightOf(ShownSize(registry.Get<const Creature>(creature))), dead,
	                          registry.AllOf<ScriptControlled>(opponent), registry.AllOf<ScriptControlled>(creature)))
	{
		return StartResult::Refused;
	}
	// The arena appears only after the face-off, so a fight with no room for an arena starts and then stops
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

		const auto& record = registry.AllOf<CreatureFightRecord>(self) ? registry.Get<CreatureFightRecord>(self)
		                                                               : registry.Assign<CreatureFightRecord>(self);
		const auto* needs = registry.TryGet<const CreatureNeeds>(self);
		const auto bodyNeeds = needs != nullptr ? needs->needs : creature_physiology::Needs {};

		CreatureFighting fighting {
		    .stage = CreatureFighting::Stage::FaceOff,
		    .opponent = other,
		    .madeArena = self == creature,
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
	const bool planned = _startReason == "plan" || _startReason == "anger";
	registry.Get<CreatureFighting>(creature).planned = planned;
	// The fight has the minds of both creatures
	if (Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().FightStarted(creature, opponent, planned);
	}

	if (fight_transcript::Recording())
	{
		using fight_transcript::Id;
		// A fight that a script or a tool asks for is a script's fight when a script controls either creature
		const bool scripted = registry.AnyOf<ScriptControlled>(creature) || registry.AnyOf<ScriptControlled>(opponent);
		const auto reason = scripted && _startReason == "request" ? std::string_view("script") : _startReason;
		Locator::fightTranscript::value().Begin(creature, opponent,
		                                        {{"first", Id(creature)}, {"second", Id(opponent)}, {"reason", reason}});
		for (const auto self : {creature, opponent})
		{
			auto fields = Levels(registry, self);
			const auto& body = registry.Get<const Creature>(self);
			fields.emplace_back("species", static_cast<int32_t>(body.species));
			fields.emplace_back("owner", static_cast<int32_t>(body.owner));
			fields.emplace_back("script_controlled", registry.AllOf<ScriptControlled>(self));
			Locator::fightTranscript::value().Note(self, "fighter", std::move(fields), std::nullopt);
			_seen.erase(self);
			_actionCause.erase(self);
		}
	}
	_startReason = "request";

	return StartResult::Started;
}

namespace
{
/// A fight's arena is on from when it appears until the duel is over
bool ArenaOn(const ecs::Registry& registry, entt::entity creature)
{
	const auto* fighting = registry.Valid(creature) ? registry.TryGet<const CreatureFighting>(creature) : nullptr;
	return fighting != nullptr && fighting->arenaEntity != entt::null && fighting->stage <= CreatureFighting::Stage::Duel;
}

fight_view::Fighter ViewedFighter(const ecs::Registry& registry, entt::entity creature)
{
	const auto* locomotion = registry.TryGet<const CreatureLocomotion>(creature);
	return {.position = registry.Get<const Transform>(creature).position,
	        .radius = locomotion != nullptr ? locomotion->radius : 5.0f,
	        .height = HeightOf(ShownSize(registry.Get<const Creature>(creature)))};
}
} // namespace

void CreatureFightSystem::Reset()
{
	_pressed.reset();
	_markHeld = false;
	EndView("new_land");
	_lookSeconds = 0.0f;
	_seen.clear();
	_actionCause.clear();
	_flightPlace.reset();
	_screenCentre.reset();
	_fightExit = true;
	if (Locator::particleSystem::has_value())
	{
		Locator::particleSystem::value().SetFightMarks({});
	}
}

void CreatureFightSystem::DrawMarks(float realMs, const glm::vec3& eye)
{
	std::vector<particles::draw::FightMark> drawn;
	auto& registry = Locator::entitiesRegistry::value();
	const bool step = _marksPace.Advance(realMs);
	// Only the local player's creature shows its sparkles, for as long as it has an opponent
	const auto self = PrimaryCreatureOf(registry, k_LocalPlayer);
	auto* fighting = self.has_value() ? registry.TryGet<CreatureFighting>(*self) : nullptr;
	if (fighting != nullptr && registry.Valid(fighting->opponent))
	{
		auto& list = fighting->fighter.marks;
		if (step)
		{
			list.Animate();
		}
		const auto halfSize = ShownSize(registry.Get<const Creature>(*self));
		const auto shown = list.Marks();
		for (size_t place = 0; place < shown.size(); ++place)
		{
			const auto& mark = shown[place];
			if (const auto at = MarkPosition(registry, *self, fighting->opponent, mark))
			{
				drawn.push_back({.position = marks::DrawnAt(*at, eye),
				                 .halfSize = halfSize,
				                 .uv = marks::FrameUv(mark.frame),
				                 .colour = marks::Rgba(marks::ColourOf(place))});
			}
		}
	}
	if (Locator::particleSystem::has_value())
	{
		Locator::particleSystem::value().SetFightMarks(std::move(drawn));
	}
}

void CreatureFightSystem::UpdateView(float seconds)
{
	// Only this update uses the double click
	const auto flightPlace = std::exchange(_flightPlace, std::nullopt);
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
				EndView("fight_over");
			}
			return;
		}
		if (ArenaOn(registry, _view->first) && ArenaOn(registry, _view->second))
		{
			model->SetFighters(ViewedFighter(registry, _view->first), ViewedFighter(registry, _view->second));
			// Zoomed far enough out, the player leaves the fight, if they may
			if (model->ZoomedOut() && _fightExit)
			{
				EndView("zoomed_out");
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
	// A double click that sends the camera into the arena of a fight shows that fight at once. Only the first arena that
	// holds the place counts.
	if (flightPlace.has_value())
	{
		std::optional<std::pair<entt::entity, entt::entity>> into;
		fight::Arena intoArena;
		registry.Each<const CreatureFighting>([&](entt::entity entity, const CreatureFighting& fighting) {
			if (!into.has_value() && fighting.madeArena && fight::InArena(fighting.arena, flightPlace->place))
			{
				into = std::pair(entity, fighting.opponent);
				intoArena = fighting.arena;
			}
		});
		if (into.has_value())
		{
			if (ArenaOn(registry, into->first) && ArenaOn(registry, into->second))
			{
				TryStartView(into->first, into->second, intoArena, "double_click",
				             std::pair(flightPlace->eye, flightPlace->focus));
			}
			return;
		}
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
		TryStartView(found->first, found->second, foundArena, "look");
	}
}

void CreatureFightSystem::TryStartView(entt::entity first, entt::entity second, const fight::Arena& arena, std::string_view how,
                                       std::optional<std::pair<glm::vec3, glm::vec3>> from)
{
	auto& camera = Locator::camera::value();
	const auto& registry = Locator::entitiesRegistry::value();
	const auto own = PrimaryCreatureOf(registry, k_LocalPlayer);
	const bool involved = own == first || own == second;
	const bool scripted = registry.AnyOf<ScriptControlled>(first) || registry.AnyOf<ScriptControlled>(second);
	// The camera is too far when it is far from the arena now, and so is the land in the middle of the screen. A double
	// click measures from where the camera was before its flight.
	const auto [eye, looking] = from.has_value() ? *from : ViewNow();
	// The player's creature's fight under a script's control is always watched; any other only from near enough
	const bool tooFar =
	    !(involved && scripted) && fight_view::TooFar({eye.x, eye.z}, {looking.x, looking.z}, arena.centre, arena.radius, 1.0f);
	fight_transcript::Note(first, tooFar ? "view_refused" : "view_on", [&] {
		return fight_transcript::Fields {{"how", how},
		                                 {"why", tooFar ? "too_far" : "shown"},
		                                 {"players_creature", involved},
		                                 {"script_controlled", scripted},
		                                 {"eye_x", eye.x},
		                                 {"eye_y", eye.y},
		                                 {"eye_z", eye.z},
		                                 {"focus_x", looking.x},
		                                 {"focus_y", looking.y},
		                                 {"focus_z", looking.z}};
	});
	if (tooFar)
	{
		// For a fight of the player's creature, the advisors can say one of their lines about the fight instead
		if (involved && Locator::guidanceSystem::has_value())
		{
			Locator::guidanceSystem::value().RemarkIfNow(guidance::Kind::CreatureFight);
		}
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

void CreatureFightSystem::EndView(std::string_view why)
{
	if (_view.has_value())
	{
		fight_transcript::Note(_view->first, "view_off", [why] { return fight_transcript::Fields {{"why", why}}; });
	}
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
			NoteFightOver(self, "aborted");
			EndFightFor(self, other == nullptr || side->fighter.health >= other->fighter.health);
			NoteStage(self, side->stage, CreatureFighting::Stage::Respond);
			side->stage = CreatureFighting::Stage::Respond;
			side->played = false;
			fight::Enter(side->fighter, fight::State::Finish);
		}
		else if (IsLeadIn(side->stage))
		{
			Leave(self);
		}
	}
}

void CreatureFightSystem::CallOff(entt::entity creature)
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
		if (side->stage == CreatureFighting::Stage::Duel)
		{
			// Neither wins: each finishes what it is doing and stands, and its fight is over with nothing more to it
			NoteFightOver(self, "called_off");
			EndFightFor(self, std::nullopt);
			NoteStage(self, side->stage, CreatureFighting::Stage::Respond);
			side->stage = CreatureFighting::Stage::Respond;
			side->played = true;
			side->fighter.stopping = true;
			side->fighter.queue.Clear();
			side->fighter.marks.Clear();
		}
		else if (IsLeadIn(side->stage))
		{
			Leave(self);
		}
	}
}

bool CreatureFightSystem::IsSettingUpFight(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* fighting = registry.Valid(creature) ? registry.TryGet<const CreatureFighting>(creature) : nullptr;
	return fighting != nullptr && IsLeadIn(fighting->stage);
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
		EndView("creature_withdrawn");
	}
}

bool CreatureFightSystem::IsFighting(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(creature) && registry.AllOf<CreatureFighting>(creature);
}

bool CreatureFightSystem::IsDuelling(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(creature) && InDuel(registry, creature);
}

void CreatureFightSystem::TakeForScript(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature))
	{
		return;
	}
	const bool fought = registry.AllOf<CreatureFighting>(creature);
	if (fought)
	{
		CallOff(creature);
	}
	auto* knockedOut = registry.TryGet<CreatureKnockedOut>(creature);
	const bool roused = knockedOut != nullptr && !knockedOut->permanent;
	if (roused)
	{
		// It gets up where it is. If it was on its way home, it is back in full sight at once.
		StopCarryingHome(creature, *knockedOut);
		if (auto* animation = registry.TryGet<CreatureAnimation>(creature))
		{
			animation->body = creature_layers::EndLoop(animation->body);
		}
		SetEyes(registry, creature, creature_eyes::Mode::Calm);
		if (auto* needs = registry.TryGet<CreatureNeeds>(creature))
		{
			needs->rest = CreatureNeeds::Rest::Awake;
		}
		registry.Remove<CreatureKnockedOut>(creature);
	}
	if (fought || roused)
	{
		fight_transcript::Note(creature, "taken_by_script", [&] {
			return fight_transcript::Fields {
			    {"creature", fight_transcript::Id(creature)}, {"called_off", fought}, {"got_up", roused}};
		});
	}
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
	const auto controlBefore = fighting->fighter.control;
	const bool queued = fight::PlayerMove(fighting->fighter, move, replace);
	const auto index = fight_transcript::Note(creature, "move_queued", [&] {
		return fight_transcript::Fields {{"creature", fight_transcript::Id(creature)},
		                                 {"move", fight::Name(move.kind)},
		                                 {"value", move.value},
		                                 {"replace", replace},
		                                 {"queued", queued},
		                                 {"source", _queueSource},
		                                 {"queue", static_cast<uint32_t>(fighting->fighter.queue.Size())},
		                                 {"blows_queued", fight::QueuedBlows(fighting->fighter.queue.Moves())}};
	});
	if (controlBefore != fighting->fighter.control)
	{
		fight_transcript::Note(
		    creature, "control",
		    [&] {
			    return fight_transcript::Fields {{"creature", fight_transcript::Id(creature)},
			                                     {"from", fight::Name(controlBefore)},
			                                     {"to", fight::Name(fighting->fighter.control)},
			                                     {"why", "move_given"},
			                                     {"computer_wait_ms", fighting->fighter.computerWaitMs}};
		    },
		    index);
	}
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
	if (!registry.Valid(creature) || !registry.AllOf<Creature>(creature))
	{
		return;
	}
	// The computer fights it, or nobody does and it makes only the moves queued for it
	const auto control = autoFight ? fight::Control::Computer : fight::Control::None;
	if (auto* fighting = registry.TryGet<CreatureFighting>(creature))
	{
		fight_transcript::Note(creature, "control", [&] {
			return fight_transcript::Fields {{"creature", fight_transcript::Id(creature)},
			                                 {"from", fight::Name(fighting->fighter.control)},
			                                 {"to", fight::Name(control)},
			                                 {"why", "script"}};
		});
		fighting->fighter.control = control;
	}
	auto& record = registry.AllOf<CreatureFightRecord>(creature) ? registry.Get<CreatureFightRecord>(creature)
	                                                             : registry.Assign<CreatureFightRecord>(creature);
	record.control = control;
}

bool CreatureFightSystem::IsAutoFighting(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature))
	{
		return false;
	}
	// Anyone choosing its moves, the player or the computer
	if (const auto* fighting = registry.TryGet<const CreatureFighting>(creature))
	{
		return fighting->fighter.control != fight::Control::None;
	}
	const auto* record = registry.TryGet<const CreatureFightRecord>(creature);
	return record != nullptr && record->control != fight::Control::None;
}

fight::FightAction CreatureFightSystem::CurrentFightAction(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature))
	{
		return fight::FightAction::Other;
	}
	if (const auto* fighting = registry.TryGet<const CreatureFighting>(creature))
	{
		return fight::FightActionOf(fighting->fighter.animation);
	}
	return registry.AllOf<CreatureKnockedOut>(creature) ? fight::FightAction::Fainted : fight::FightAction::Other;
}

uint32_t CreatureFightSystem::QueuedBlows(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* fighting = registry.Valid(creature) ? registry.TryGet<const CreatureFighting>(creature) : nullptr;
	return fighting != nullptr ? fight::QueuedBlows(fighting->fighter.queue.Moves()) : 0;
}

float CreatureFightSystem::GetFightHealth(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature))
	{
		return 1.0f;
	}
	if (const auto* fighting = registry.TryGet<const CreatureFighting>(creature))
	{
		return fighting->fighter.health;
	}
	const auto* record = registry.TryGet<const CreatureFightRecord>(creature);
	return record != nullptr ? record->health : 1.0f;
}

void CreatureFightSystem::SetFightHealth(entt::entity creature, float health)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature))
	{
		return;
	}
	if (auto* fighting = registry.TryGet<CreatureFighting>(creature))
	{
		const auto before = fighting->fighter.health;
		fighting->fighter.health = health;
		fight_transcript::Note(creature, "level", [&] { return LevelChange(creature, "health", before, health, "script"); });
	}
	auto& record = registry.AllOf<CreatureFightRecord>(creature) ? registry.Get<CreatureFightRecord>(creature)
	                                                             : registry.Assign<CreatureFightRecord>(creature);
	record.health = health;
}

std::optional<CreatureFightSystemInterface::FoundArena>
CreatureFightSystem::FindOrMakeArena(const glm::vec3& point, entt::entity creature, entt::entity other, float within)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* first = registry.Valid(creature) ? registry.TryGet<const Creature>(creature) : nullptr;
	const auto* second = registry.Valid(other) ? registry.TryGet<const Creature>(other) : nullptr;
	if (first == nullptr || second == nullptr)
	{
		return std::nullopt;
	}
	const glm::ivec2 start {map_coords::ToFixed(point.x), map_coords::ToFixed(point.z)};
	// An arena found is taken as it is, whatever its size
	if (const auto nearest = NearestArena(registry, start, within))
	{
		return FoundArena {.arena = *nearest, .made = false};
	}
	const float wanted = fight::ArenaRadius(ShownSize(*first), ShownSize(*second));
	if (const auto made = MakeArena(registry, start, wanted, ShownSize(*first)))
	{
		return FoundArena {.arena = *made, .made = true};
	}
	return std::nullopt;
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

bool CreatureFightSystem::DirectsFight() const
{
	return fight::DirectsFight(PlayersFighter().has_value(), _view.has_value(),
	                           HandHoldsNothing(Locator::entitiesRegistry::value()));
}

std::pair<glm::vec3, glm::vec3> CreatureFightSystem::ViewNow()
{
	if (!Locator::camera::has_value())
	{
		return {glm::vec3(0.0f), _screenCentre.value_or(glm::vec3(0.0f))};
	}
	const auto& camera = Locator::camera::value();
	if (const auto hit = camera.RaycastScreenCoordToLand({0.5f, 0.5f}, true, Camera::Interpolation::Drawn); hit.has_value())
	{
		_screenCentre = hit->position;
	}
	return {camera.GetOrigin(Camera::Interpolation::Drawn),
	        _screenCentre.value_or(camera.GetFocus(Camera::Interpolation::Drawn))};
}

void CreatureFightSystem::FlewTo(const glm::vec3& place)
{
	// The test of distance uses the camera as it is before the flight moves it
	const auto [eye, focus] = ViewNow();
	_flightPlace = Flight {.place = glm::vec2(place.x, place.z), .eye = eye, .focus = focus};
}

bool CreatureFightSystem::Press(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, fight::Button button,
                                uint32_t milliseconds, uint32_t turn)
{
	auto& registry = Locator::entitiesRegistry::value();
	// The transcript keeps each press while the player's creature is in a fight, with what the fight did with it
	const auto note = [&](std::string_view result, fight_transcript::Fields more) {
		if (!fight_transcript::Recording())
		{
			return;
		}
		const auto own = PrimaryCreatureOf(registry, k_LocalPlayer);
		if (!own.has_value() || !registry.AllOf<CreatureFighting>(*own))
		{
			return;
		}
		const auto pointer =
		    Locator::gameActionSystem::has_value() ? Locator::gameActionSystem::value().GetPointerPosition() : glm::ivec2(0);
		fight_transcript::Fields fields {{"creature", fight_transcript::Id(*own)},
		                                 {"button", button == fight::Button::Action ? "action" : "move"},
		                                 {"result", result},
		                                 {"pointer_x", pointer.x},
		                                 {"pointer_y", pointer.y},
		                                 {"ray_x", rayOrigin.x},
		                                 {"ray_y", rayOrigin.y},
		                                 {"ray_z", rayOrigin.z},
		                                 {"direction_x", rayDirection.x},
		                                 {"direction_y", rayDirection.y},
		                                 {"direction_z", rayDirection.z}};
		std::ranges::move(more, std::back_inserter(fields));
		Locator::fightTranscript::value().Note(*own, "press", std::move(fields), std::nullopt);
	};
	const auto self = PlayersFighter();
	if (!self.has_value() || !DirectsFight())
	{
		note("refused_gate", {{"duelling", self.has_value()},
		                      {"camera_shows_fight", _view.has_value()},
		                      {"hand_empty", HandHoldsNothing(registry)}});
		return false;
	}
	auto& fighting = registry.Get<CreatureFighting>(*self);
	// Nothing more is taken while twelve sparkles show
	if (fighting.fighter.marks.Full())
	{
		note("refused_full", {{"queue", static_cast<uint32_t>(fighting.fighter.queue.Size())}});
		return false;
	}
	const auto opponent = fighting.opponent;
	const auto& selfAt = registry.Get<const Transform>(*self).position;
	const auto& opponentAt = registry.Get<const Transform>(opponent).position;
	const auto opponentHeight = HeightOf(ShownSize(registry.Get<const Creature>(opponent)));
	// Over the player's creature or its opponent, the press is theirs: it counts only where the line meets their skin
	const auto under = CreatureUnderHand(registry);
	if (under.has_value() && *under != *self && *under != opponent)
	{
		note("refused_other_creature", {{"under", fight_transcript::Id(*under)}});
		return false;
	}

	fight::Move move;
	marks::Mark mark;
	fight_transcript::Fields target;
	if (under.has_value())
	{
		const auto hit = SkinHitOn(registry, *under, rayOrigin, rayDirection);
		if (!hit.has_value())
		{
			note("missed_skin", {{"target", *under == opponent ? "opponent" : "self"}});
			return true;
		}
		if (*under == opponent)
		{
			// High, in the middle or low by where on the opponent's body the press lands
			const auto share = (hit->hit.point.y - opponentAt.y) / opponentHeight;
			move = fight::AttackMove(fight::BandOf(share));
			mark = MarkOnSkin(*hit, marks::Anchor::Opponent);
			target = {{"target", "opponent"}, {"height_share", share}, {"part", fight::Name(fight::BandOf(share))}};
		}
		else
		{
			move = fight::BlockMove();
			mark = MarkOnSkin(*hit, marks::Anchor::Self);
			target = {{"target", "self"}};
		}
		target.emplace_back("hit_x", hit->hit.point.x);
		target.emplace_back("hit_y", hit->hit.point.y);
		target.emplace_back("hit_z", hit->hit.point.z);
	}
	else
	{
		// The arena's ground: a step towards where it was pressed
		if (rayDirection.y >= 0.0f)
		{
			note("refused_sky", {});
			return false;
		}
		const auto along = (selfAt.y - rayOrigin.y) / rayDirection.y;
		const auto point = Flat(rayOrigin + (rayDirection * along));
		if (!fight::GroundPressCounts(fighting.arena, point))
		{
			note("refused_outside_arena", {{"ground_x", point.x}, {"ground_z", point.y}});
			return false;
		}
		const auto heading = registry.Get<const CreatureLocomotion>(*self).heading;
		const auto step = fight::StepTowards(WorldToLocal(point - Flat(selfAt), heading));
		move = fight::StepMove(step);
		const glm::vec3 ground {point.x, GroundAt(point), point.y};
		mark = {.anchor = marks::Anchor::Ground, .local = marks::ToLocal(marks::GroundFrame(selfAt, heading), ground)};
		target = {{"target", "ground"}, {"ground_x", point.x}, {"ground_z", point.y}, {"step", fight::Name(step)}};
	}
	// The sparkle goes up first; a press on a body charges it while held, one on the ground doesn't
	fighting.fighter.marks.Add(mark);
	_markHeld = mark.anchor != marks::Anchor::Ground;
	PlayDirectSound();
	target.emplace_back("move", fight::Name(move.kind));
	note("accepted", std::move(target));
	_queueSource = "hand";
	QueueMove(*self, move, fight::ReplacesQueue(button));
	_queueSource = "script";
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
	_markHeld = false;
	const auto realMs = milliseconds >= pressed.milliseconds ? milliseconds - pressed.milliseconds : 0u;
	const auto turns = turn >= pressed.turn ? turn - pressed.turn : 0u;
	const auto heldMs = fight::HeldMs(static_cast<float>(realMs), turns);
	fight_transcript::Note(pressed.creature, "release", [&] {
		return fight_transcript::Fields {{"creature", fight_transcript::Id(pressed.creature)},
		                                 {"real_ms", realMs},
		                                 {"turns", turns},
		                                 {"held_ms", heldMs},
		                                 {"charge_ms", fight::ReleasedCharge(heldMs)},
		                                 {"blow_speed", fight::BlowSpeed(fight::ReleasedCharge(heldMs))}};
	});
	ReleaseCharge(pressed.creature, heldMs);
}

std::optional<fight::Tip> CreatureFightSystem::HandTip(std::optional<entt::entity> under) const
{
	const auto self = PlayersFighter();
	if (!self.has_value() || !DirectsFight())
	{
		return std::nullopt;
	}
	const auto opponent = Locator::entitiesRegistry::value().Get<const CreatureFighting>(*self).opponent;
	return fight::TipOver(under == self, under == opponent);
}

std::optional<posed_model::SkinHit> CreatureFightSystem::SeesOpponent(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto opponent = registry.Get<const CreatureFighting>(creature).opponent;
	// The line from the fighter's groin through the opponent's must meet the opponent's skin
	const auto from = creature_scars::GroinOf(creature);
	const auto to = creature_scars::GroinOf(opponent);
	if (!from.has_value() || !to.has_value())
	{
		return std::nullopt;
	}
	return SkinHitOn(registry, opponent, *from, *to - *from);
}

bool CreatureFightSystem::GestureMove(const fight::Move& move)
{
	const auto self = PlayersFighter();
	auto& registry = Locator::entitiesRegistry::value();
	if (!self.has_value() || registry.Get<const CreatureFighting>(*self).fighter.marks.Full())
	{
		return false;
	}
	const auto seen = SeesOpponent(*self);
	if (!seen.has_value())
	{
		return false;
	}
	// Its sparkle sits where the line to the opponent meets its skin; a gesture charges nothing
	auto& fighting = registry.Get<CreatureFighting>(*self);
	fighting.fighter.marks.Add(MarkOnSkin(*seen, marks::Anchor::Opponent));
	_markHeld = false;
	_queueSource = "gesture";
	QueueMove(*self, move, false);
	_queueSource = "script";
	return true;
}

bool CreatureFightSystem::GestureSpecialMove()
{
	return GestureMove({.kind = fight::Move::Kind::Special});
}

bool CreatureFightSystem::GestureSpell(MagicType type)
{
	return GestureMove({.kind = fight::Move::Kind::Spell, .value = static_cast<uint32_t>(type)});
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
	const auto index = fight_transcript::Note(creature, "recoil", [&] {
		return fight_transcript::Fields {{"creature", fight_transcript::Id(creature)},
		                                 {"why", "miracle"},
		                                 {"blocking", IsBlocking(creature)},
		                                 {"state", fight::Name(fighter.state)}};
	});
	if (index.has_value())
	{
		_actionCause[creature] = *index;
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
	if (auto* fighting = registry.TryGet<CreatureFighting>(creature))
	{
		if (fighting->stage == CreatureFighting::Stage::Duel && InDuel(registry, fighting->opponent))
		{
			Win(fighting->opponent, creature, false);
			return;
		}
		Leave(creature);
	}
	Faint(creature);
}

void CreatureFightSystem::ForceFaint(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !registry.AllOf<Creature, CreatureAnimation>(creature) ||
	    registry.AllOf<CreatureKnockedOut>(creature))
	{
		return;
	}
	if (registry.AllOf<CreatureFighting>(creature))
	{
		Leave(creature);
	}
	Faint(creature);
}

void CreatureFightSystem::Faint(entt::entity creature)
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
	// It is taken to its home: its temple's pen while the temple stands, else the home it was last given (where it was
	// made, unless a script moved it). Only a creature with no home at all stays where it lies.
	const auto here = registry.Get<const Transform>(creature).position;
	const auto home = creature_home::HomeOf(registry, creature).value_or(here);
	registry.AssignOrReplace<CreatureKnockedOut>(creature, CreatureKnockedOut {.rest = rest, .home = home});
	// The people and the creatures near it react to the faint as to a death. The reaction belongs to no player.
	if (Locator::reactionSystem::has_value())
	{
		Locator::reactionSystem::value().Create(
		    {.initiator = creature, .type = Reaction::ReactToDeath, .position = here, .playerless = true});
	}
	fight_transcript::Note(creature, "faint", [&] {
		auto fields = Levels(registry, creature);
		fields.emplace_back("rest_later", rest);
		fields.emplace_back("at_x", here.x);
		fields.emplace_back("at_z", here.z);
		fields.emplace_back("home_x", home.x);
		fields.emplace_back("home_z", home.z);
		fields.emplace_back("script_controlled", registry.AllOf<ScriptControlled>(creature));
		return fields;
	});
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
		side.name = registry.Get<const Creature>(entity).name;
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
	StartFightsFromMinds();
	ProcessStages();
	ProcessDuels();
	ProcessKnockedOut();
	NoteActions();
}

void CreatureFightSystem::NoteActions()
{
	if (!fight_transcript::Recording())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	registry.Each<const CreatureFighting>([this](entt::entity entity, const CreatureFighting& fighting) {
		const auto& fighter = fighting.fighter;
		const auto seen = _seen.find(entity);
		if (seen != _seen.end() && seen->second.state == fighter.state && seen->second.animation == fighter.animation)
		{
			return;
		}
		_seen[entity] = {.state = fighter.state, .animation = fighter.animation};
		std::optional<uint64_t> cause;
		if (const auto found = _actionCause.find(entity); found != _actionCause.end())
		{
			cause = found->second;
			_actionCause.erase(found);
		}
		Locator::fightTranscript::value().Note(
		    entity, "action",
		    {{"creature", fight_transcript::Id(entity)},
		     {"state", fight::Name(fighter.state)},
		     {"animation", static_cast<uint64_t>(fighter.animation)},
		     {"fight_action", fight::Name(fight::FightActionOf(fighter.animation))},
		     {"speed", fighter.speed},
		     {"special", fighter.special},
		     {"mirrored", fighter.mirrored},
		     {"spell", fighter.spell.has_value() ? static_cast<int64_t>(*fighter.spell) : int64_t {-1}},
		     {"queue", static_cast<uint32_t>(fighter.queue.Size())},
		     {"stage", Name(fighting.stage)}},
		    cause);
	});
}

void CreatureFightSystem::NoteStage(entt::entity creature, CreatureFighting::Stage from, CreatureFighting::Stage to)
{
	if (from == to)
	{
		return;
	}
	fight_transcript::Note(creature, "stage", [&] {
		const auto* fighting = Locator::entitiesRegistry::value().TryGet<const CreatureFighting>(creature);
		return fight_transcript::Fields {{"creature", fight_transcript::Id(creature)},
		                                 {"from", Name(from)},
		                                 {"to", Name(to)},
		                                 {"stage_seconds", fighting != nullptr ? fighting->stageSeconds : 0.0f}};
	});
}

void CreatureFightSystem::NoteFightOver(entt::entity creature, std::string_view reason)
{
	if (!fight_transcript::Recording())
	{
		return;
	}
	auto& transcript = Locator::fightTranscript::value();
	const auto fight = transcript.GetRecorder().FightOf(entt::to_integral(creature));
	const auto* found = fight.has_value() ? transcript.GetRecorder().Find(*fight) : nullptr;
	// Only the first reason counts, as each side of a fight ends it
	if (found == nullptr || found->ended.has_value())
	{
		return;
	}
	transcript.Note(creature, "fight_over", {{"creature", fight_transcript::Id(creature)}, {"reason", reason}}, std::nullopt);
	transcript.End(creature);
}

CreatureFightSystemInterface::StartResult CreatureFightSystem::StartPlannedFight(entt::entity creature, entt::entity opponent)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* mind = registry.Valid(creature) ? registry.TryGet<const CreatureMindState>(creature) : nullptr;
	const bool angry = mind != nullptr && mind->planner.current.has_value() &&
	                   mind->planner.current->desire == creature_desires::Desire::Anger;
	_startReason = angry ? "anger" : "plan";
	const auto result = StartFight(creature, opponent);
	_startReason = "request";
	return result;
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

	for (const auto& [creature, opponent] : wanted)
	{
		_startReason = "leash";
		StartFight(creature, opponent);
	}
	_startReason = "request";
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
		const auto next = [this, entity, fighting](CreatureFighting::Stage stage) {
			NoteStage(entity, fighting->stage, stage);
			fighting->stage = stage;
			fighting->stageSeconds = 0.0f;
			fighting->played = false;
		};

		// The creature that started the fight tells the other creature when to do each step before the duel
		const auto* partner = opponentHere ? registry.TryGet<const CreatureFighting>(opponent) : nullptr;
		const bool together = partner != nullptr && partner->opponent == entity;
		const bool starter = fighting->madeArena;
		const auto order = [this, &registry](entt::entity who, CreatureFighting::Stage stage) {
			if (auto* side = registry.TryGet<CreatureFighting>(who))
			{
				NoteStage(who, side->stage, stage);
				side->stage = stage;
				side->stageSeconds = 0.0f;
				side->played = false;
			}
		};
		if (IsLeadIn(fighting->stage) && !together)
		{
			Leave(entity);
			continue;
		}

		switch (fighting->stage)
		{
		case CreatureFighting::Stage::FaceOff:
		case CreatureFighting::Stage::Square:
			if (!fighting->played)
			{
				fighting->played = true;
				if (locomotion != nullptr)
				{
					locomotion->Stop(entity);
					locomotion->TurnToFace(entity, Flat(registry.Get<const Transform>(opponent).position));
				}
			}
			else if (starter && (!moving(entity) || fighting->stageSeconds > k_ApproachSeconds))
			{
				if (fighting->stage == CreatureFighting::Stage::FaceOff)
				{
					order(opponent, CreatureFighting::Stage::Taunt);
					next(CreatureFighting::Stage::Taunt);
				}
				else
				{
					// Both start together, so neither finds the other not yet duelling
					BeginDuel(entity);
					BeginDuel(opponent);
				}
			}
			break;
		case CreatureFighting::Stage::Taunt:
			if (!fighting->played)
			{
				fighting->played = true;
				if (locomotion != nullptr)
				{
					locomotion->Stop(entity);
				}
				// Each creature plays the taunt or the rude gesture, half and half. A species without the action plays
				// nothing.
				const auto clip = fight::FaceOffAnimation(std::bernoulli_distribution(0.5)(_random));
				fighting->taunted = Has(entity, clip);
				if (fighting->taunted)
				{
					PlayOnce(animation, clip);
				}
				fight_transcript::Note(entity, "face_off", [&] {
					return fight_transcript::Fields {{"creature", fight_transcript::Id(entity)},
					                                 {"animation", static_cast<int32_t>(clip)},
					                                 {"played", fighting->taunted}};
				});
			}
			else if (!creature_layers::IsPlaying(animation.body))
			{
				if (!starter)
				{
					next(CreatureFighting::Stage::Ready);
				}
				else if (DrawArenaFor(registry, entity, opponent))
				{
					next(CreatureFighting::Stage::Point);
				}
				else
				{
					// With no arena the fight stops
					fight_transcript::Note(entity, "no_arena", [&] {
						return fight_transcript::Fields {{"creature", fight_transcript::Id(entity)}};
					});
					Leave(opponent);
					Leave(entity);
				}
			}
			break;
		case CreatureFighting::Stage::Point:
		{
			auto* hands =
			    Locator::creatureObjectActionSystem::has_value() ? &Locator::creatureObjectActionSystem::value() : nullptr;
			const bool busy = hands != nullptr && hands->GetState(entity) == CreatureObjectActionSystemInterface::State::Busy;
			if (!fighting->played)
			{
				// It points at the middle of the arena on the ground, when its hands are free of other work
				if (busy)
				{
					break;
				}
				fighting->played = true;
				if (hands != nullptr)
				{
					const auto centre = fighting->arena.centre;
					hands->PointAtFor(entity, {centre.x, GroundAt(centre), centre.y},
					                  static_cast<float>(fight::k_PointAtArenaTurns) * k_TurnSeconds, std::nullopt);
				}
			}
			else if (!busy)
			{
				if (hands != nullptr && hands->GetState(entity) != CreatureObjectActionSystemInterface::State::Idle)
				{
					hands->Cancel(entity);
				}
				order(opponent, CreatureFighting::Stage::Approach);
				next(CreatureFighting::Stage::Approach);
			}
			break;
		}
		case CreatureFighting::Stage::Approach:
			if (!fighting->played)
			{
				fighting->played = true;
				if (locomotion != nullptr)
				{
					const auto spot = fight::ArenaSpot(fighting->arena, ShownSize(body), fighting->madeArena);
					locomotion->MoveTo(entity, spot, Pace::Walk, 0.0f, fight::ArrivalDistance(ShownSize(body)));
				}
			}
			else if (!moving(entity) || fighting->stageSeconds > k_ApproachSeconds)
			{
				next(CreatureFighting::Stage::Ready);
			}
			break;
		case CreatureFighting::Stage::Ready:
			// The other creature waits for its next order. The first creature, at its place, waits for the other to arrive.
			if (!starter)
			{
				break;
			}
			if (partner->stage == CreatureFighting::Stage::Ready)
			{
				order(opponent, CreatureFighting::Stage::Square);
				next(CreatureFighting::Stage::Square);
			}
			else if (fighting->stageSeconds > k_ApproachSeconds)
			{
				AbortFight(entity);
			}
			break;
		case CreatureFighting::Stage::Duel:
			break;
		case CreatureFighting::Stage::Celebrate:
			// Its move finished, it leaves the fight. Its mind then does what the win made it do to the loser.
			if (fighting->fighter.state == fight::State::Idle)
			{
				Leave(entity);
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
				// The creature that has no more life than the other is made to fear the other
				const bool fears = opponentHere && fight::FearsAfterFight(LifeOf(registry, entity), LifeOf(registry, opponent));
				Leave(entity);
				if (fears && Locator::creatureMindSystem::has_value())
				{
					[[maybe_unused]] const bool planned =
					    Locator::creatureMindSystem::value().ForceDesireOn(entity, creature_desires::Desire::Fear, opponent);
					fight_transcript::Note(entity, "fear_after_fight", [&] {
						return fight_transcript::Fields {{"creature", fight_transcript::Id(entity)},
						                                 {"of", fight_transcript::Id(opponent)},
						                                 {"planned", planned}};
					});
					// The fear stops what the other creature does: its response ends, and its mind is free again.
					// Thus, when both have the same life, only the first of the two to finish its response fears.
					if (const auto* other =
					        registry.Valid(opponent) ? registry.TryGet<const CreatureFighting>(opponent) : nullptr;
					    other != nullptr && other->opponent == entity && other->stage == CreatureFighting::Stage::Respond)
					{
						fight_transcript::Note(opponent, "stopped_by_partner", [&] {
							return fight_transcript::Fields {{"creature", fight_transcript::Id(opponent)},
							                                 {"by", fight_transcript::Id(entity)}};
						});
						Leave(opponent);
					}
				}
			}
			break;
		}
	}
}

void CreatureFightSystem::BeginDuel(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& fighting = registry.Get<CreatureFighting>(creature);
	NoteStage(creature, fighting.stage, CreatureFighting::Stage::Duel);
	fighting.stage = CreatureFighting::Stage::Duel;
	fighting.stageSeconds = 0.0f;
	// A fight counts once the duel starts. A fight left before it does not count.
	auto& record = registry.AllOf<CreatureFightRecord>(creature) ? registry.Get<CreatureFightRecord>(creature)
	                                                             : registry.Assign<CreatureFightRecord>(creature);
	++record.fights;
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
			NoteFightOver(entity, "opponent_left");
			EndFightFor(entity, true);
			NoteStage(entity, fighting->stage, CreatureFighting::Stage::Respond);
			fighting->stage = CreatureFighting::Stage::Respond;
			fighting->played = false;
			fight::Enter(fighting->fighter, fight::State::Finish);
			continue;
		}
		auto& fighter = fighting->fighter;
		const auto staminaBefore = fighter.stamina;
		fighter.stamina = fight::RegainStamina(fighter.stamina);
		if (fighter.stamina != staminaBefore)
		{
			fight_transcript::Note(entity, "level",
			                       [&] { return LevelChange(entity, "stamina", staminaBefore, fighter.stamina, "regain"); });
		}
		const auto& other = registry.Get<const CreatureFighting>(opponent).fighter;

		if (fighter.state == fight::State::Stance && fighter.control == fight::Control::Player)
		{
			// Left alone long enough, the computer takes over the player's creature
			fighter.computerWaitMs -= k_TurnMs;
			if (fighter.computerWaitMs < 0.0f)
			{
				fighter.control = fight::Control::Computer;
				fight_transcript::Note(entity, "control", [&] {
					return fight_transcript::Fields {{"creature", fight_transcript::Id(entity)},
					                                 {"from", "player"},
					                                 {"to", "computer"},
					                                 {"why", "player_idle"},
					                                 {"computer_wait_ms", fighter.computerWaitMs}};
				});
			}
		}
		const bool computer = fighter.control == fight::Control::Computer;
		const bool facing = FacesOpponent(registry, entity, opponent);
		// Near the edge of the arena, it makes no moves and steps back to the middle
		const bool kept = facing && (fighter.state == fight::State::Stance || fighter.state == fight::State::Block) &&
		                  KeepInArena(registry, entity, *fighting);
		// Bodies that overlap move apart, in its stance and in its steps
		if ((facing && !kept && fighter.state == fight::State::Stance) ||
		    (fighter.state == fight::State::Action && !fight::IsAttack(fighter.animation)))
		{
			PullApartFighters(registry, entity, *fighting);
		}
		if (!facing || kept)
		{
			// It turns to its opponent first, or it keeps inside the arena
		}
		else if (!fighter.queue.Empty())
		{
			CheckQueue(entity);
		}
		else if (computer && fighter.state == fight::State::Stance)
		{
			const auto tier = fight::TierOf(fighter.tendency);
			const auto seen = fight::OpponentOf(other.state, other.animation);
			const auto choice = fight::ChooseMove(tier, seen, random);
			// The transcript keeps the turns on which the computer makes a move, not the turns on which it waits
			if (choice.kind != fight::Choice::Kind::None)
			{
				const auto index = fight_transcript::Note(entity, "ai_choice", [&] {
					return fight_transcript::Fields {{"creature", fight_transcript::Id(entity)},
					                                 {"tendency", fighter.tendency},
					                                 {"tier", tier},
					                                 {"opponent_state", fight::Name(seen)},
					                                 {"choice", fight::Name(choice.kind)},
					                                 {"band", fight::Name(choice.band)},
					                                 {"step", fight::Name(choice.step)}};
				});
				if (index.has_value())
				{
					_actionCause[entity] = *index;
				}
			}
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
			const auto index = fight_transcript::Note(entity, "ai_choice", [&] {
				return fight_transcript::Fields {{"creature", fight_transcript::Id(entity)}, {"choice", "end_block"}};
			});
			if (index.has_value())
			{
				_actionCause[entity] = *index;
			}
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
	const bool inRange = fight::WithinRange(fighting.arena, opponentAt);
	// The help record counts the moves of the player's creature
	if (fight::CountsHelpEvent(fighter.state, inRange, front.has_value()) &&
	    registry.Get<const Creature>(creature).owner == k_LocalPlayer && Locator::helpProfileSystem::has_value())
	{
		Locator::helpProfileSystem::value().Trigger(fight::HelpEventOf(front->move.kind));
	}
	const auto order = fight::TakeOrder(fighter, inRange);
	if (!order.has_value())
	{
		return;
	}
	const auto orderIndex = fight_transcript::Note(creature, "order", [&] {
		return fight_transcript::Fields {{"creature", fight_transcript::Id(creature)},
		                                 {"order", fight::Name(order->kind)},
		                                 {"band", fight::Name(order->band)},
		                                 {"value", order->value},
		                                 {"speed", order->speed},
		                                 {"in_range", inRange},
		                                 {"queue", static_cast<uint32_t>(fighter.queue.Size())}};
	});
	if (orderIndex.has_value())
	{
		_actionCause[creature] = *orderIndex;
	}
	// A human player's creature learns from each move it makes, whoever asked for it
	if (order->kind != fight::Order::Kind::EndBlock && front.has_value() &&
	    registry.Get<const Creature>(creature).owner == k_LocalPlayer)
	{
		const auto before = fighter.tendency;
		fighter.tendency = fight::LearnTendency(fighter.tendency, front->move.kind);
		fight_transcript::Note(
		    creature, "learning",
		    [&] {
			    return fight_transcript::Fields {{"creature", fight_transcript::Id(creature)},
			                                     {"what", "tendency"},
			                                     {"move", fight::Name(front->move.kind)},
			                                     {"before", before},
			                                     {"after", fighter.tendency}};
		    },
		    orderIndex);
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
	const auto note = [&](std::string_view use, entt::entity target, std::optional<bool> cast) {
		fight_transcript::Note(
		    creature, "spell",
		    [&] {
			    return fight_transcript::Fields {{"creature", fight_transcript::Id(creature)},
			                                     {"magic_type", static_cast<int32_t>(type)},
			                                     {"use", use},
			                                     {"target", fight_transcript::Id(target)},
			                                     {"result", !cast.has_value() ? "not_cast" : (*cast ? "cast" : "failed")}};
		    },
		    fight_transcript::LastOf(creature, "action"));
	};
	if (effect.isAggressiveSpellWhichIsUsedInCreatureFightArena != 0)
	{
		if (opponent != entt::null)
		{
			note("attack", opponent, Locator::creatureMindSystem::value().TryMiracle(creature, type, opponent));
		}
		else
		{
			note("attack", opponent, std::nullopt);
		}
	}
	else if (effect.isDefensiveSpellWhichIsUsedInCreatureFightArena != 0)
	{
		note("defence", creature, Locator::creatureMindSystem::value().TryMiracle(creature, type, creature));
	}
	else
	{
		// A miracle that the fight cannot use does nothing
		note("not_for_fights", entt::null, std::nullopt);
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
	const auto choice = fight::ChooseAttack(fighting.reaches, band, gap, stepLength, ShownSize(body),
	                                        HeightOf(ShownSize(registry.Get<const Creature>(opponent))));
	fight_transcript::Note(
	    creature, "attack_choice",
	    [&] {
		    return fight_transcript::Fields {
		        {"creature", fight_transcript::Id(creature)},
		        {"band", fight::Name(band)},
		        {"gap", gap},
		        {"step_length", stepLength},
		        {"animation", choice.animation.has_value() ? static_cast<int64_t>(*choice.animation) : int64_t {-1}},
		        {"step", choice.step.has_value() ? fight::Name(*choice.step) : std::string_view("none")},
		        {"speed", speed}};
	    },
	    _actionCause.contains(creature) ? std::optional(_actionCause.at(creature)) : std::nullopt);
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
	// A press held on a body charges the sparkle added last
	if (_markHeld)
	{
		if (const auto self = PrimaryCreatureOf(registry, k_LocalPlayer))
		{
			if (auto* held = registry.TryGet<CreatureFighting>(*self))
			{
				held->fighter.marks.Charge(milliseconds);
			}
		}
	}
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
		fighter.stateMs += milliseconds;

		// An action carries the creature as far as its animation's root moves. Only the reel in a block stops at the range.
		if ((fighter.state == fight::State::Action || fighter.state == fight::State::BlockRecoil) && duration > 0.0f)
		{
			const auto share = std::min(fighter.timeMs / duration, 1.0f);
			const auto scale = std::abs(registry.Get<const Transform>(entity).scale.x);
			auto by = MeshToWorld(DisplacementOf(body.species, fighter.animation) * scale, locomotion->heading) *
			          (share - fighting->movedShare);
			fighting->movedShare = share;
			const auto at = Flat(locomotion->toPosition);
			auto to = fighter.state == fight::State::BlockRecoil ? fight::BlockRecoilTo(fighting->arena, at, at + by) : at + by;
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
		// A blow lands as it reaches furthest, while the duel is on: a fight called off lands no more blows
		if (hasOpponent && fighter.state == fight::State::Action && fight::IsAttack(fighter.animation) && !fighter.landed)
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
		const bool done = fight::StateDone(current.state, current.timeMs, duration, current.stateMs);
		// Called off, it winds down: the move under way plays out, then it finishes and stands
		if (current.stopping)
		{
			if (const auto next = fight::StopTransition(current.state, done))
			{
				if (current.state == fight::State::Cast && Locator::magicSystem::has_value())
				{
					Locator::magicSystem::value().ReleaseCreatureCast(entity);
					fight_transcript::Note(entity, "spell_release", [entity] {
						return fight_transcript::Fields {{"creature", fight_transcript::Id(entity)}, {"why", "called_off"}};
					});
				}
				current.spell.reset();
				fight::Enter(current, *next);
				fighting->movedShare = 0.0f;
				Show(*animation, current, false, DurationOf(entity, current.animation));
				continue;
			}
		}
		if (done)
		{
			// The casting pose over, the creature lets go of the miracle: one that lasts only while held stops
			if (current.state == fight::State::Cast && Locator::magicSystem::has_value())
			{
				Locator::magicSystem::value().ReleaseCreatureCast(entity);
				fight_transcript::Note(entity, "spell_release", [entity] {
					return fight_transcript::Fields {{"creature", fight_transcript::Id(entity)}, {"why", "cast_over"}};
				});
			}
			// As the start of a cast ends, still facing its opponent, the spell is cast: an attacking one at the opponent, a
			// defending one on itself
			if (current.state == fight::State::CastStart && current.spell.has_value() && hasOpponent)
			{
				CastFightSpell(entity, static_cast<MagicType>(*current.spell), fighting->opponent);
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
	NoteActions();
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
	const auto tolerance = k_BlowReachPerSize * ShownSize(body) * (attacker.special ? 2.0f : 1.0f);
	const auto opponentHeight = HeightOf(ShownSize(opponentBody));
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
	// The blow follows from the action of the attacker that strikes it
	const auto blowCause = fight_transcript::LastOf(creature, "action");
	if (!hit)
	{
		fight_transcript::Note(
		    creature, "blow",
		    [&] {
			    return fight_transcript::Fields {{"attacker", fight_transcript::Id(creature)},
			                                     {"defender", fight_transcript::Id(opponent)},
			                                     {"animation", static_cast<uint64_t>(attacker.animation)},
			                                     {"outcome", "miss"},
			                                     {"point_x", point.x},
			                                     {"point_y", point.y},
			                                     {"point_z", point.z},
			                                     {"tolerance", tolerance},
			                                     {"special", attacker.special}};
		    },
		    blowCause);
		return;
	}
	// The victim remembers when it was struck, which the villagers watching cheer or despair at
	auto& struck = registry.AllOf<CreatureFightRecord>(opponent) ? registry.Get<CreatureFightRecord>(opponent)
	                                                             : registry.Assign<CreatureFightRecord>(opponent);
	struck.struckTurn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0u;

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
	        .attackerSize = ShownSize(body),
	        .defenderSize = ShownSize(opponentBody),
	        .attackerStrength = body.strength,
	        .defenderStrength = opponentBody.strength,
	        .speed = attacker.speed,
	        .special = attacker.special,
	        .heightShare = heightShare,
	        .blocked = blocked,
	    },
	    direction);
	const auto healthBefore = victim.health;
	victim.health = std::max(victim.health - result.damage, 0.0f);
	const auto blowIndex = fight_transcript::Note(
	    creature, "blow",
	    [&] {
		    return fight_transcript::Fields {{"attacker", fight_transcript::Id(creature)},
		                                     {"defender", fight_transcript::Id(opponent)},
		                                     {"animation", static_cast<uint64_t>(attacker.animation)},
		                                     {"outcome", blocked ? "blocked" : "hit"},
		                                     {"part", fight::Name(result.band)},
		                                     {"height_share", heightShare},
		                                     {"point_x", point.x},
		                                     {"point_y", point.y},
		                                     {"point_z", point.z},
		                                     {"tolerance", tolerance},
		                                     {"attacker_size", ShownSize(body)},
		                                     {"defender_size", ShownSize(opponentBody)},
		                                     {"attacker_strength", body.strength},
		                                     {"defender_strength", opponentBody.strength},
		                                     {"speed", attacker.speed},
		                                     {"special", attacker.special},
		                                     {"blocked", blocked},
		                                     {"damage_factor", fight::DamageFactor({.attackerSize = ShownSize(body),
		                                                                            .defenderSize = ShownSize(opponentBody),
		                                                                            .attackerStrength = body.strength,
		                                                                            .defenderStrength = opponentBody.strength,
		                                                                            .speed = attacker.speed,
		                                                                            .special = attacker.special,
		                                                                            .heightShare = heightShare,
		                                                                            .blocked = blocked})},
		                                     {"damage", result.damage},
		                                     {"direction", fight::Name(direction)},
		                                     {"recoil", static_cast<uint64_t>(result.recoil)},
		                                     {"plain_recoil", static_cast<uint64_t>(result.plainRecoil)},
		                                     {"health_before", healthBefore},
		                                     {"health_after", victim.health}};
	    },
	    blowCause);
	if (blowIndex.has_value())
	{
		fight_transcript::Note(
		    opponent, "level",
		    [&] { return LevelChange(opponent, "health", healthBefore, victim.health, blocked ? "blow_blocked" : "blow"); },
		    blowIndex);
		// The defender reels or recoils in its block because of the blow
		_actionCause[opponent] = *blowIndex;
	}
	// Getting hit takes away a charged blow still waiting, and the sparkle of the move added last
	const auto queuedBefore = victim.queue.Size();
	victim.queue.CancelWaiting();
	if (victim.queue.Size() != queuedBefore)
	{
		fight_transcript::Note(
		    opponent, "queue_cancelled",
		    [&] {
			    return fight_transcript::Fields {{"creature", fight_transcript::Id(opponent)},
			                                     {"why", "struck_while_charging"},
			                                     {"before", static_cast<uint32_t>(queuedBefore)},
			                                     {"after", static_cast<uint32_t>(victim.queue.Size())}};
		    },
		    blowIndex);
	}
	if (victim.queue.Size() != queuedBefore && victim.marks.Size() > 0)
	{
		victim.marks.DropNewest();
		_markHeld = false;
	}

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
		const auto u = static_cast<uint8_t>(texel(_random));
		const auto v = static_cast<uint8_t>(texel(_random));
		// The column is drawn first, then the kind by the wound type the attacker's species gives the blow
		const auto column = static_cast<uint8_t>(random(creature_marks::k_CellsPerRow));
		const auto* attackerRig = RigOf(body.species);
		const auto woundType = attackerRig != nullptr && attackerRig->blowWoundTypes.has_value()
		                           ? fight::BlowWoundType(attacker.animation, *attackerRig->blowWoundTypes)
		                           : -1;
		creature_marks::Mark wound {
		    .u = u,
		    .v = v,
		    .skin = 0,
		    .age = 0,
		    .type = fight::WoundKind(woundType, random),
		    .column = column,
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
			const auto by = between * (((closest - distance) * 0.5f) / distance);
			Shift(*moves, by);
			fight_transcript::Note(
			    creature, "pushed_back",
			    [&] {
				    return fight_transcript::Fields {{"creature", fight_transcript::Id(creature)},
				                                     {"by_x", by.x},
				                                     {"by_z", by.y},
				                                     {"distance", distance},
				                                     {"closest", closest}};
			    },
			    blowIndex);
		}
	}

	if (victim.health <= 0.0f)
	{
		Win(creature, opponent, true);
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

void CreatureFightSystem::Win(entt::entity winner, entt::entity loser, bool byBlow)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto winRecord = fight_transcript::Note(winner, "win", [&] {
		return fight_transcript::Fields {{"winner", fight_transcript::Id(winner)},
		                                 {"loser", fight_transcript::Id(loser)},
		                                 {"by_blow", byBlow},
		                                 {"winner_health", GetFightHealth(winner)},
		                                 {"loser_health", GetFightHealth(loser)}};
	});
	// The winner is made to do something to the loser at once: an evil winner has a poo on it, any other shows off. Its
	// mind does it when the winner has left the fight. This plan counts for the feedback of the player.
	if (Locator::creatureMindSystem::has_value() && registry.Valid(loser))
	{
		const auto alignment = registry.Get<const Creature>(winner).alignment;
		const std::string_view action = fight::WinnerPoos(alignment) ? "Poo" : "ShowImpressiveAnimation";
		const bool planned =
		    Locator::creatureMindSystem::value().ForceActivity(winner, {.desire = creature_desires::Desire::ObeyPlayer,
		                                                                .activityObject = loser,
		                                                                .action = action,
		                                                                .actionObject = loser,
		                                                                .instrument = std::nullopt});
		fight_transcript::Note(
		    winner, "forced_plan",
		    [&] {
			    return fight_transcript::Fields {
			        {"creature", fight_transcript::Id(winner)}, {"desire", "obey_player"}, {"action", action},
			        {"object", fight_transcript::Id(loser)},    {"alignment", alignment},  {"planned", planned}};
		    },
		    winRecord);
	}
	NoteFightOver(winner, "knockout");
	EndFightFor(winner, true);
	EndFightFor(loser, false);
	if (auto* record = registry.TryGet<CreatureFightRecord>(winner))
	{
		++record->wins;
	}
	Leave(loser);
	Faint(loser);
	// Both take up no reaction until what the knock-out made them do is over
	for (const auto self : {winner, loser})
	{
		if (const auto* mind = registry.Valid(self) ? registry.TryGet<const CreatureMindState>(self) : nullptr)
		{
			registry.AssignOrReplace<CreatureAfterFight>(self, CreatureAfterFight {.agenda = mind->idle.serial});
		}
	}
	if (auto* fighting = registry.TryGet<CreatureFighting>(winner))
	{
		NoteStage(winner, fighting->stage, CreatureFighting::Stage::Celebrate);
		fighting->stage = CreatureFighting::Stage::Celebrate;
		fighting->stageSeconds = 0.0f;
		fighting->played = false;
		fight::Enter(fighting->fighter, fight::State::Finish);
		auto& won = registry.Get<CreatureAnimation>(winner);
		won.face = creature_layers::RelaxFace(won.face);
	}
	// A fight won by a blow is seen by the living around the winner, if the winner has a player. The reaction runs at
	// once: the villagers' towns move the player's alignment by what the reaction table says of a fight won.
	const auto owner = registry.Get<const Creature>(winner).owner;
	if (fight::FightWonIsSeen(byBlow, owner != PlayerNames::NEUTRAL) && Locator::reactionSystem::has_value())
	{
		fight_transcript::Note(
		    winner, "reaction",
		    [&] {
			    return fight_transcript::Fields {{"creature", fight_transcript::Id(winner)},
			                                     {"reaction", "react_to_fight_won"},
			                                     {"player", static_cast<int32_t>(owner)}};
		    },
		    winRecord);
		// Those who take it up believe by how impressive the winner is: what it last did, its size, and how their town
		// feels about it
		Locator::reactionSystem::value().Create({.initiator = winner,
		                                         .type = Reaction::ReactToFightWon,
		                                         .player = owner,
		                                         .position = registry.Get<const Transform>(winner).position,
		                                         .onCast = true});
	}
}

void CreatureFightSystem::EndFightFor(entt::entity creature, std::optional<bool> won)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* fighting = registry.TryGet<CreatureFighting>(creature);
	if (fighting == nullptr || fighting->ended)
	{
		return;
	}
	fighting->ended = true;
	const auto ended = fight_transcript::Note(creature, "fight_end", [&] {
		return fight_transcript::Fields {{"creature", fight_transcript::Id(creature)},
		                                 {"result", !won.has_value() ? "none" : (*won ? "won" : "lost")},
		                                 {"health", fighting->fighter.health},
		                                 {"tendency", fighting->fighter.tendency},
		                                 {"control", fight::Name(fighting->fighter.control)}};
	});
	// A quarter of the fight health it lost comes off its life
	if (auto* needs = registry.TryGet<CreatureNeeds>(creature))
	{
		const auto before = needs->needs.life;
		needs->needs.life = fight::LifeAfterFight(needs->needs.life, fighting->fighter.health);
		fight_transcript::Note(
		    creature, "level", [&] { return LevelChange(creature, "life", before, needs->needs.life, "fight_end"); }, ended);
	}
	auto& record = registry.AllOf<CreatureFightRecord>(creature) ? registry.Get<CreatureFightRecord>(creature)
	                                                             : registry.Assign<CreatureFightRecord>(creature);
	record.tendency = fighting->fighter.tendency;
	record.control = fighting->fighter.control;
	record.foughtBefore = true;
	if (won.has_value() && Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().FoughtFight(creature, *won);
		fight_transcript::Note(
		    creature, "learning",
		    [&] {
			    return fight_transcript::Fields {
			        {"creature", fight_transcript::Id(creature)}, {"what", "fought_fight"}, {"won", *won}};
		    },
		    ended);
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
	auto& record = registry.AllOf<CreatureFightRecord>(creature) ? registry.Get<CreatureFightRecord>(creature)
	                                                             : registry.Assign<CreatureFightRecord>(creature);
	// Its fight health stays as the fight left it
	record.health = fighting->fighter.health;
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
		const auto next = [&knockedOut, entity](CreatureKnockedOut::Stage stage) {
			fight_transcript::Note(entity, "knocked_out", [&] {
				return fight_transcript::Fields {{"creature", fight_transcript::Id(entity)},
				                                 {"from", Name(knockedOut.stage)},
				                                 {"to", Name(stage)},
				                                 {"seconds", knockedOut.seconds}};
			});
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
			if (!knockedOut.permanent && knockedOut.seconds >= fight::FaintSeconds(ShownSize(body)))
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
					const auto from = registry.Get<const Transform>(entity).position;
					const auto to = creature_home::HomeOf(registry, entity).value_or(*knockedOut.home);
					PlaceAt(*locomotion, registry.Get<Transform>(entity), to);
					registry.SetDirty();
					fight_transcript::Note(entity, "taken_home", [&] {
						return fight_transcript::Fields {{"creature", fight_transcript::Id(entity)},
						                                 {"from_x", from.x},
						                                 {"from_z", from.z},
						                                 {"to_x", to.x},
						                                 {"to_z", to.z},
						                                 {"distance", glm::distance(Flat(from), Flat(to))},
						                                 {"script_controlled", registry.AllOf<ScriptControlled>(entity)}};
					});
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
		{
			// It rests until its life is full, it is not tired at all, and the rest has lasted long enough
			const auto turns = static_cast<uint32_t>(std::lround(knockedOut.seconds / k_TurnSeconds));
			if (needs == nullptr || (creature_mind::IsBetter(needs->needs.life, needs->needs.exhaustion) &&
			                         turns > creature_mind::k_FewestRestTurns))
			{
				getUp();
			}
			break;
		}
		case CreatureKnockedOut::Stage::GettingUp:
			if (!creature_layers::IsPlaying(animation.body))
			{
				if (needs != nullptr)
				{
					needs->rest = CreatureNeeds::Rest::Awake;
				}
				fight_transcript::Note(entity, "got_up", [&] { return Levels(registry, entity); });
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

bool CreatureFightSystem::FightMusicHeard(const glm::vec3& camera) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto creature = PrimaryCreatureOf(registry, k_LocalPlayer);
	if (!creature.has_value())
	{
		return false;
	}
	std::vector<glm::vec3> arenas;
	registry.Each<const CreatureArena, const Transform>(
	    [&arenas, own = *creature](const CreatureArena& arena, const Transform& transform) {
		    if (arena.fightOn && (arena.first == own || arena.second == own))
		    {
			    arenas.push_back(transform.position);
		    }
	    });
	return fight::FightMusicHeard(DirectsFight(), arenas, camera);
}

void CreatureFightSystem::LoseFightHealth(entt::entity creature, float damage)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!IsDuelling(creature) || IsKnockedOut(creature))
	{
		return;
	}
	auto& fighter = registry.Get<CreatureFighting>(creature).fighter;
	const auto before = fighter.health;
	fighter.health = fight::HealthAfterLifeLost(before, damage);
	fight_transcript::Note(creature, "level", [&] { return LevelChange(creature, "health", before, fighter.health, "life"); });
	if (fighter.health <= 0.0f)
	{
		KnockOut(creature);
	}
}
