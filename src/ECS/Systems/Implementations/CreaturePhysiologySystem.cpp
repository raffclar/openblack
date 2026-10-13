/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CreaturePhysiologySystem.h"

#include <cmath>

#include <algorithm>
#include <chrono>
#include <numbers>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "3D/ChimneySmoke.h"
#include "3D/CreatureBody.h"
#include "3D/DayNightClock.h"
#include "3D/LandIslandInterface.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureFart.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreaturePhysiology.h"
#include "Creature/CreatureRig.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Archetypes/MobileObjectArchetype.h"
#include "ECS/Components/ChimneySmoke.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureSquat.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "Graphics/Texture2D.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
namespace physiology = openblack::creature_physiology;

namespace
{
constexpr float k_TurnsPerSecond = 1.0f / std::chrono::duration<float>(TimeSystemInterface::k_TurnDuration).count();
/// Without a stage of growing up to go by, a creature's body works as a grown up's does
constexpr uint32_t k_GrownUpPhase = 13;
/// The time scale can't run more than this many turns of the body in a game turn
constexpr float k_MaxTimeScale = 3600.0f;

/// Sick comes out as this many drops from the creature's mouth, about this high and this far ahead for its size, flung
/// forwards and up, a little to either side, and lies on the land a while before it is gone
constexpr int k_PukeDrops = 12;
constexpr float k_MouthHeight = 9.0f;
constexpr float k_MouthAhead = 3.0f;
constexpr float k_PukeSpeed = 5.0f;
constexpr float k_PukeSpread = 2.0f;
constexpr float k_PukeDropScale = 0.5f;
constexpr float k_PukeSeconds = 4.0f;
constexpr float k_PukeFadeSeconds = 1.5f;
constexpr float k_Gravity = 9.81f;
/// The sick drops are a frame of the land effects' sprite sheet, in greenish colours
constexpr entt::hashed_string k_PukeSpriteSheet = entt::hashed_string("raw/S_SpriteSheet3a");
constexpr float k_PukeSheetCell = 1.0f / 8.0f;

const GCreatureInfo* SpeciesInfo(CreatureType species)
{
	if (!Locator::infoConstants::has_value())
	{
		return nullptr;
	}
	const auto& creatures = Locator::infoConstants::value().creature;
	const auto row = creature::InfoRow(species);
	return row < creatures.size() ? &creatures.at(row) : nullptr;
}

/// What the species' body is like, from its row of the game's creature table
physiology::Species SpeciesOf(CreatureType species)
{
	const auto* info = SpeciesInfo(species);
	if (info == nullptr)
	{
		return {};
	}
	return {
	    .startEnergy = info->startEnergy,
	    .startWarmth = info->startWarmth,
	    .comfortTemperature = info->comfortTemperature,
	    .secondsPerAgeTick = info->secondsPerAgeTick,
	    .growUpMinutes = info->growUpMinutes,
	    .lowEnergyThreshold = info->lowEnergyThreshold,
	    .exhaustionRate = info->exhaustionRate,
	    .secondsToDehydrate = info->secondsToDehydrate,
	    .strengthDecay = info->strengthDecay,
	    .carryStrengthMinutes = info->carryStrengthMinutes,
	    .energyDrain = info->energyDrain,
	    .fatBurn = info->fatBurn,
	    .overeatFatFactor = info->overeatFatFactor,
	    .sleepHeal = info->sleepHeal,
	    .sleepRecover = info->sleepRecover,
	    .sleepLength = info->sleepLength,
	    .foodToEnergy = info->foodToEnergy,
	    .pooPerEnergy = info->pooPerEnergy,
	};
}

uint32_t PhaseOf(ecs::Registry& registry, entt::entity entity)
{
	const auto* mind = registry.TryGet<const CreatureMindState>(entity);
	return mind != nullptr ? mind->developmentPhase : k_GrownUpPhase;
}

bool IsNight()
{
	return Locator::skySystem::has_value() && Locator::skySystem::value().GetClock().IsVisualNight();
}

float TemperatureAt(const glm::vec3& position)
{
	if (!Locator::weatherSystem::has_value())
	{
		return 0.0f;
	}
	return static_cast<float>(Locator::weatherSystem::value().GetWeather(position).temperature);
}

float HeightAt(glm::vec2 point)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(point) : 0.0f;
}

bool IsMoving(const CreatureLocomotion* locomotion)
{
	using Motion = CreatureLocomotion::Motion;
	return locomotion != nullptr && (locomotion->motion == Motion::Walking || locomotion->motion == Motion::Stepping ||
	                                 locomotion->motion == Motion::Turning);
}

/// The creature's body as its needs see it, and back
physiology::Shape ShapeOf(const Creature& creature)
{
	return {.fatness = creature.fatness, .strength = creature.strength, .size = creature.size, .penSize = creature.penSize};
}

/// Takes on a changed shape; returns whether its drawn size changed
bool TakeShape(Creature& creature, Transform* transform, const physiology::Shape& shape)
{
	creature.fatness = shape.fatness;
	creature.strength = shape.strength;
	if (shape.size == creature.size)
	{
		return false;
	}
	creature.size = shape.size;
	// In its pen the pen sets how big it is drawn
	if (transform != nullptr && !creature.penSize.has_value())
	{
		transform->scale = glm::vec3(ecs::archetypes::CreatureArchetype::DrawnScale(creature.species, creature.size));
	}
	return true;
}

void StartNeeds(CreatureNeeds& needs, const physiology::Species& species)
{
	if (!needs.started)
	{
		needs.needs = needs.kept.has_value() ? physiology::Start(species, *needs.kept) : physiology::Start(species);
		needs.kept.reset();
		needs.started = true;
	}
}

/// Where the creature faces on the land, for its rotation; its mesh looks back along +z
glm::vec3 AheadOf(const Transform& transform)
{
	auto ahead = -(transform.rotation * glm::vec3(0.0f, 0.0f, 1.0f));
	ahead.y = 0.0f;
	return glm::length(ahead) > 0.0f ? glm::normalize(ahead) : glm::vec3(0.0f, 0.0f, -1.0f);
}
/// Where the creature's groin is as its body is posed this frame; its feet when its species' bones aren't known
glm::vec3 GroinOf(const ecs::Registry& registry, entt::entity creature, const Creature& self, const Transform& transform)
{
	const auto* animation = registry.TryGet<const CreatureAnimation>(creature);
	if (animation == nullptr || !Locator::resources::has_value())
	{
		return transform.position;
	}
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto rigId = creature::GetRigId(self.species);
	if (!rigs.Contains(rigId) || !rigs.Handle(rigId)->actionPoints.has_value())
	{
		return transform.position;
	}
	const auto placement = creature::PlacementMatrix(transform.position, transform.rotation, transform.scale);
	const auto groin = rigs.Handle(rigId)->actionPoints->groin;
	return glm::vec3(creature::PosedBone(groin, animation->boneMatrices, placement)[3]);
}
} // namespace

void CreaturePhysiologySystem::ProcessTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	_owedTurns += _timeScale;
	const auto turns = static_cast<uint32_t>(std::floor(_owedTurns));
	_owedTurns -= static_cast<float>(turns);
	const bool night = IsNight();
	bool resized = false;
	// The player's creature, whose size the creatures scripts scale to follow
	std::optional<float> playersSize;
	if (Locator::playerSystem::has_value())
	{
		if (const auto players = Locator::playerSystem::value().GetPrimaryCreature(PlayerNames::PLAYER_ONE);
		    players.has_value() && registry.Valid(*players))
		{
			if (const auto* body = registry.TryGet<const Creature>(*players))
			{
				playersSize = body->size;
			}
		}
	}

	registry.Each<Creature, CreatureNeeds>([&](entt::entity entity, Creature& creature, CreatureNeeds& needs) {
		const auto species = SpeciesOf(creature.species);
		StartNeeds(needs, species);
		auto* transform = registry.TryGet<Transform>(entity);
		needs.moving = IsMoving(registry.TryGet<const CreatureLocomotion>(entity));
		const auto phase = PhaseOf(registry, entity);
		if (needs.rest == CreatureNeeds::Rest::Awake)
		{
			needs.restTurns = 0;
			needs.rested = false;
		}

		const physiology::Turn turn {
		    .moving = needs.moving,
		    .asleep = needs.rest == CreatureNeeds::Rest::Asleep,
		    .resting = needs.rest == CreatureNeeds::Rest::Resting,
		    .phase = phase,
		    .carriedWeight = needs.carriedWeight,
		    .temperature = transform != nullptr ? TemperatureAt(transform->position) : 0.0f,
		    .turnsPerSecond = k_TurnsPerSecond,
		};
		auto shape = ShapeOf(creature);
		// Not while a hand holds it
		const bool autoScaled = creature.autoScale.has_value() && playersSize.has_value() && !registry.AllOf<InHand>(entity);
		// While the player's hand holds it to interact with it, its body's turns stand still: it lives no turns and its
		// body neither grows hungry, tired nor older
		const bool interacting =
		    Locator::creatureHandSystem::has_value() && Locator::creatureHandSystem::value().GetCreature() == entity;
		for (uint32_t i = 0; i < (interacting ? 0 : turns); ++i)
		{
			if (autoScaled)
			{
				shape.size = physiology::AutoScaledSize(shape.size, *playersSize, *creature.autoScale);
			}
			physiology::TickTurn(needs.needs, shape, species, turn);
			// Asleep or resting, it heals and rests, and may be ready to wake
			if (turn.asleep || turn.resting)
			{
				++needs.restTurns;
				needs.rested = physiology::SleepTurn(needs.needs, species, shape.size, needs.restTurns, night) || needs.rested;
			}
		}
		resized = TakeShape(creature, transform, shape) || resized;

		needs.faint.reset();
		if (_fainting && needs.rest != CreatureNeeds::Rest::Unconscious)
		{
			const bool owned = creature.owner != PlayerNames::NEUTRAL;
			const bool computer =
			    Locator::playerSystem::has_value() && Locator::playerSystem::value().IsComputerPlayer(creature.owner);
			if (physiology::CanFaintFromNeeds(owned, computer, registry.AllOf<ScriptControlled>(entity)))
			{
				needs.faint = physiology::ShouldFaint(needs.needs, phase, owned);
			}
		}
	});
	if (resized)
	{
		registry.SetDirty();
	}
	// The squats count real game turns. The time scale of the bodies has no effect on them
	StepSquats();
}

void CreaturePhysiologySystem::Update(float seconds)
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> gone;
	registry.Each<CreaturePukeDrop, Sprite, Transform>(
	    [&](entt::entity entity, CreaturePukeDrop& drop, Sprite& sprite, Transform& transform) {
		    drop.seconds += seconds;
		    if (drop.seconds >= k_PukeSeconds)
		    {
			    gone.push_back(entity);
			    return;
		    }
		    const auto ground = HeightAt(glm::vec2(transform.position.x, transform.position.z));
		    if (transform.position.y > ground)
		    {
			    drop.velocity.y -= k_Gravity * seconds;
			    transform.position += drop.velocity * seconds;
			    transform.position.y = std::max(transform.position.y, ground);
		    }
		    // It lies where it landed, fading at the end
		    const auto left = k_PukeSeconds - drop.seconds;
		    sprite.tint.a = std::clamp(left / k_PukeFadeSeconds, 0.0f, 1.0f);
	    });
	for (const auto entity : gone)
	{
		registry.Destroy(entity);
	}
	if (!gone.empty() || registry.Size<CreaturePukeDrop>() > 0)
	{
		registry.SetDirty();
	}
}

void CreaturePhysiologySystem::StepSquats()
{
	if (!Locator::creatureMindSystem::has_value())
	{
		return;
	}
	const auto* tables = Locator::creatureMindSystem::value().GetTables();
	const auto fart = tables != nullptr ? creature_mind_tables::FindAction(*tables, "Fart") : std::nullopt;
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> winds;
	std::vector<entt::entity> lumps;
	registry.Each<const CreatureAnimation, const CreatureMindState>(
	    [&](entt::entity creature, const CreatureAnimation& animation, const CreatureMindState& mind) {
		    if (!creature_fart::PlaysSquatLoop(animation.body))
		    {
			    return;
		    }
		    // The creature counts the game turns of its squat loops
		    auto* squat = registry.TryGet<CreatureSquat>(creature);
		    if (squat == nullptr)
		    {
			    squat = &registry.Assign<CreatureSquat>(creature);
		    }
		    if (!creature_fart::CountTurn(squat->turns))
		    {
			    return;
		    }
		    // A fart makes a cloud. Any other squat drops a lump of poo
		    const bool farting = fart.has_value() && mind.planActive && mind.planner.current.has_value() &&
		                         mind.planner.current->action == *fart;
		    (creature_fart::BurstOf(farting) == creature_fart::Burst::Cloud ? winds : lumps).push_back(creature);
	    });
	for (const auto creature : winds)
	{
		LetOutWind(creature);
	}
	for (const auto creature : lumps)
	{
		DropPooLump(creature);
	}
}

void CreaturePhysiologySystem::LetOutWind(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* self = registry.TryGet<const Creature>(creature);
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (self == nullptr || transform == nullptr || !Locator::gameRandom::has_value())
	{
		return;
	}
	const float size = ShownSize(*self);
	// A cloud of grey dust moves out behind the creature
	const auto ahead = AheadOf(*transform);
	auto& random = Locator::gameRandom::value();
	const auto velocities = creature_fart::PuffVelocities(glm::vec2(ahead.x, ahead.z), size,
	                                                      [&random](float limit) { return random.GameFloatRand(limit); });
	if (Locator::dynamicsSystem::has_value())
	{
		for (const auto& velocity : velocities)
		{
			Locator::dynamicsSystem::value().AddPuff(transform->position, velocity, size * creature_fart::k_PuffSize,
			                                         creature_fart::k_PuffColour);
		}
	}
	// A new white smoke starts at the groin. It replaces the last smoke of the creature
	registry.AssignOrReplace<ChimneySmoke>(
	    creature, chimney_smoke::Create(GroinOf(registry, creature, *self, *transform), chimney_smoke::k_HomeSmoke,
	                                    [&random](float a, float b) { return random.CrtRandom(a, b); }));
	// If the body of the creature looks evil, the villagers near it faint
	const auto* morph = registry.TryGet<const CreatureMorph>(creature);
	if (morph != nullptr && creature_fart::Stinks(morph->drawn.evilGood) && Locator::reactionSystem::has_value())
	{
		// TODO(gossip-and-fainting): the creature must impress the villagers by its size and its last action. The
		// reactions do not have this value yet, thus the fart impresses no villager
		Locator::reactionSystem::value().Create(
		    {.initiator = creature, .type = Reaction::Fainting, .player = self->owner, .position = transform->position});
	}
}

void CreaturePhysiologySystem::Eat(entt::entity creature, float foodValue)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* self = registry.TryGet<Creature>(creature);
	auto* needs = registry.TryGet<CreatureNeeds>(creature);
	if (self == nullptr || needs == nullptr)
	{
		return;
	}
	const auto species = SpeciesOf(self->species);
	StartNeeds(*needs, species);
	auto shape = ShapeOf(*self);
	physiology::Eat(needs->needs, shape, species, foodValue);
	if (TakeShape(*self, registry.TryGet<Transform>(creature), shape))
	{
		registry.SetDirty();
	}
}

void CreaturePhysiologySystem::Drink(entt::entity creature)
{
	if (auto* needs = Locator::entitiesRegistry::value().TryGet<CreatureNeeds>(creature))
	{
		physiology::Drink(needs->needs);
	}
}

void CreaturePhysiologySystem::SitTurn(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* self = registry.TryGet<const Creature>(creature);
	auto* needs = registry.TryGet<CreatureNeeds>(creature);
	if (self == nullptr || needs == nullptr)
	{
		return;
	}
	const auto species = SpeciesOf(self->species);
	StartNeeds(*needs, species);
	physiology::SitTurn(needs->needs, species);
}

void CreaturePhysiologySystem::DropPooLump(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* needs = registry.TryGet<CreatureNeeds>(creature);
	const auto* self = registry.TryGet<const Creature>(creature);
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (needs == nullptr || self == nullptr || transform == nullptr)
	{
		return;
	}
	physiology::Poo(needs->needs);
	// The desire to poo becomes zero and stops for a time
	if (Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().ChangeDesires(creature, [](creature_desires::Desires& desires) {
			desires[creature_desires::Desire::Poo].value = 0.0f;
			creature_desires::Suppress(desires, creature_desires::Desire::Poo, physiology::k_PooHoldsDesireBackSeconds,
			                           k_TurnsPerSecond);
		});
	}
	// The lump comes out at the groin as the body squats, turned any way, and is thrown back along the ground
	if (!Locator::infoConstants::has_value() || !Locator::gameRandom::has_value())
	{
		return;
	}
	const auto ahead = AheadOf(*transform);
	const auto lump =
	    physiology::PooLumpOf(GroinOf(registry, creature, *self, *transform), glm::vec2(ahead.x, ahead.z), ShownSize(*self));
	const auto yaw = Locator::gameRandom::value().GameFloatRand(2.0f * std::numbers::pi_v<float>);
	const auto entity =
	    ecs::archetypes::MobileObjectArchetype::Create(lump.position, MobileObjectInfo::LumpOfPoo, yaw, lump.scale);
	if (Locator::dynamicsSystem::has_value())
	{
		Locator::dynamicsSystem::value().InitialisePhysics(entity, {.velocity = lump.velocity, .thrower = creature});
	}
}

void CreaturePhysiologySystem::EmptyBowels(entt::entity creature)
{
	if (auto* needs = Locator::entitiesRegistry::value().TryGet<CreatureNeeds>(creature); needs != nullptr)
	{
		physiology::Poo(needs->needs);
	}
}

void CreaturePhysiologySystem::Puke(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* self = registry.TryGet<const Creature>(creature);
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (self == nullptr || transform == nullptr || !Locator::resources::has_value())
	{
		return;
	}
	auto& textures = Locator::resources::value().GetTextures();
	if (!textures.Contains(k_PukeSpriteSheet.value()))
	{
		return;
	}
	const auto texture = textures.Handle(k_PukeSpriteSheet.value());
	const auto ahead = AheadOf(*transform);
	const glm::vec3 side {ahead.z, 0.0f, -ahead.x};
	const auto mouth = transform->position + (ahead * (k_MouthAhead * ShownSize(*self))) +
	                   glm::vec3(0.0f, k_MouthHeight * ShownSize(*self), 0.0f);
	std::uniform_real_distribution<float> unit(-1.0f, 1.0f);
	std::uniform_real_distribution<float> green(0.4f, 0.9f);
	for (int i = 0; i < k_PukeDrops; ++i)
	{
		const auto entity = registry.Create();
		const auto velocity = (ahead * (k_PukeSpeed * ShownSize(*self))) +
		                      (side * (unit(_random) * k_PukeSpread * ShownSize(*self))) +
		                      glm::vec3(0.0f, (1.0f + (0.5f * unit(_random))) * k_PukeSpeed * ShownSize(*self) * 0.5f, 0.0f);
		const auto g = green(_random);
		registry.Assign<CreaturePukeDrop>(entity, velocity, 0.0f);
		registry.Assign<Sprite>(entity, texture->GetNativeHandle(), glm::vec2(0.0f), glm::vec2(k_PukeSheetCell),
		                        glm::vec4(g * 0.6f, g, g * 0.2f, 1.0f), false);
		registry.Assign<Transform>(entity, mouth, glm::mat3(1.0f), glm::vec3(k_PukeDropScale * ShownSize(*self)));
	}
}

void CreaturePhysiologySystem::WakeFromFaint(entt::entity creature)
{
	if (auto* needs = Locator::entitiesRegistry::value().TryGet<CreatureNeeds>(creature))
	{
		physiology::WakeFromFaint(needs->needs);
		needs->faint.reset();
	}
}

void CreaturePhysiologySystem::FinishAction(entt::entity creature, std::string_view action)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* self = registry.TryGet<Creature>(creature);
	auto* needs = registry.TryGet<CreatureNeeds>(creature);
	if (self == nullptr || needs == nullptr || !Locator::infoConstants::has_value())
	{
		return;
	}
	const auto& actions = Locator::infoConstants::value().creatureAction;
	const auto found = std::ranges::find_if(actions, [action](const auto& row) {
		return std::string_view(row.name.data(), strnlen(row.name.data(), row.name.size())) == action;
	});
	if (found == actions.end())
	{
		return;
	}
	auto shape = ShapeOf(*self);
	physiology::ApplyActionCost(
	    needs->needs, shape,
	    {.strengthGain = found->strengthGain, .energyCost = found->energyCost, .exhaustionCost = found->exhaustionCost},
	    PhaseOf(registry, creature));
	if (TakeShape(*self, registry.TryGet<Transform>(creature), shape))
	{
		registry.SetDirty();
	}
}

void CreaturePhysiologySystem::ModifyStrength(entt::entity creature, float amount)
{
	if (auto* self = Locator::entitiesRegistry::value().TryGet<Creature>(creature))
	{
		auto shape = ShapeOf(*self);
		physiology::ModifyStrength(shape, amount);
		self->strength = shape.strength;
	}
}

void CreaturePhysiologySystem::SetTimeScale(float scale)
{
	_timeScale = std::clamp(scale, 0.0f, k_MaxTimeScale);
}

float CreaturePhysiologySystem::GetTimeScale() const
{
	return _timeScale;
}

void CreaturePhysiologySystem::SetFaintingEnabled(bool enabled)
{
	_fainting = enabled;
}

bool CreaturePhysiologySystem::IsFaintingEnabled() const
{
	return _fainting;
}
