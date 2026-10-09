/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "LeashSystem.h"

#include <cmath>

#include <algorithm>
#include <limits>
#include <numbers>
#include <utility>
#include <vector>

#include <LNDFile.h>
#include <glm/geometric.hpp>
#include <glm/gtx/transform.hpp>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/LandLightFrame.h"
#include "3D/LandLightTable.h"
#include "Audio/AudioManagerInterface.h"
#include "Audio/Sound.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureAudio.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureRoute.h"
#include "Creature/LeashKeys.h"
#include "Creature/LeashOrders.h"
#include "Creature/LeashOwnership.h"
#include "Creature/LeashRules.h"
#include "Creature/TempleLeashes.h"
#include "ECS/Archetypes/LeashMarkerArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/SkinOverride.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureAudioSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Graphics/Texture2D.h"
#include "Input/GameActionMapInterface.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using openblack::ecs::Registry;
namespace leash = openblack::creature_leash;
namespace orders = openblack::creature_leash_orders;
using creature_desires::Desire;

namespace
{
constexpr float k_TurnsPerSecond = 1.0f / std::chrono::duration<float>(TimeSystemInterface::k_TurnDuration).count();
/// Where the leash meets a creature with no collar bone, as a share of its height
constexpr float k_CollarHeightShare = 0.7f;
/// Where the leash meets something it is tied to, as a share of its height, and that height when it has no mesh
constexpr float k_TiedHeightShare = 0.5f;
constexpr float k_DefaultObjectHeight = 5.0f;
/// How near the hand the creature walks
constexpr float k_HandArrival = leash::k_CloseToHand * 0.5f;
/// How far round a creature is tapped, as a share of its height
constexpr float k_CreatureTapShare = 0.4f;
/// How far a tap reaches
constexpr float k_TapReach = 1e6f;

std::optional<glm::vec3> HandPoint()
{
	if (!Locator::handSystem::has_value())
	{
		return std::nullopt;
	}
	return Locator::handSystem::value().GetPlayerHandPositions()[static_cast<size_t>(HandSystemInterface::Side::Left)];
}

float CreatureHeight(const Creature& creature)
{
	return creature_morph::k_HeightAtSizeOne * ShownSize(creature);
}

float GroundAt(glm::vec2 point)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(point) : 0.0f;
}

/// Where the leash meets the creature: its collar bone as posed this frame, or high on its body when it has none
glm::vec3 CollarPoint(const Registry& registry, entt::entity entity)
{
	const auto& transform = registry.Get<const Transform>(entity);
	const auto& creature = registry.Get<const Creature>(entity);
	const auto fallback = transform.position + glm::vec3(0.0f, CreatureHeight(creature) * k_CollarHeightShare, 0.0f);
	const auto* animation = registry.TryGet<const CreatureAnimation>(entity);
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto rigId = creature::GetRigId(creature.species);
	if (animation == nullptr || animation->boneMatrices.empty() || !rigs.Contains(rigId))
	{
		return fallback;
	}
	const auto bone = rigs.Handle(rigId)->leashBone;
	if (!bone.has_value() || *bone >= animation->boneMatrices.size())
	{
		return fallback;
	}
	const auto placement = creature::PlacementMatrix(transform.position, transform.rotation, transform.scale);
	return glm::vec3(creature::PosedBone(*bone, animation->boneMatrices, placement)[3]);
}

float ObjectHeight(const Registry& registry, entt::entity entity)
{
	const auto& transform = registry.Get<const Transform>(entity);
	if (const auto* creature = registry.TryGet<const Creature>(entity))
	{
		return CreatureHeight(*creature);
	}
	const auto* mesh = registry.TryGet<const Mesh>(entity);
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (mesh == nullptr || !meshes.Contains(mesh->id))
	{
		return k_DefaultObjectHeight;
	}
	const auto box = meshes.Handle(mesh->id)->GetBoundingBox();
	return std::max((box.maxima.y - box.minima.y) * transform.scale.y, 0.0f);
}

/// Where the leash meets something it is tied to: a creature's collar, else halfway up it
glm::vec3 TiedPoint(const Registry& registry, entt::entity entity)
{
	if (registry.TryGet<const Creature>(entity) != nullptr)
	{
		return CollarPoint(registry, entity);
	}
	return registry.Get<const Transform>(entity).position +
	       glm::vec3(0.0f, ObjectHeight(registry, entity) * k_TiedHeightShare, 0.0f);
}

bool IsMobile(const Registry& registry, entt::entity entity)
{
	return registry.TryGet<const Creature>(entity) != nullptr || registry.TryGet<const Villager>(entity) != nullptr ||
	       registry.TryGet<const Mobile>(entity) != nullptr || registry.TryGet<const MobileObject>(entity) != nullptr;
}

/// The leash's lengths: by the creature's size in the hand, by what it is tied to otherwise
leash::Lengths LengthsOf(const Registry& registry, entt::entity creature, const CreatureLeash::Worn& worn)
{
	const auto& body = registry.Get<const Creature>(creature);
	if (!worn.tiedTo.has_value())
	{
		return leash::InHand(ShownSize(body));
	}
	if (IsMobile(registry, *worn.tiedTo))
	{
		return leash::TiedToMobile(CreatureHeight(body));
	}
	const auto& from = registry.Get<const Transform>(creature).position;
	const auto& to = registry.Get<const Transform>(*worn.tiedTo).position;
	return leash::TiedToStatic(glm::distance(glm::vec2(from.x, from.z), glm::vec2(to.x, to.z)));
}

/// How far along a ray it meets a ball, if it does
std::optional<float> RayBall(const glm::vec3& origin, const glm::vec3& direction, const glm::vec3& centre, float radius)
{
	const auto toCentre = centre - origin;
	const auto along = glm::dot(toCentre, direction);
	if (along < 0.0f)
	{
		return std::nullopt;
	}
	const auto apart = glm::dot(toCentre, toCentre) - (along * along);
	if (apart > radius * radius)
	{
		return std::nullopt;
	}
	return along;
}

void PlaySound(audio::SoundId sound, std::optional<glm::vec3> position)
{
	if (Locator::audio::has_value())
	{
		Locator::audio::value().PlaySoundEffect(static_cast<entt::id_type>(sound), position);
	}
}

CreatureMindState* MindOf(Registry& registry, entt::entity creature)
{
	return registry.TryGet<CreatureMindState>(creature);
}

/// What the leash shortcuts need to know about a creature
leash::KeyState KeyStateOf(const Registry& registry, entt::entity creature)
{
	const auto* leashes = registry.TryGet<const CreatureLeash>(creature);
	if (leashes == nullptr)
	{
		return {};
	}
	return {
	    .worn = leashes->worn.has_value(),
	    .tied = leashes->worn.has_value() && leashes->worn->tiedTo.has_value(),
	    .known = leashes->known,
	    .selected = leashes->worn.has_value() ? leashes->worn->type : leashes->selected,
	};
}

int PlayerNumber(PlayerNames player)
{
	return static_cast<int>(player) + 1;
}

/// The smoke sheet the temple's leashes glow with, eight pictures a row, and its alpha
constexpr auto k_SmokeId = entt::hashed_string("raw/smoke");
constexpr auto k_SmokeAlphaId = entt::hashed_string("raw/smokea");
constexpr uint32_t k_SmokeCellsPerRow = 8;
constexpr float k_SmokeCellsPerSide = 8.0f;

/// The player at this computer, whose own creature's marker is drawn and who hears their orders acknowledged
constexpr PlayerNames k_LocalPlayer = PlayerNames::PLAYER_ONE;

/// Whether a point is in the sea: a cell of the land with water in it, or off the land
bool IsWater(glm::vec2 point)
{
	if (!Locator::terrainSystem::has_value() || point.x < 0.0f || point.y < 0.0f)
	{
		return true;
	}
	const auto cell = glm::u16vec2(glm::floor(point / LandIslandInterface::k_CellSize));
	const auto* landCell = Locator::terrainSystem::value().FindCell(cell);
	return landCell == nullptr || landCell->properties.hasWater != 0;
}

/// The field lying at a point of the land, if any: one whose model covers it
std::optional<entt::entity> FieldAt(const Registry& registry, glm::vec2 point)
{
	const auto& meshes = Locator::resources::value().GetMeshes();
	std::optional<entt::entity> found;
	registry.Each<const Field, const Transform, const Mesh>(
	    [&](entt::entity entity, const Field& /*field*/, const Transform& transform, const Mesh& mesh) {
		    if (found.has_value() || !meshes.Contains(mesh.id))
		    {
			    return;
		    }
		    const auto box = meshes.Handle(mesh.id)->GetBoundingBox();
		    const auto model = glm::translate(transform.position) * glm::mat4(transform.rotation) * glm::scale(transform.scale);
		    const auto local = glm::vec3(glm::inverse(model) * glm::vec4(point.x, transform.position.y, point.y, 1.0f));
		    if (local.x >= box.minima.x && local.x <= box.maxima.x && local.z >= box.minima.z && local.z <= box.maxima.z)
		    {
			    found = entity;
		    }
	    });
	return found;
}

/// Where the creature's home is, when its player has a citadel: where it was given a home, else by the citadel
std::optional<glm::vec3> CitadelHome(const Registry& registry, entt::entity creature)
{
	const auto owner = registry.Get<const Creature>(creature).owner;
	std::optional<glm::vec3> citadel;
	registry.Each<const Temple, const Transform>([&citadel, owner](const Temple& temple, const Transform& transform) {
		if (temple.owner == owner && !citadel.has_value())
		{
			citadel = transform.position;
		}
	});
	if (!citadel.has_value())
	{
		return std::nullopt;
	}
	const auto* leashes = registry.TryGet<const CreatureLeash>(creature);
	return leashes != nullptr && leashes->home.has_value() ? leashes->home : citadel;
}

/// What the creature's body allows of an order
orders::Body BodyOf(const Registry& registry, entt::entity creature)
{
	const auto* leashes = registry.TryGet<const CreatureLeash>(creature);
	const auto* needs = registry.TryGet<const CreatureNeeds>(creature);
	return {
	    .leashWorks = leashes != nullptr && leashes->worn.has_value() && leashes->worn->works,
	    .life = needs != nullptr ? needs->needs.life : 1.0f,
	    .exhaustion = needs != nullptr ? needs->needs.exhaustion : 0.0f,
	};
}

std::optional<entt::entity> HeldBy(entt::entity creature)
{
	return Locator::creatureObjectActionSystem::has_value() ? Locator::creatureObjectActionSystem::value().GetHeld(creature)
	                                                        : std::nullopt;
}

using ForcedPlan = CreatureMindSystemInterface::ForcedPlan;
bool Force(entt::entity creature, const ForcedPlan& plan)
{
	return Locator::creatureMindSystem::has_value() && Locator::creatureMindSystem::value().ForcePlan(creature, plan);
}
} // namespace

bool LeashSystem::Knows(entt::entity creature, LeashType type) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* leashes = registry.TryGet<const CreatureLeash>(creature);
	const auto index = leash::IndexOf(type);
	return leashes != nullptr && index.has_value() && leashes->known.test(*index);
}

void LeashSystem::SetKnown(entt::entity creature, LeashType type, bool known)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto index = leash::IndexOf(type);
	if (!index.has_value() || registry.TryGet<Creature>(creature) == nullptr)
	{
		return;
	}
	auto& leashes = registry.TryGet<CreatureLeash>(creature) != nullptr ? registry.Get<CreatureLeash>(creature)
	                                                                    : registry.Assign<CreatureLeash>(creature);
	leashes.known.set(*index, known);
	// Forgetting the learning leash takes any leash off, as no leash can be worn without it
	if (!known && leashes.worn.has_value() && (leashes.worn->type == type || type == LeashType::Rope))
	{
		TakeOff(creature);
	}
}

std::vector<leash::Claim> LeashSystem::Claims() const
{
	const auto& registry = Locator::entitiesRegistry::value();
	std::vector<leash::Claim> claims;
	registry.Each<const Creature>([&claims](entt::entity entity, const Creature& creature) {
		claims.push_back({.creature = entt::to_integral(entity), .owner = creature.owner, .leashable = creature.leashable});
	});
	return claims;
}

bool LeashSystem::IsLeashable(entt::entity creature) const
{
	const auto* body = Locator::entitiesRegistry::value().TryGet<const Creature>(creature);
	return body != nullptr && body->leashable;
}

bool LeashSystem::SetLeashable(entt::entity creature, bool leashable)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* body = registry.TryGet<Creature>(creature);
	if (body == nullptr)
	{
		return false;
	}
	const auto owner = body->owner;
	if (!leashable)
	{
		if (body->leashable)
		{
			TakeOff(creature);
			registry.Get<Creature>(creature).leashable = false;
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {} is no longer the one player {} leads",
			                   entt::to_integral(creature), PlayerNumber(owner));
		}
		return true;
	}
	if (!leash::CanLead(owner))
	{
		Refuse(owner, creature, leash::Refusal::NoPlayer);
		return false;
	}
	// A player leads one creature: the one chosen last
	for (const auto id : leash::Displaced(Claims(), entt::to_integral(creature), owner))
	{
		const auto other = static_cast<entt::entity>(id);
		TakeOff(other);
		registry.Get<Creature>(other).leashable = false;
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {} is no longer the one player {} leads", id, PlayerNumber(owner));
	}
	if (!registry.Get<Creature>(creature).leashable)
	{
		registry.Get<Creature>(creature).leashable = true;
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {} is now the one player {} leads", entt::to_integral(creature),
		                   PlayerNumber(owner));
	}
	return true;
}

void LeashSystem::SetOwner(entt::entity creature, PlayerNames owner)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* body = registry.TryGet<Creature>(creature);
	if (body == nullptr || body->owner == owner)
	{
		return;
	}
	TakeOff(creature);
	auto& changed = registry.Get<Creature>(creature);
	changed.owner = owner;
	// It stays the one its new owner leads only if they have no other
	if (changed.leashable &&
	    (!leash::CanLead(owner) || !leash::Displaced(Claims(), entt::to_integral(creature), owner).empty()))
	{
		registry.Get<Creature>(creature).leashable = false;
	}
}

void LeashSystem::ClaimOnArrival(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.TryGet<const Creature>(creature);
	if (body == nullptr || body->leashable)
	{
		return;
	}
	auto others = Claims();
	std::erase_if(others, [creature](const leash::Claim& claim) { return claim.creature == entt::to_integral(creature); });
	if (leash::ClaimsOnArrival(others, body->owner))
	{
		registry.Get<Creature>(creature).leashable = true;
	}
}

leash::Refusal LeashSystem::WhyNot(PlayerNames player, entt::entity creature, LeashType type) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.Valid(creature) ? registry.TryGet<const Creature>(creature) : nullptr;
	if (body == nullptr)
	{
		return leash::Refusal::NotACreature;
	}
	const auto* leashes = registry.TryGet<const CreatureLeash>(creature);
	return leash::WhyNot(
	    player,
	    {
	        .owner = body->owner,
	        .leashable = body->leashable,
	        .knowsLearningLeash = Knows(creature, LeashType::Rope),
	        .knowsType = Knows(creature, type),
	        .heldBy = leashes != nullptr && leashes->worn.has_value() ? std::optional(leashes->worn->holder) : std::nullopt,
	    });
}

std::optional<LeashSystemInterface::Refused> LeashSystem::LastRefusal() const
{
	return _lastRefusal;
}

void LeashSystem::Refuse(PlayerNames player, entt::entity creature, leash::Refusal why)
{
	_lastRefusal = Refused {.player = player, .creature = creature, .why = why};
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Player {} can't leash creature {}: {}", PlayerNumber(player),
	                   entt::to_integral(creature), leash::Describe(why));
}

bool LeashSystem::PutOn(entt::entity creature, LeashType type)
{
	const auto* body = Locator::entitiesRegistry::value().TryGet<const Creature>(creature);
	if (body == nullptr)
	{
		Refuse(PlayerNames::NEUTRAL, creature, leash::Refusal::NotACreature);
		return false;
	}
	// Whoever puts it on, it is held by the creature's owner
	return PutOnFor(body->owner, creature, type);
}

bool LeashSystem::PutOnFor(PlayerNames player, entt::entity creature, LeashType type)
{
	if (const auto why = WhyNot(player, creature, type); why != leash::Refusal::None)
	{
		Refuse(player, creature, why);
		return false;
	}
	auto& registry = Locator::entitiesRegistry::value();
	auto* leashes = &registry.Get<CreatureLeash>(creature);
	if (!leashes->worn.has_value())
	{
		// Assigned rather than emplaced: clang can't yet see that the nested type is default constructible
		leashes->worn = CreatureLeash::Worn {};
		leashes->worn->holder = player;
	}
	leashes->worn->type = type;
	leashes->selected = type;
	leashes->control = CreatureLeash::Control::Idle;
	// The leash going on ends any drag the body still leans against
	if (auto* animation = registry.TryGet<CreatureAnimation>(creature))
	{
		creature_sway::SetLeashDrag(animation->sway, 0.0f);
	}
	// The rope is laid afresh from the hand to the collar next frame
	leashes->worn->ropeStarted = false;
	return true;
}

void LeashSystem::TakeOff(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* leashes = registry.TryGet<CreatureLeash>(creature);
	if (leashes == nullptr || !leashes->worn.has_value())
	{
		return;
	}
	EndOrder(creature);
	leashes = registry.TryGet<CreatureLeash>(creature);
	leashes->worn.reset();
	leashes->control = CreatureLeash::Control::Idle;
	leashes->pull = 0.0f;
	leashes->confinementRadius = 0.0f;
	leashes->turnsWithOther = 0;
	if (auto* mind = MindOf(registry, creature))
	{
		mind->leash.obeying = false;
		mind->leash.forcedDesire.reset();
		mind->leash.forcedValue = 0.0f;
		mind->leash.learningInHand = false;
		mind->leash.miracleSightingWeight = leash::MiracleSightingWeight(false);
	}
}

bool LeashSystem::Toggle(entt::entity creature)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.TryGet<const Creature>(creature);
	if (body == nullptr)
	{
		Refuse(PlayerNames::NEUTRAL, creature, leash::Refusal::NotACreature);
		return false;
	}
	// As the leash key does, for the creature's owner
	return Carry(body->owner, creature, leash::CommandFor(leash::LeashKey::Leash, KeyStateOf(registry, creature)));
}

bool LeashSystem::Carry(PlayerNames player, entt::entity creature, const leash::KeyCommand& command)
{
	using Kind = leash::KeyCommand::Kind;
	const auto checked = command.kind == Kind::PutOn || command.kind == Kind::ChangeType ? command.type : LeashType::Rope;
	if (const auto why = WhyNot(player, creature, checked); why != leash::Refusal::None)
	{
		Refuse(player, creature, why);
		return false;
	}
	switch (command.kind)
	{
	case Kind::None:
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {} knows no other leash to pick", entt::to_integral(creature));
		return false;
	case Kind::PutOn:
		return PutOnFor(player, creature, command.type);
	case Kind::TakeOff:
		TakeOff(creature);
		return true;
	case Kind::UntieToHand:
		UntieToHand(creature);
		return true;
	case Kind::ChangeType:
		return ChangeType(creature, command.type);
	}
	return false;
}

bool LeashSystem::PressKey(PlayerNames player, leash::LeashKey key)
{
	const auto creature = PlayersCreature(player);
	if (!creature.has_value())
	{
		_lastRefusal = Refused {.player = player, .creature = entt::null, .why = leash::Refusal::NotACreature};
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Player {} has no creature they can lead", PlayerNumber(player));
		return false;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	return Carry(player, *creature, leash::CommandFor(key, KeyStateOf(registry, *creature)));
}

bool LeashSystem::Shake(PlayerNames player)
{
	const auto creature = PlayersCreature(player);
	if (!creature.has_value() || !IsLeashed(*creature) || TiedTo(*creature).has_value())
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Player {} shook the hand with no leash held in it", PlayerNumber(player));
		return false;
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Player {} shook the leash off creature {}", PlayerNumber(player),
	                   entt::to_integral(*creature));
	TakeOff(*creature);
	return true;
}

bool LeashSystem::TapCreature(PlayerNames player, entt::entity creature)
{
	if (IsLeashed(creature))
	{
		// Already on: tapping its own creature again does nothing, and another's is refused
		if (const auto why = WhyNot(player, creature, TypeOf(creature)); why != leash::Refusal::None)
		{
			Refuse(player, creature, why);
		}
		return false;
	}
	const auto* leashes = Locator::entitiesRegistry::value().TryGet<const CreatureLeash>(creature);
	const auto picked = leashes != nullptr ? leashes->selected : LeashType::Rope;
	return PutOnFor(player, creature, Knows(creature, picked) ? picked : LeashType::Rope);
}

bool LeashSystem::ChangeType(entt::entity creature, LeashType type)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* leashes = registry.TryGet<CreatureLeash>(creature);
	if (leashes == nullptr || !Knows(creature, type))
	{
		return false;
	}
	leashes->selected = type;
	if (leashes->worn.has_value())
	{
		leashes->worn->type = type;
		leashes->worn->rope.look = leash::LookFor(type);
	}
	// The posts show the leash picked
	registry.Each<LeashPost>([leashes](LeashPost& post) { post.selected = post.type == leashes->selected; });
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {}'s picked leash is now the {} leash", entt::to_integral(creature),
	                   leash::Name(type));
	return true;
}

bool LeashSystem::TieTo(entt::entity creature, entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (object == creature || !registry.Valid(object) || registry.TryGet<const Transform>(object) == nullptr)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {}'s leash can't be tied to that", entt::to_integral(creature));
		return false;
	}
	if (!IsLeashed(creature))
	{
		const auto* picked = registry.TryGet<const CreatureLeash>(creature);
		if (!PutOn(creature, picked != nullptr ? picked->selected : LeashType::Rope))
		{
			return false;
		}
	}
	auto* leashes = &registry.Get<CreatureLeash>(creature);
	auto& worn = *leashes->worn;
	worn.tiedTo = object;
	worn.tiedTurn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
	const auto lengths = LengthsOf(registry, creature, worn);
	worn.rope.slackLength = lengths.slack;
	worn.rope.maxLength = lengths.max;
	worn.ropeStarted = false;
	leashes->control = CreatureLeash::Control::Idle;
	leashes->turnsWithOther = 0;

	// The two tying sounds in turn
	PlaySound(_secondAttachSound ? audio::SoundId::G_LeashAttach_01_2 : audio::SoundId::G_LeashAttach_01_1, std::nullopt);
	_secondAttachSound = !_secondAttachSound;

	// The creature learns that the player wants something done with what it is tied to
	if (auto* mind = MindOf(registry, creature))
	{
		mind->leash.obeying = false;
		const bool isCreature = registry.TryGet<const Creature>(object) != nullptr;
		mind->leash.shown.push_back({
		    .object = static_cast<uint32_t>(object),
		    .type = worn.type,
		    .lessons = leash::LessonsFor(worn.type, isCreature),
		});
		mind->leash.actOn.push_back(static_cast<uint32_t>(object));
	}
	return true;
}

void LeashSystem::UntieToHand(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* leashes = registry.TryGet<CreatureLeash>(creature);
	if (leashes == nullptr || !leashes->worn.has_value() || !leashes->worn->tiedTo.has_value())
	{
		return;
	}
	leashes->worn->tiedTo.reset();
	leashes->worn->ropeStarted = false;
	leashes->turnsWithOther = 0;
}

void LeashSystem::SetWorks(entt::entity creature, bool works)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* leashes = registry.TryGet<CreatureLeash>(creature); leashes != nullptr && leashes->worn.has_value())
	{
		leashes->worn->works = works;
	}
}

void LeashSystem::SetDrawn(bool drawn)
{
	Locator::entitiesRegistry::value().Each<CreatureLeash>([drawn](CreatureLeash& leashes) { leashes.drawn = drawn; });
}

void LeashSystem::ConfineToHome(entt::entity creature, float radius)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.TryGet<Creature>(creature) == nullptr)
	{
		return;
	}
	auto& leashes = registry.TryGet<CreatureLeash>(creature) != nullptr ? registry.Get<CreatureLeash>(creature)
	                                                                    : registry.Assign<CreatureLeash>(creature);
	if (!leashes.home.has_value())
	{
		// Its home is by its player's temple, or where it stands when there is none
		const auto owner = registry.Get<const Creature>(creature).owner;
		registry.Each<const Temple, const Transform>([&leashes, owner](const Temple& temple, const Transform& transform) {
			if (temple.owner == owner && !leashes.home.has_value())
			{
				leashes.home = transform.position;
			}
		});
		if (!leashes.home.has_value())
		{
			leashes.home = registry.Get<const Transform>(creature).position;
		}
	}
	leashes.confinementCentre = *leashes.home;
	leashes.confinementRadius = radius;
}

void LeashSystem::ClearConfinement(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* leashes = registry.TryGet<CreatureLeash>(creature))
	{
		leashes->confinementRadius = 0.0f;
		leashes->returning = false;
	}
}

bool LeashSystem::FreeOfHome(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (body == nullptr || transform == nullptr)
	{
		return false;
	}
	std::optional<glm::vec3> temple;
	registry.Each<const Temple, const Transform>([&temple, body](const Temple& t, const Transform& at) {
		if (t.owner == body->owner && !temple.has_value())
		{
			temple = at.position;
		}
	});
	const auto* leashes = registry.TryGet<const CreatureLeash>(creature);
	const auto home = leashes != nullptr && leashes->home.has_value() ? leashes->home : temple;
	if (!home.has_value())
	{
		return false;
	}
	return leash::FreeOfHome(glm::distance(transform->position, *home), temple.has_value());
}

bool LeashSystem::IsLeashed(entt::entity creature) const
{
	const auto* leashes = Locator::entitiesRegistry::value().TryGet<const CreatureLeash>(creature);
	return leashes != nullptr && leashes->worn.has_value();
}

std::optional<entt::entity> LeashSystem::TiedTo(entt::entity creature) const
{
	const auto* leashes = Locator::entitiesRegistry::value().TryGet<const CreatureLeash>(creature);
	return leashes != nullptr && leashes->worn.has_value() ? leashes->worn->tiedTo : std::nullopt;
}

std::optional<glm::vec3> LeashSystem::HolderPoint(entt::entity creature) const
{
	const auto* leashes = Locator::entitiesRegistry::value().TryGet<const CreatureLeash>(creature);
	if (leashes == nullptr || !leashes->worn.has_value() || leashes->worn->tiedTo.has_value())
	{
		return std::nullopt;
	}
	return HandPoint();
}

LeashType LeashSystem::TypeOf(entt::entity creature) const
{
	const auto* leashes = Locator::entitiesRegistry::value().TryGet<const CreatureLeash>(creature);
	return leashes != nullptr && leashes->worn.has_value() ? leashes->worn->type : LeashType::None;
}

std::optional<entt::entity> LeashSystem::PlayersCreature(PlayerNames player) const
{
	if (const auto id = leash::LeashableOf(Claims(), player))
	{
		return static_cast<entt::entity>(*id);
	}
	return std::nullopt;
}

void LeashSystem::PlacePosts(PlayerNames owner, const std::array<glm::vec3, 3>& points)
{
	auto& registry = Locator::entitiesRegistry::value();
	// A player has one set of posts
	std::vector<entt::entity> old;
	registry.Each<const LeashPost>([&old, owner](entt::entity entity, const LeashPost& post) {
		if (post.owner == owner)
		{
			old.push_back(entity);
			if (post.glow != entt::null)
			{
				old.push_back(post.glow);
			}
		}
	});
	for (const auto entity : old)
	{
		if (registry.Valid(entity))
		{
			registry.Destroy(entity);
		}
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto& textures = Locator::resources::value().GetTextures();
	const bool hasSkin = textures.Contains(LeashPost::k_TextureId);
	const bool hasSmoke = textures.Contains(k_SmokeId.value()) && textures.Contains(k_SmokeAlphaId.value());
	auto* random = Locator::gameRandom::has_value() ? &Locator::gameRandom::value() : nullptr;
	const auto draw = [random] { return random != nullptr ? random->CrtRand() : 0; };
	for (size_t i = 0; i < points.size(); ++i)
	{
		const auto entity = registry.Create();
		// Hidden until it is known to hang there
		registry.Assign<Transform>(entity, points.at(i), glm::mat3(1.0f), glm::vec3(0.0f));
		// Each starts at a random scroll, tumble and glow, drawn in that order
		const std::array<int32_t, 4> draws {draw(), draw(), draw(), draw()};
		LeashPost post {.type = leash::k_Types.at(i),
		                .owner = owner,
		                .selected = false,
		                .point = points.at(i),
		                .look = temple_leashes::Start(draws)};
		if (hasSmoke)
		{
			post.glow = registry.Create();
			registry.Assign<Transform>(post.glow, points.at(i), glm::mat3(1.0f), glm::vec3(0.0f));
			registry.Assign<Sprite>(post.glow, Sprite {.texture = textures.Handle(k_SmokeId)->GetNativeHandle(),
			                                           .uvMin = glm::vec2(0.0f),
			                                           .uvExtent = glm::vec2(1.0f / k_SmokeCellsPerSide),
			                                           .tint = glm::vec4(1.0f),
			                                           .additive = false,
			                                           .facesCamera = true,
			                                           .alpha = textures.Handle(k_SmokeAlphaId)->GetNativeHandle()});
		}
		registry.Assign<LeashPost>(entity, post);
		const auto collar = entt::hashed_string::value(temple_leashes::CollarMeshName(owner, post.type).c_str());
		const auto meshId = meshes.Contains(collar) ? collar : LeashPost::k_MeshId;
		if (meshes.Contains(meshId))
		{
			registry.Assign<Mesh>(entity, meshId, static_cast<int8_t>(0), static_cast<int8_t>(0));
			if (hasSkin && meshId != LeashPost::k_MeshId)
			{
				registry.Assign<SkinOverride>(
				    entity, SkinOverride {.texture = textures.Handle(LeashPost::k_TextureId)->GetNativeHandle(),
				                          .uvOffset = glm::vec2(post.look.scroll, temple_leashes::Band(post.type))});
			}
		}
	}
	registry.SetDirty();
	_postsPlaced = true;
}

void LeashSystem::UpdatePosts(float seconds)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto light = temple_leashes::GlowAlpha(FrameLandLight(LandLightTable::k_Size - 1));
	// The hand carrying the picked leash
	std::optional<Transform> hand;
	if (Locator::handSystem::has_value())
	{
		const auto entity = Locator::handSystem::value().GetPlayerHands()[static_cast<size_t>(HandSystemInterface::Side::Left)];
		if (const auto* at = registry.Valid(entity) ? registry.TryGet<const Transform>(entity) : nullptr)
		{
			hand = *at;
		}
	}
	std::vector<entt::entity> posts;
	registry.Each<const LeashPost>([&posts](entt::entity entity, const LeashPost& /*post*/) { posts.push_back(entity); });
	for (const auto entity : posts)
	{
		auto& post = registry.Get<LeashPost>(entity);
		const auto creature = PlayersCreature(post.owner);
		post.hung = temple_leashes::Hung(creature.has_value(), creature.has_value() && Knows(*creature, post.type));
		auto& transform = registry.Get<Transform>(entity);
		auto* glow = post.glow != entt::null && registry.Valid(post.glow) ? registry.TryGet<Transform>(post.glow) : nullptr;
		if (!post.hung)
		{
			transform.scale = glm::vec3(0.0f);
			if (glow != nullptr)
			{
				glow->scale = glm::vec3(0.0f);
			}
			continue;
		}
		post.look = temple_leashes::Advance(post.look, seconds);
		if (auto* skin = registry.TryGet<SkinOverride>(entity))
		{
			skin->uvOffset = glm::vec2(post.look.scroll, temple_leashes::Band(post.type));
		}
		// The picked leash of this machine's player is carried in the hand; the others tumble where they hang
		const bool picked = post.selected && post.owner == k_LocalPlayer;
		if (picked && hand.has_value())
		{
			const auto scale = hand->scale.x * temple_leashes::k_HandScale;
			transform.position = hand->position + (hand->rotation * (temple_leashes::k_InHandOffset * scale));
			// Carried, it is turned as the hand is, without its tumble
			transform.rotation = hand->rotation;
			transform.scale = glm::vec3(scale * temple_leashes::k_InHandShare);
		}
		else
		{
			transform.position = post.point;
			transform.rotation = temple_leashes::Turn(post.look);
			transform.scale = glm::vec3(1.0f);
		}
		if (glow != nullptr)
		{
			glow->scale = glm::vec3(temple_leashes::k_GlowSize);
			auto& sprite = registry.Get<Sprite>(post.glow);
			const auto look = temple_leashes::GlowOf(picked, light);
			sprite.tint = look.tint;
			sprite.additive = look.additive;
			const auto picture = temple_leashes::GlowPicture(post.look);
			sprite.uvMin =
			    glm::vec2(static_cast<float>(picture % k_SmokeCellsPerRow), static_cast<float>(picture / k_SmokeCellsPerRow)) /
			    k_SmokeCellsPerSide;
		}
	}
}

bool LeashSystem::TapPost(entt::entity post)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* tapped = registry.TryGet<LeashPost>(post);
	if (tapped == nullptr)
	{
		return false;
	}
	const auto creature = PlayersCreature(tapped->owner);
	// Tapping the picked leash again puts it back
	if (tapped->selected)
	{
		tapped->selected = false;
		return true;
	}
	PlaySound(audio::SoundId::G_ClickOnSpell_01, std::nullopt);
	registry.Each<LeashPost>([tapped](LeashPost& other) {
		if (other.owner == tapped->owner)
		{
			other.selected = &other == tapped;
		}
	});
	if (creature.has_value())
	{
		ChangeType(*creature, tapped->type);
	}
	return true;
}

void LeashSystem::PlacePostsAtTemples()
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& meshes = Locator::resources::value().GetMeshes();
	std::vector<std::pair<PlayerNames, std::array<glm::vec3, 3>>> found;
	bool anyTemple = false;
	registry.Each<const Temple, const Transform, const Mesh>([&](const Temple& temple, const Transform& transform,
	                                                             const Mesh& mesh) {
		anyTemple = true;
		if (!meshes.Contains(mesh.id))
		{
			return;
		}
		// The leashes hang at the first three points of the temple's heart
		const auto& points = meshes.Handle(mesh.id)->GetExtraMetrics();
		if (points.size() < 3)
		{
			SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "The temple has {} points, too few to hang its leashes at", points.size());
			return;
		}
		const auto model = glm::translate(transform.position) * glm::mat4(transform.rotation) * glm::scale(transform.scale);
		std::array<glm::vec3, 3> at {};
		for (size_t i = 0; i < at.size(); ++i)
		{
			at.at(i) = glm::vec3(model * points.at(i)[3]);
		}
		found.emplace_back(temple.owner, at);
	});
	if (!anyTemple)
	{
		return;
	}
	for (const auto& [owner, points] : found)
	{
		PlacePosts(owner, points);
	}
	_postsPlaced = true;
}

void LeashSystem::Pull(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& leashes = registry.Get<CreatureLeash>(creature);
	auto* mind = MindOf(registry, creature);
	const auto hand = HandPoint();
	if (!hand.has_value() || !Locator::creatureLocomotionSystem::has_value())
	{
		return;
	}
	// Asleep or out cold, a pull doesn't wake it
	if (mind != nullptr && (creature_mind::IsAsleep(mind->idle) || creature_mind::IsUnconscious(mind->idle)))
	{
		return;
	}
	auto& locomotion = Locator::creatureLocomotionSystem::value();
	const auto* walk = registry.TryGet<const CreatureLocomotion>(creature);
	const auto position = registry.Get<const Transform>(creature).position;
	const bool walking = leashes.control == CreatureLeash::Control::WalkingToHand && locomotion.IsMoving(creature);
	const auto decision = leash::DecideLead(position, *hand, walk != nullptr ? walk->destination : std::nullopt, walking);
	if (decision != leash::Lead::GoToHand)
	{
		return;
	}
	// The first pull stops whatever it was doing; pulled away from the same desire twice, it is held back a while
	if (leashes.control == CreatureLeash::Control::Idle && mind != nullptr)
	{
		// The desire behind a plan it carries out, else behind what it does with nothing better to do
		const auto desire = mind->planActive && mind->planner.current.has_value()
		                        ? std::optional(mind->planner.current->desire)
		                        : leash::DesireBehind(mind->idle.activity, mind->idle.shown);
		// Pulled away from a plan, it says so
		if (mind->planActive)
		{
			leashes.help = CreatureLeash::Help::PulledAway;
			leashes.helpDesire = desire;
		}
		mind->planActive = false;
		mind->planner.current.reset();
		if (desire.has_value() && mind->desires.has_value())
		{
			if (const auto seconds = leash::RecordPull(leashes.pulls, *desire))
			{
				creature_desires::Suppress(*mind->desires, *desire, *seconds, k_TurnsPerSecond);
			}
		}
		creature_mind::Plan(mind->idle, creature_mind::Activity::None, {});
	}
	if (locomotion.LeadTo(creature, glm::vec2(hand->x, hand->z), leashes.pull, k_HandArrival) ==
	    CreatureLocomotionSystemInterface::MoveResult::Started)
	{
		leashes.control = CreatureLeash::Control::WalkingToHand;
		// Once on its way, it is pulled along as fast as it goes
		leashes.pull = 1.0f;
		// and its body leans against the drag
		if (auto* animation = registry.TryGet<CreatureAnimation>(creature))
		{
			creature_sway::SetLeashDrag(animation->sway, 1.0f);
		}
		if (mind != nullptr)
		{
			mind->leash.obeying = true;
		}
	}
}

void LeashSystem::ProcessTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!_postsPlaced)
	{
		PlacePostsAtTemples();
	}
	std::vector<entt::entity> leashed;
	registry.Each<CreatureLeash>([&leashed](entt::entity entity, CreatureLeash& /*leashes*/) { leashed.push_back(entity); });
	// An order's marker goes once the creature finishes or gives up what it was told, or the thing is gone
	for (const auto entity : leashed)
	{
		if (registry.Get<const CreatureLeash>(entity).order.has_value() && !OrderInForce(entity))
		{
			EndOrder(entity);
		}
	}
	for (const auto entity : leashed)
	{
		auto& leashes = registry.Get<CreatureLeash>(entity);
		auto* mind = MindOf(registry, entity);
		const auto* body = registry.TryGet<const Creature>(entity);
		const auto* transform = registry.TryGet<const Transform>(entity);
		// Fighting or knocked out, the leash doesn't pull it about
		if (body == nullptr || transform == nullptr || registry.AnyOf<CreatureFighting, CreatureKnockedOut>(entity))
		{
			continue;
		}
		const auto position = glm::vec2(transform->position.x, transform->position.z);
		const bool moving =
		    Locator::creatureLocomotionSystem::has_value() && Locator::creatureLocomotionSystem::value().IsMoving(entity);

		// Arrived at the hand, or stopped on the way, its mind takes over again
		if (leashes.control == CreatureLeash::Control::WalkingToHand && !moving)
		{
			leashes.control = CreatureLeash::Control::Idle;
			if (mind != nullptr)
			{
				mind->leash.obeying = false;
			}
		}
		if (leashes.control == CreatureLeash::Control::Idle)
		{
			leashes.pull = leash::FadePull(leashes.pull);
		}

		if (!leashes.worn.has_value())
		{
			if (mind != nullptr)
			{
				mind->leash.obeying = false;
				mind->leash.forcedDesire.reset();
				mind->leash.learningInHand = false;
				mind->leash.miracleSightingWeight = leash::MiracleSightingWeight(false);
			}
			// Kept within its home, it walks back when it strays
			if (leash::IsConfined(leashes.confinementRadius, false, true) &&
			    leash::OutsideArea(position, glm::vec2(leashes.confinementCentre.x, leashes.confinementCentre.z),
			                       leashes.confinementRadius))
			{
				if (!moving && Locator::creatureLocomotionSystem::has_value())
				{
					Locator::creatureLocomotionSystem::value().MoveTo(
					    entity, glm::vec2(leashes.confinementCentre.x, leashes.confinementCentre.z),
					    CreatureLocomotionSystemInterface::Pace::Walk, 0.0f, leashes.confinementRadius * 0.5f);
					leashes.returning = true;
				}
			}
			else
			{
				leashes.returning = false;
			}
			continue;
		}

		// Made someone else's or no longer the one its owner leads, the leash comes off
		if (!body->leashable || leashes.worn->holder != body->owner)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {}'s leash comes off: it isn't its holder's to lead",
			                   entt::to_integral(entity));
			TakeOff(entity);
			continue;
		}
		auto& worn = *leashes.worn;
		if (worn.tiedTo.has_value() &&
		    (!registry.Valid(*worn.tiedTo) || registry.TryGet<const Transform>(*worn.tiedTo) == nullptr))
		{
			worn.tiedTo.reset();
			worn.ropeStarted = false;
		}
		const auto hand = HandPoint();
		// It is kept as near what holds the leash as the leash is long
		if (worn.tiedTo.has_value())
		{
			leashes.confinementCentre = registry.Get<const Transform>(*worn.tiedTo).position;
		}
		else if (hand.has_value())
		{
			leashes.confinementCentre = *hand;
		}
		leashes.confinementRadius = worn.rope.maxLength;

		// The leash's feelings, every turn
		if (mind != nullptr)
		{
			mind->leash.forcedDesire = leash::ForcedDesireFor(worn.type);
			mind->leash.forcedValue = mind->leash.forcedDesire.has_value() ? leash::k_ForcedDesireValue : 0.0f;
			mind->leash.learningInHand = worn.type == LeashType::Rope && !worn.tiedTo.has_value();
			mind->leash.miracleSightingWeight = leash::MiracleSightingWeight(worn.type == LeashType::Rope);
		}

		if (worn.tiedTo.has_value())
		{
			const auto object = *worn.tiedTo;
			// Tied to someone else's village, or its own, on any leash but aggression, it wants to impress it
			if (registry.TryGet<const Town>(object) != nullptr && worn.type != LeashType::Evil && mind != nullptr)
			{
				mind->leash.forcedDesire = creature_desires::Desire::Impress;
				mind->leash.forcedValue = leash::k_ImpressTownValue;
			}
			// Tied to another creature: anger spreads on the aggression leash, and they warm or cool to each other
			if (registry.TryGet<const Creature>(object) != nullptr)
			{
				++leashes.turnsWithOther;
				auto* otherMind = MindOf(registry, object);
				const auto otherAt = registry.Get<const Transform>(object).position;
				if (worn.type == LeashType::Evil && otherMind != nullptr &&
				    glm::distance(transform->position, otherAt) < leash::k_AngerOtherReach * CreatureHeight(*body))
				{
					otherMind->leash.forcedDesire = creature_desires::Desire::Anger;
					otherMind->leash.forcedValue = leash::k_ForcedDesireValue;
				}
				if (const auto change = leash::AttitudeChange(worn.type, leashes.turnsWithOther); change != 0.0f)
				{
					if (mind != nullptr)
					{
						mind->leash.attitudes.push_back({.creature = static_cast<uint32_t>(object), .change = change});
					}
					if (otherMind != nullptr)
					{
						otherMind->leash.attitudes.push_back({.creature = static_cast<uint32_t>(entity), .change = change});
					}
				}
			}
			// Strayed beyond the leash's length, it walks back to what it is tied to
			if (worn.works && !moving &&
			    leash::OutsideArea(position, glm::vec2(leashes.confinementCentre.x, leashes.confinementCentre.z),
			                       worn.rope.slackLength) &&
			    Locator::creatureLocomotionSystem::has_value())
			{
				Locator::creatureLocomotionSystem::value().LeadTo(
				    entity, glm::vec2(leashes.confinementCentre.x, leashes.confinementCentre.z), leashes.pull,
				    worn.rope.slackLength * 0.5f);
			}
			continue;
		}

		// Held in the hand: a taut rope pulls it to the hand, unless it is carrying out an order
		if (worn.works && leash::ShouldPull(worn.rope.tension) && !leashes.order.has_value())
		{
			Pull(entity);
		}
	}
}

void LeashSystem::Update(float seconds)
{
	UpdateMarkers(seconds);
	UpdatePosts(seconds);
	auto& registry = Locator::entitiesRegistry::value();
	const auto hand = HandPoint();
	registry.Each<CreatureLeash, const Creature, const Transform>(
	    [&](entt::entity entity, CreatureLeash& leashes, const Creature& /*creature*/, const Transform& /*transform*/) {
		    if (!leashes.worn.has_value())
		    {
			    return;
		    }
		    auto& worn = *leashes.worn;
		    std::optional<glm::vec3> start;
		    if (worn.tiedTo.has_value() && registry.Valid(*worn.tiedTo) &&
		        registry.TryGet<const Transform>(*worn.tiedTo) != nullptr)
		    {
			    start = TiedPoint(registry, *worn.tiedTo);
		    }
		    else
		    {
			    start = hand;
		    }
		    if (!start.has_value())
		    {
			    // The hand is off the land: the rope keeps its last place
			    return;
		    }
		    const auto end = CollarPoint(registry, entity);
		    // Held in the hand, its length follows the creature's size
		    if (!worn.tiedTo.has_value() || !worn.ropeStarted)
		    {
			    const auto lengths = LengthsOf(registry, entity, worn);
			    worn.rope.slackLength = lengths.slack;
			    worn.rope.maxLength = lengths.max;
		    }
		    if (!worn.ropeStarted)
		    {
			    worn.rope =
			        leash_rope::Create(*start, end, worn.rope.slackLength, worn.rope.maxLength, leash::LookFor(worn.type));
			    worn.ropeStarted = true;
			    return;
		    }
		    worn.rope.look = leash::LookFor(worn.type);
		    leash_rope::Step(worn.rope, *start, end, seconds, GroundAt);
	    });
}

void LeashSystem::HandleInput(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, glm::vec2 cursor,
                              uint32_t milliseconds, bool actionTaken)
{
	if (!Locator::gameActionSystem::has_value())
	{
		return;
	}
	using input::BindableActionMap;
	const auto& actions = Locator::gameActionSystem::value();
	const auto player = k_LocalPlayer;
	const auto creature = PlayersCreature(player);
	auto& registry = Locator::entitiesRegistry::value();
	const auto pressed = [&actions](BindableActionMap action) { return actions.GetChanged(action) && actions.Get(action); };

	// The leash shortcuts
	for (const auto action :
	     {BindableActionMap::LEASH_UNLEASH_CREATURE, BindableActionMap::PREVIOUS_LEASH, BindableActionMap::NEXT_LEASH})
	{
		if (const auto key = leash::KeyFor(action); key.has_value() && pressed(action))
		{
			PressKey(player, *key);
		}
	}

	if (actionTaken || !pressed(BindableActionMap::ACTION) || glm::length(rayDirection) <= 0.0f)
	{
		return;
	}
	const bool doubleTap = _doubleTaps.OnPress(milliseconds, cursor);
	const auto direction = glm::normalize(rayDirection);
	// The player's leash posts along the ray come first
	std::optional<entt::entity> post;
	float nearest = k_TapReach;
	registry.Each<const LeashPost, const Transform>([&](entt::entity entity, const LeashPost& at, const Transform&) {
		// Only the leashes hanging there can be tapped, where they hang even when the picked one is carried
		if (at.owner != player || !at.hung)
		{
			return;
		}
		if (const auto along = RayBall(rayOrigin, direction, at.point, temple_leashes::k_TapRadius);
		    along.has_value() && *along < nearest)
		{
			nearest = *along;
			post = entity;
		}
	});
	if (post.has_value())
	{
		TapPost(*post);
		return;
	}
	if (!creature.has_value())
	{
		return;
	}

	// What the interface picked under the cursor: a thing, or else the land or the sea
	std::optional<entt::entity> object;
	std::optional<glm::vec3> land;
	if (Locator::pickingSystem::has_value())
	{
		const auto& pick = Locator::pickingSystem::value().GetPick();
		object = pick.object;
		land = pick.land;
	}
	if (!object.has_value())
	{
		// A creature along the ray, when the interface picked none
		nearest = k_TapReach;
		registry.Each<const Creature, const Transform>([&](entt::entity entity, const Creature& body, const Transform& at) {
			const auto height = CreatureHeight(body);
			const auto centre = at.position + glm::vec3(0.0f, height * 0.5f, 0.0f);
			if (const auto along = RayBall(rayOrigin, direction, centre, height * k_CreatureTapShare);
			    along.has_value() && *along < nearest)
			{
				nearest = *along;
				object = entity;
			}
		});
	}
	if (object.has_value() && !registry.Valid(*object))
	{
		object.reset();
	}

	const auto* leashes = registry.TryGet<const CreatureLeash>(*creature);
	const bool worn = leashes != nullptr && leashes->worn.has_value() && leashes->worn->holder == player;
	const orders::Leash state {
	    .worn = worn,
	    .tiedTo =
	        worn && leashes->worn->tiedTo.has_value() ? std::optional(entt::to_integral(*leashes->worn->tiedTo)) : std::nullopt,
	    .creature = entt::to_integral(*creature),
	};
	// The temple's taps are its own: on its entrance they take the player inside
	const orders::Tapped tapped {
	    .object = object.has_value() ? std::optional(entt::to_integral(*object)) : std::nullopt,
	    .leashTarget = !object.has_value() || !registry.AnyOf<LeashPost, Temple, LeashMarker>(*object),
	    .miracleBubble = object.has_value() && registry.AllOf<OneOffSpellSeed>(*object),
	};
	switch (doubleTap ? orders::OnDoubleTap(state, tapped) : orders::OnTap(state, tapped))
	{
	case orders::Tap::OrderOnThing:
		OrderOn(player, *object);
		break;
	case orders::Tap::OrderOnLand:
		if (land.has_value())
		{
			OrderAt(player, *land);
		}
		break;
	case orders::Tap::Tie:
	{
		// Grown up enough, it can be tied to things, and it leaves what it was doing for the thing
		const auto* mind = registry.TryGet<const CreatureMindState>(*creature);
		if (mind != nullptr && mind->developmentPhase <= orders::k_TyingPhase)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {} is too young to have its leash tied to things",
			                   entt::to_integral(*creature));
			break;
		}
		if (TieTo(*creature, *object))
		{
			TakeThingOrder(*creature, *object);
		}
		break;
	}
	case orders::Tap::Untie:
		UntieToHand(*creature);
		break;
	case orders::Tap::Normal:
	case orders::Tap::Nothing:
		break;
	}
}

bool LeashSystem::OrderAt(PlayerNames player, const glm::vec3& place)
{
	const auto creature = PlayersCreature(player);
	if (!creature.has_value() || !IsLeashed(*creature))
	{
		return false;
	}
	return TakeGroundOrder(*creature, place);
}

bool LeashSystem::OrderOn(PlayerNames player, entt::entity object)
{
	const auto creature = PlayersCreature(player);
	if (!creature.has_value() || !IsLeashed(*creature))
	{
		return false;
	}
	return TakeThingOrder(*creature, object);
}

bool LeashSystem::TakeGroundOrder(entt::entity creature, const glm::vec3& place)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!orders::TakesOrders(BodyOf(registry, creature)))
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {} takes no orders now", entt::to_integral(creature));
		return false;
	}
	auto& leashes = registry.Get<CreatureLeash>(creature);
	const auto& body = registry.Get<const Creature>(creature);
	const auto& transform = registry.Get<const Transform>(creature);
	const auto* mind = registry.TryGet<const CreatureMindState>(creature);
	const auto* needs = registry.TryGet<const CreatureNeeds>(creature);
	const glm::vec2 at {place.x, place.z};
	const glm::vec2 position {transform.position.x, transform.position.z};
	const auto height = CreatureHeight(body);

	// Where it can stand nearest the place
	bool placeReachable = true;
	std::optional<glm::vec2> reachable = at;
	if (Locator::creatureLocomotionSystem::has_value())
	{
		const auto& walkable = Locator::creatureLocomotionSystem::value().GetWalkableLand();
		placeReachable = walkable.IsValid(at, creature_route::k_DestinationClearance);
		if (!placeReachable)
		{
			reachable = walkable.NearestValid(at, creature_route::k_DestinationClearance, orders::k_NearestReachableSearch);
		}
	}
	const auto home = CitadelHome(registry, creature);
	const auto field = FieldAt(registry, at);
	const bool wantsWater = mind != nullptr && mind->desires.has_value() && (*mind->desires)[Desire::Water].activated;
	const auto order = orders::OnGround(
	    at,
	    {
	        .carrying = HeldBy(creature).has_value(),
	        .reachable = reachable,
	        .placeReachable = placeReachable,
	        .water = IsWater(at),
	        .thirsty = wantsWater && needs != nullptr && needs->needs.dehydration > 0.0f,
	        .field = field.has_value(),
	        .playerHasCitadel = home.has_value(),
	        .exhaustion = needs != nullptr ? needs->needs.exhaustion : 0.0f,
	        .sleeping = mind != nullptr && mind->idle.activity == creature_mind::Activity::Sleep,
	        .distanceFromHome = home.has_value() ? std::optional(glm::distance(glm::vec2(home->x, home->z), at)) : std::nullopt,
	        .leash = TypeOf(creature),
	        .orderInForce = OrderInForce(creature),
	        .height = height,
	    });

	using Kind = orders::GroundOrder::Kind;
	if (order.kind == Kind::Inaccessible)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {} can't get there: it is inaccessible", entt::to_integral(creature));
		leashes.help = CreatureLeash::Help::Inaccessible;
		leashes.helpDesire.reset();
		return false;
	}
	if (order.acknowledged)
	{
		Acknowledge(creature);
	}
	const auto markAt = glm::vec3(order.point.x, GroundAt(order.point), order.point.y);
	if (order.kind == Kind::ActOnField)
	{
		// The place is marked, then the field is acted on as if it had been tapped
		MarkOrder(creature, std::nullopt, markAt, false);
		return TakeThingOrder(creature, *field);
	}

	std::optional<Desire> desire;
	bool taken = false;
	switch (order.kind)
	{
	case Kind::Drink:
		desire = Desire::Water;
		taken = Force(creature, {.desire = *desire, .action = orders::k_DrinkAction});
		break;
	case Kind::LookAtReflection:
		desire = Desire::Water;
		taken = Force(creature, {.desire = *desire, .action = orders::k_ReflectionAction});
		break;
	case Kind::SleepAtHome:
		taken = Force(creature,
		              {.desire = Desire::Tiredness, .action = orders::k_SleepAction, .point = glm::vec2(home->x, home->z)});
		break;
	case Kind::MoveTo:
	{
		const auto extra =
		    Locator::gameRandom::has_value() ? Locator::gameRandom::value().GameFloatRand(orders::k_RunWaitExtraSeconds) : 0.0f;
		const auto wait = orders::RunWaitSeconds(extra, k_TurnsPerSecond);
		taken = Force(creature, {.desire = Desire::ObeyPlayer,
		                         .action = orders::k_MoveAction,
		                         .agenda = orders::MoveTo(order.point, order.arrival, order.runAndWait, wait)});
		// Sent far, it says which desire it acts on
		if (glm::distance(at, position) > orders::k_TellDesireDistance)
		{
			desire = Desire::ObeyPlayer;
		}
		break;
	}
	case Kind::PutDownAt:
		taken = Force(
		    creature,
		    {.desire = Desire::ObeyPlayer, .action = orders::k_MoveAction, .agenda = orders::PutDownAt(order.point, height)});
		break;
	case Kind::ThrowAt:
		taken = Force(
		    creature,
		    {.desire = Desire::ObeyPlayer, .action = orders::k_MoveAction, .agenda = orders::ThrowAt(order.point, height)});
		break;
	case Kind::Inaccessible:
	case Kind::ActOnField:
		break;
	}
	if (!taken)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {} couldn't carry out the order", entt::to_integral(creature));
		return false;
	}
	if (desire.has_value())
	{
		leashes.help = CreatureLeash::Help::CurrentDesire;
		leashes.helpDesire = desire;
	}
	// Carrying, the place itself is marked; otherwise where it goes
	const bool carried = order.kind == Kind::PutDownAt || order.kind == Kind::ThrowAt;
	MarkOrder(creature, std::nullopt, carried ? glm::vec3(at.x, GroundAt(at), at.y) : markAt, false);
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {} is told to go to ({:.1f}, {:.1f})", entt::to_integral(creature),
	                   order.point.x, order.point.y);
	return true;
}

bool LeashSystem::TakeThingOrder(entt::entity creature, entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object) || registry.TryGet<const Transform>(object) == nullptr || object == creature)
	{
		return false;
	}
	if (!orders::TakesOrders(BodyOf(registry, creature)))
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {} takes no orders now", entt::to_integral(creature));
		return false;
	}
	const auto objectAt = registry.Get<const Transform>(object).position;
	if (registry.AllOf<BigForest>(object))
	{
		return TakeGroundOrder(creature, objectAt);
	}
	const auto& body = registry.Get<const Creature>(creature);
	const auto& transform = registry.Get<const Transform>(creature);
	const auto height = CreatureHeight(body);
	const auto type = TypeOf(creature);
	const bool isCreature = registry.AllOf<Creature>(object);
	const auto attempts = orders::OnThing({
	    .carrying = HeldBy(creature).has_value(),
	    .liftable =
	        Locator::creatureObjectActionSystem::has_value() && Locator::creatureObjectActionSystem::value().CanPickUp(object),
	    .creature = isCreature,
	    .distance = glm::distance(transform.position, objectAt),
	    .leash = type,
	    .height = height,
	});
	if (orders::Acknowledges(attempts))
	{
		Acknowledge(creature);
	}
	const bool previously = OrderInForce(creature);
	std::optional<Desire> desire;
	bool fight = false;
	for (const auto attempt : attempts)
	{
		switch (attempt)
		{
		case orders::Attempt::GoToForest:
			return TakeGroundOrder(creature, objectAt);
		case orders::Attempt::FishAndEat:
			if (Force(creature, {.desire = Desire::Hunger, .action = orders::k_FishAction, .object = object}))
			{
				desire = Desire::Hunger;
			}
			break;
		case orders::Attempt::DesiresUsingCarried:
		case orders::Attempt::Desires:
			if (Locator::creatureMindSystem::has_value())
			{
				desire = Locator::creatureMindSystem::value().ForcePlanOn(creature, object);
			}
			break;
		case orders::Attempt::Fight:
			if (Locator::creatureFightSystem::has_value() &&
			    Locator::creatureFightSystem::value().StartFight(creature, object) ==
			        CreatureFightSystemInterface::StartResult::Started)
			{
				desire = Desire::Anger;
				fight = true;
			}
			break;
		case orders::Attempt::Hold:
		case orders::Attempt::Look:
			if (Force(creature, {.desire = Desire::Curiosity,
			                     .action = attempt == orders::Attempt::Hold ? orders::k_HoldAction : orders::k_LookAction,
			                     .object = object}))
			{
				desire = Desire::Curiosity;
			}
			break;
		}
		if (desire.has_value())
		{
			break;
		}
	}
	if (!desire.has_value())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {} found nothing to do to {}", entt::to_integral(creature),
		                   entt::to_integral(object));
		return false;
	}
	auto* mind = MindOf(registry, creature);
	// Told again before it was done, it runs
	if (previously && mind != nullptr)
	{
		for (auto& step : mind->idle.agenda)
		{
			if (step.kind == creature_mind::Step::Kind::Move)
			{
				step.movement.run = true;
			}
		}
	}
	auto& leashes = registry.Get<CreatureLeash>(creature);
	leashes.help = CreatureLeash::Help::CurrentDesire;
	leashes.helpDesire = desire;
	MarkOrder(creature, object, objectAt, fight);
	// What it is shown on the aggression and compassion leashes teaches it which desire to act on such things with
	if (auto lessons = leash::LessonsFor(type, isCreature); mind != nullptr && !lessons.empty())
	{
		mind->leash.shown.push_back({.object = entt::to_integral(object), .type = type, .lessons = std::move(lessons)});
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Creature {} is told to act on {}", entt::to_integral(creature),
	                   entt::to_integral(object));
	return true;
}

void LeashSystem::Acknowledge(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* leashes = registry.TryGet<const CreatureLeash>(creature);
	// The player whose leash it is hears the order taken
	if (leashes != nullptr && leashes->worn.has_value() && leashes->worn->holder == k_LocalPlayer)
	{
		PlaySound(audio::SoundId::G_AcknowledgeCommand, std::nullopt);
	}
	if (Locator::creatureAudioSystem::has_value())
	{
		Locator::creatureAudioSystem::value().Play(
		    creature,
		    {.kind = creature_audio::EventKind::Voice, .timeMs = 0, .action = audio::SoundAction::Acknowledge, .mode = 0});
	}
	if (Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().PlayGesture(creature, creature_layers::animations::k_FirstGesture);
	}
}

void LeashSystem::MarkOrder(entt::entity creature, std::optional<entt::entity> object, const glm::vec3& point, bool fight)
{
	auto& registry = Locator::entitiesRegistry::value();
	EndOrder(creature);
	auto& leashes = registry.Get<CreatureLeash>(creature);
	const auto* mind = registry.TryGet<const CreatureMindState>(creature);
	CreatureLeash::Order order {
	    .serial = mind != nullptr ? mind->idle.serial : 0,
	    .fight = fight,
	    .object = object,
	    .point = point,
	};
	// The sparkles, which every player sees, stay where they were made
	if (Locator::particleSystem::has_value())
	{
		order.sparkles = Locator::particleSystem::value().StartSpotVisual(SpotVisualType::CreatureTarget, point,
		                                                                  orders::k_SparkleTurns, creature, 1.0f);
	}
	// The ring only its player sees
	if (leashes.worn.has_value() && leashes.worn->holder == k_LocalPlayer)
	{
		if (const auto ring =
		        archetypes::LeashMarkerArchetype::Create(creature, registry.Get<const Creature>(creature).species))
		{
			order.ring = ring->first;
			order.footprint = ring->second;
		}
	}
	leashes.order = order;
	registry.SetDirty();
}

void LeashSystem::EndOrder(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* leashes = registry.TryGet<CreatureLeash>(creature);
	if (leashes == nullptr || !leashes->order.has_value())
	{
		return;
	}
	const auto order = *leashes->order;
	leashes->order.reset();
	if (order.sparkles != ParticleSystemInterface::k_NoEffect && Locator::particleSystem::has_value() &&
	    Locator::particleSystem::value().IsRunning(order.sparkles))
	{
		Locator::particleSystem::value().CloseDown(order.sparkles);
	}
	for (const auto sprite : {order.ring, order.footprint})
	{
		if (sprite != entt::null && registry.Valid(sprite))
		{
			registry.Destroy(sprite);
		}
	}
	registry.SetDirty();
}

bool LeashSystem::OrderInForce(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* leashes = registry.TryGet<const CreatureLeash>(creature);
	if (leashes == nullptr || !leashes->order.has_value())
	{
		return false;
	}
	const auto& order = *leashes->order;
	if (order.object.has_value() &&
	    (!registry.Valid(*order.object) || registry.TryGet<const Transform>(*order.object) == nullptr))
	{
		return false;
	}
	if (order.fight)
	{
		return Locator::creatureFightSystem::has_value() && Locator::creatureFightSystem::value().IsFighting(creature);
	}
	const auto* mind = registry.TryGet<const CreatureMindState>(creature);
	return mind != nullptr && mind->idle.serial == order.serial && mind->idle.step < mind->idle.agenda.size();
}

std::optional<glm::vec3> LeashSystem::OrderTarget(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* leashes = registry.TryGet<const CreatureLeash>(creature);
	if (leashes == nullptr || !leashes->order.has_value())
	{
		return std::nullopt;
	}
	const auto& order = *leashes->order;
	if (order.object.has_value() && registry.Valid(*order.object))
	{
		if (const auto* at = registry.TryGet<const Transform>(*order.object))
		{
			return orders::MarkerOverThing(at->position, ObjectHeight(registry, *order.object));
		}
	}
	return orders::MarkerOverLand(order.point);
}

void LeashSystem::UpdateMarkers(float seconds)
{
	auto& registry = Locator::entitiesRegistry::value();
	_markerClockMs = std::fmod(_markerClockMs + (seconds * 1000.0f), static_cast<float>(orders::k_PulsePeriodMs));
	const auto size = orders::MarkerSize(static_cast<uint32_t>(_markerClockMs));
	// The footprint is a quarter turn round, in the plane facing the camera
	const auto quarterTurn = glm::mat3(glm::rotate(-0.5f * std::numbers::pi_v<float>, glm::vec3(0.0f, 0.0f, 1.0f)));
	std::vector<std::pair<entt::entity, CreatureLeash::Order>> marked;
	registry.Each<const CreatureLeash>([&marked](entt::entity creature, const CreatureLeash& leashes) {
		if (leashes.order.has_value() && leashes.order->ring != entt::null)
		{
			marked.emplace_back(creature, *leashes.order);
		}
	});
	for (const auto& [creature, order] : marked)
	{
		const auto at = OrderTarget(creature);
		if (!at.has_value() || !registry.Valid(order.ring) || !registry.Valid(order.footprint))
		{
			continue;
		}
		auto& ring = registry.Get<Transform>(order.ring);
		ring.position = *at;
		ring.scale = glm::vec3(size, 1.0f);
		auto& footprint = registry.Get<Transform>(order.footprint);
		footprint.position = *at;
		footprint.rotation = quarterTurn;
		footprint.scale = glm::vec3(size * orders::k_FootprintShare, 1.0f);
	}
}

std::optional<uint32_t> LeashSystem::ToolTip(PlayerNames player, std::optional<entt::entity> hovered) const
{
	const auto creature = PlayersCreature(player);
	if (!creature.has_value())
	{
		return std::nullopt;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* leashes = registry.TryGet<const CreatureLeash>(*creature);
	const bool worn = leashes != nullptr && leashes->worn.has_value() && leashes->worn->holder == player;
	if (hovered.has_value() && !registry.Valid(*hovered))
	{
		hovered.reset();
	}
	// Over one of the player's leashes on the temple, the hand names it
	if (const auto* post = hovered.has_value() ? registry.TryGet<const LeashPost>(*hovered) : nullptr;
	    post != nullptr && post->owner == player && post->hung)
	{
		return temple_leashes::ToolTipOf(post->type);
	}
	const auto* mind = registry.TryGet<const CreatureMindState>(*creature);
	return orders::ToolTipFor(
	    {.worn = worn,
	     .tiedTo = worn && leashes->worn->tiedTo.has_value() ? std::optional(entt::to_integral(*leashes->worn->tiedTo))
	                                                         : std::nullopt,
	     .creature = entt::to_integral(*creature)},
	    {.object = hovered.has_value() ? std::optional(entt::to_integral(*hovered)) : std::nullopt,
	     .leashTarget = hovered.has_value() && !registry.AnyOf<LeashPost, Temple, LeashMarker>(*hovered),
	     .miracleBubble = hovered.has_value() && registry.AllOf<OneOffSpellSeed>(*hovered),
	     .developmentPhase = mind != nullptr ? mind->developmentPhase : CreatureMindState::k_FullyGrownUp});
}
