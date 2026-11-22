/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Creatures casting miracles. A creature casts at an object as big as the object: a little bigger than its radius on the
// ground, a creature by its height, a fire seed's miracle by the caster's own height. The miracle takes its prayer power
// to start with and the creatures' timer from its tables, and the creature pays what it costs to make with its body; it
// can't cast while too tired to, nor at something the miracle can't be cast at. It is cast at the object's feet, or on
// the object for a seed that casts on objects. Each turn its effect flows from between the creature's hands, towards
// what it was cast at. Letting go of it stops a miracle that lasts only while held, such as a lightning bolt.

#define LOCATOR_IMPLEMENTATIONS 1

#include <algorithm>

#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureSpellCasting.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/FlockSpell.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/EffectRequestSystemInterface.h"
#include "ECS/ThingMapPlace.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Magic/SpellBehaviours.h"
#include "MagicSystem.h"
#include "ObjectMeasures.h"
#include "Particles/ParticleSpellLink.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
ecs::Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

/// Where a creature's left and right hands are, as it is posed at the end of its newest game turn; none when it has
/// no hands
std::optional<std::pair<glm::vec3, glm::vec3>> HandBonesOf(entt::entity creature)
{
	const auto& registry = Entities();
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* transform = registry.TryGet<const Transform>(creature);
	const auto* animation = registry.TryGet<const CreatureAnimation>(creature);
	if (body == nullptr || transform == nullptr || animation == nullptr || animation->boneMatrices.empty())
	{
		return std::nullopt;
	}
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto rigId = creature::GetRigId(body->species);
	if (!rigs.Contains(rigId) || !rigs.Handle(rigId)->actionPoints.has_value())
	{
		return std::nullopt;
	}
	const auto right = rigs.Handle(rigId)->actionPoints->rightHand;
	const auto left = right < animation->mirror.size() ? animation->mirror[right] : right;
	const auto pose = creature::TurnPose(animation->newerTurnPose, animation->boneMatrices,
	                                     creature::PlacementMatrix(transform->position, transform->rotation, transform->scale));
	return std::pair {glm::vec3(creature::PosedBone(left, pose.bones, pose.placement)[3]),
	                  glm::vec3(creature::PosedBone(right, pose.bones, pose.placement)[3])};
}

/// Where a creature holds its hands: between its right hand and the left, as it is posed at the end of its newest
/// game turn; where it stands when it has no hands to hold
glm::vec3 HandsOf(entt::entity creature)
{
	const auto& registry = Entities();
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* transform = registry.TryGet<const Transform>(creature);
	const auto* animation = registry.TryGet<const CreatureAnimation>(creature);
	if (body == nullptr || transform == nullptr)
	{
		return glm::vec3(0.0f);
	}
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto rigId = creature::GetRigId(body->species);
	if (animation == nullptr || animation->boneMatrices.empty() || !rigs.Contains(rigId) ||
	    !rigs.Handle(rigId)->actionPoints.has_value())
	{
		return transform->position;
	}
	const auto right = rigs.Handle(rigId)->actionPoints->rightHand;
	const auto left = right < animation->mirror.size() ? animation->mirror[right] : right;
	const auto pose = creature::TurnPose(animation->newerTurnPose, animation->boneMatrices,
	                                     creature::PlacementMatrix(transform->position, transform->rotation, transform->scale));
	const auto rightAt = glm::vec3(creature::PosedBone(right, pose.bones, pose.placement)[3]);
	const auto leftAt = glm::vec3(creature::PosedBone(left, pose.bones, pose.placement)[3]);
	return (rightAt + leftAt) * 0.5f;
}

/// The creature's body as it pays for miracles, and its species' casting values
std::optional<std::pair<creature_spell_casting::Body, creature_spell_casting::Rates>> CasterBodyOf(entt::entity creature)
{
	const auto& registry = Entities();
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* needs = registry.TryGet<const CreatureNeeds>(creature);
	const auto& info = Locator::infoConstants::value();
	if (body == nullptr || needs == nullptr)
	{
		return std::nullopt;
	}
	const auto row = creature::InfoRow(body->species);
	if (row >= info.creature.size())
	{
		return std::nullopt;
	}
	const auto& species = info.creature.at(row);
	return std::pair {creature_spell_casting::Body {.size = ShownSize(*body),
	                                                .strength = body->strength,
	                                                .energy = needs->needs.energy,
	                                                .exhaustion = needs->needs.exhaustion},
	                  creature_spell_casting::Rates {.chantsPerEnergy = species.chantsPerEnergy,
	                                                 .energyFloor = species.spellEnergyFloor,
	                                                 .sizeFactor = species.spellSizeFactor}};
}
} // namespace

namespace
{
/// Casting from above, the creature's effect flows from at least this far above what it is cast at, or this share of its
/// height above it
constexpr float k_CastFromAboveLeast = 10.0f;
constexpr float k_CastFromAboveShare = 1.3f;
/// The beams of a cast from above are this big, and this much bigger for each of the creature's size
constexpr float k_BeamSizeLeast = 0.4f;
constexpr float k_BeamSizePerSize = 0.3f;
} // namespace

bool MagicSystem::CanCreatureCastAt(MagicType type, entt::entity target)
{
	switch (magic::ClassOf(type))
	{
	case magic::SpellClass::Creature:
		return CanCastOn(type, target);
	case magic::SpellClass::Forest:
		// A forest goes only to a place, never to a thing
		return false;
	default:
		break;
	}
	const auto position = object_measures::PositionOf(Entities(), target);
	return position.has_value() && magic::spells::CanCastAtPointItself(*this, type, *position);
}

bool MagicSystem::CastByCreature(entt::entity creature, MagicType type, entt::entity target)
{
	ReleaseCreatureCast(creature);
	auto& registry = Entities();
	const auto at = object_measures::PositionOf(registry, target);
	if (!registry.AllOf<Creature>(creature) || !at.has_value() || static_cast<size_t>(type) >= magic::k_MagicTypeCount ||
	    type == MagicType::None)
	{
		return false;
	}
	// As big as what it is cast at
	const auto* targetCreature = registry.TryGet<const Creature>(target);
	const auto seed = magic::FindFirstSpellSeedForMagicType(Info(), type);
	const float magnitude = creature_spell_casting::CastMagnitude(
	    object_measures::TwoDRadius(registry, target),
	    targetCreature != nullptr ? std::optional(ShownSize(*targetCreature)) : std::nullopt, seed == SpellSeedType::Fire,
	    object_measures::Height(registry, creature));
	// The miracle goes to the map position of the thing. A thing that stands at the place of another thing, such as the
	// flag of a worship site, gives the map position of that place.
	const auto* transform = registry.TryGet<const Transform>(target);
	const auto place = transform != nullptr && transform->position == *at ? thing_map_place::OfWithAltitude(registry, target)
	                                                                      : PlaceOfPoint(*at);
	return StartCreatureCast(creature, type, target, place, magnitude);
}

bool MagicSystem::CastByCreatureAt(entt::entity creature, MagicType type, glm::vec3 place)
{
	ReleaseCreatureCast(creature);
	auto& registry = Entities();
	if (!registry.AllOf<Creature>(creature) || static_cast<size_t>(type) >= magic::k_MagicTypeCount || type == MagicType::None)
	{
		return false;
	}
	const auto seed = magic::FindFirstSpellSeedForMagicType(Info(), type);
	const float magnitude =
	    creature_spell_casting::CastMagnitudeAtPlace(seed == SpellSeedType::Fire, object_measures::Height(registry, creature));
	return StartCreatureCast(creature, type, std::nullopt, PlaceOfPoint(place), magnitude);
}

bool MagicSystem::StartCreatureCast(entt::entity creature, MagicType type, std::optional<entt::entity> target,
                                    const map_coords::MapCoords& place, float magnitude)
{
	const auto at = _world.PointOf(place);
	auto& registry = Entities();
	const auto& info = Info();
	const auto& body = registry.Get<const Creature>(creature);
	const auto& effect = magic::GetMagicEffectInfo(info, type);
	const magic::SpellCastData cast {
	    .magnitude = magnitude,
	    .chants = effect.initialChants,
	    .duration = magic::GetTimerWhenCreatureCasting(info, type),
	    .maxObjectsToCreate = -1,
	};
	const auto paying = CasterBodyOf(creature);
	if (!paying.has_value() || !creature_spell_casting::CanCast(paying->first, paying->second, effect.costToCreate))
	{
		return false;
	}
	if (target.has_value() ? !CanCreatureCastAt(type, *target) : !magic::spells::CanCastAtPointItself(*this, type, at))
	{
		return false;
	}
	const SpellCaster caster {.kind = SpellCaster::Kind::Creature, .player = body.owner, .entity = creature};
	// Its effect flows from between its hands towards what it is cast at. It casts food, wood and water from a point
	// above what it is cast at, which stays there. Above a place, this point has the least height.
	const float aboveHeight = target.has_value() ? object_measures::Height(registry, *target) : 0.0f;
	const auto hands = magic::IsCreatureCastFromAbove(info, type)
	                       ? at + glm::vec3(0.0f, std::max(k_CastFromAboveLeast, k_CastFromAboveShare * aboveHeight), 0.0f)
	                       : HandsOf(creature);
	const particles::ProcessInfo process {
	    .handPosition = hands,
	    .cameraForward = particles::CastHeading(hands, at),
	};
	const auto seed = magic::FindFirstSpellSeedForMagicType(info, type);
	const bool onObject = target.has_value() && seed.has_value() && magic::GetSpellSeedInfo(info, *seed).castOnObject != 0;
	const auto spell = onObject ? CastOn(type, caster, *target, cast, process) : Cast(type, caster, place, cast, process);
	// Past its body's and the miracle's checks, the cast counts as made even when no miracle comes of it
	if (spell == entt::null)
	{
		return true;
	}
	_creatureCasts.insert_or_assign(creature, spell);
	// Casting from above, a beam reaches from each of its hands to the point it casts from
	if (magic::IsCreatureCastFromAbove(info, type) && Locator::effectRequestSystem::has_value())
	{
		if (const auto bones = HandBonesOf(creature))
		{
			auto& effects = Locator::effectRequestSystem::value();
			const float size = k_BeamSizeLeast + (k_BeamSizePerSize * ShownSize(body));
			const auto beamFrom = [&](glm::vec3 bone) {
				return effects.Start(
				    {.type = SpotVisualType::MagicBeam, .position = bone, .magnitude = size, .owner = creature});
			};
			auto& beams = _creatureBeams[creature];
			beams = {beamFrom(bones->first), beamFrom(bones->second)};
			for (const auto beam : beams)
			{
				effects.AddTargetPosition(beam, hands);
			}
		}
	}
	// The creature pays for making it with its body
	if (auto* made = FindSpell(spell))
	{
		if (auto* payer = CasterOf(*made))
		{
			payer->MaintainSpell(effect.costToCreate);
		}
	}
	return true;
}

void MagicSystem::EndCreatureCastBeams(entt::entity creature)
{
	const auto beams = _creatureBeams.find(creature);
	if (beams == _creatureBeams.end())
	{
		return;
	}
	if (Locator::effectRequestSystem::has_value())
	{
		for (const auto beam : beams->second)
		{
			Locator::effectRequestSystem::value().Stop(beam);
		}
	}
	_creatureBeams.erase(beams);
	if (const auto logger = spdlog::get("game"))
	{
		SPDLOG_LOGGER_INFO(logger, "The cast beams from the hands of creature {} stop", entt::to_integral(creature));
	}
}

void MagicSystem::ReleaseCreatureCast(entt::entity creature)
{
	// Its beams from casting from above go
	EndCreatureCastBeams(creature);
	const auto found = _creatureCasts.find(creature);
	if (found == _creatureCasts.end())
	{
		return;
	}
	// One that lasts only while held stops; any other runs its time
	if (const auto* spell = FindSpell(found->second); spell != nullptr && spell->duration < 0.0f)
	{
		CloseDown(found->second);
	}
	_creatureCasts.erase(found);
}

entt::entity MagicSystem::ImportCreatureCast(entt::entity creature, const magic::ImportedCast& cast)
{
	const auto* body = Entities().TryGet<const Creature>(creature);
	if (body == nullptr || !Locator::infoConstants::has_value() || static_cast<size_t>(cast.type) >= magic::k_MagicTypeCount ||
	    cast.type == MagicType::None)
	{
		return entt::null;
	}
	ReleaseCreatureCast(creature);
	const SpellCaster caster {.kind = SpellCaster::Kind::Creature, .player = body->owner, .entity = creature};
	const auto spell = MakeImportedCast(caster, cast);
	if (spell == entt::null)
	{
		return entt::null;
	}
	_creatureCasts.insert_or_assign(creature, spell);
	if (cast.closedDown)
	{
		CloseDown(spell);
	}
	return spell;
}

entt::entity MagicSystem::ImportCast(const SpellCaster& caster, const magic::ImportedCast& cast)
{
	const auto spell = MakeImportedCast(caster, cast);
	if (spell != entt::null && cast.closedDown)
	{
		CloseDown(spell);
	}
	return spell;
}

entt::entity MagicSystem::MakeImportedCast(const SpellCaster& caster, const magic::ImportedCast& cast)
{
	auto& registry = Entities();
	if (!Locator::infoConstants::has_value() || static_cast<size_t>(cast.type) >= magic::k_MagicTypeCount ||
	    cast.type == MagicType::None)
	{
		return entt::null;
	}
	// The forest, the shields and the teleport keep their state in the things that they made: the trees, the domes and
	// the stones. The import of the world makes these things for the spell after the spell.
	const auto kind = magic::ClassOf(cast.type);
	const magic::SpellCastData data {
	    .magnitude = cast.magnitude,
	    .chants = cast.initialChants,
	    .duration = cast.duration,
	    .maxObjectsToCreate = -1,
	};
	// The spell keeps the map position where it was cast. A file without it gives the map position where the spell acts
	// now.
	const auto place = cast.castPlace.value_or(cast.place);
	// The effect continues from its state in the file. When that state does not agree with the particle file, the
	// effect starts from its start.
	auto spell = MakeSpell(cast.type, caster, place, data, cast.processInfo, cast.effect);
	const bool restored = spell != entt::null && cast.effect != nullptr;
	if (spell == entt::null && cast.effect != nullptr)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"),
		                   "The saved effect of a spell of magic type {} does not start, so it starts again",
		                   static_cast<int>(cast.type));
		spell = MakeSpell(cast.type, caster, place, data, cast.processInfo);
	}
	if (spell == entt::null)
	{
		return entt::null;
	}
	// The miracle continues with the prayer power, the age and the size of the file. The size of the file is the size
	// after the limits of the cast.
	auto& made = registry.Get<Spell>(spell);
	made.place = cast.place;
	made.originalCastPlace = cast.originalCastPlace.value_or(place);
	made.chants.chants = cast.chants;
	made.chants.initialChants = cast.initialChants;
	made.age = cast.age;
	made.magnitude = cast.magnitude;
	// A spell of the hand of a person follows that hand, such as a flock that still makes its animals
	made.fromLocalHand = cast.fromHand;
	made.humanCasting = cast.humanCasting.value_or(made.humanCasting);
	made.resourceFirstDone = cast.firstDone.value_or(false);
	made.lastRipple = cast.lastRipple.value_or(0.0f);
	// A creature spell acts on its creature. The saved effect knows the creature already.
	if (kind == magic::SpellClass::Creature && registry.Valid(cast.target))
	{
		made.target = cast.target;
		if (!restored)
		{
			AddEffectTarget(made, cast.target);
		}
	}
	// A forest knows if it planted its trees and how many trees it can make
	if (kind == magic::SpellClass::Forest && cast.forest.has_value())
	{
		made.forestPlanted = cast.forest->planted;
		made.maxObjectsToCreate = cast.forest->mostTrees;
	}
	// A new effect of the heal takes the people near the spell, as at the cast
	if (kind == magic::SpellClass::Heal && !restored)
	{
		static_cast<void>(magic::spells::Start(*this, made));
	}
	// A flock continues with its flock, its animals and its sweep
	if ((kind == magic::SpellClass::FlockFlying || kind == magic::SpellClass::FlockGround) && cast.flock.has_value())
	{
		_flocks.Resume(*this, made,
		               FlockSpell {.flock = cast.flock->flock,
		                           .created = cast.flock->created,
		                           .emitted = cast.flock->emitted,
		                           .lastSpawn = cast.flock->lastSpawn,
		                           .lastSpawnHeight = cast.flock->lastSpawnHeight,
		                           .closedDown = cast.closedDown});
	}
	return spell;
}

void MagicSystem::ImportCreatureCastBeams(entt::entity creature, const std::array<uint32_t, 2>& beams)
{
	if (!Locator::effectRequestSystem::has_value() || !Entities().Valid(creature))
	{
		return;
	}
	const auto& effects = Locator::effectRequestSystem::value();
	if (std::ranges::none_of(beams, [&effects](uint32_t beam) { return effects.IsRunning(beam); }))
	{
		return;
	}
	_creatureBeams.insert_or_assign(creature, beams);
}

entt::entity MagicSystem::CreatureCastOf(entt::entity creature) const
{
	const auto found = _creatureCasts.find(creature);
	return found != _creatureCasts.end() ? found->second : entt::null;
}

void MagicSystem::UpdateCreatureCast(Spell& spell)
{
	auto& registry = Entities();
	if (!registry.Valid(spell.caster.entity) || magic::IsCreatureCastFromAbove(Info(), spell.magicType))
	{
		return;
	}
	spell.processInfo.handPosition = HandsOf(spell.caster.entity);
	spell.processInfo.cameraForward = particles::CastHeading(spell.processInfo.handPosition, _world.PointOf(spell.castPlace));
}

void MagicSystem::KeepCreatureBeams()
{
	if (!Locator::effectRequestSystem::has_value())
	{
		_creatureBeams.clear();
		return;
	}
	auto& effects = Locator::effectRequestSystem::value();
	// Each turn the beams are moved back onto the creature's hands, until they have gone
	std::erase_if(_creatureBeams, [&](auto& entry) {
		const bool running = std::ranges::any_of(entry.second, [&](auto beam) { return effects.IsRunning(beam); });
		if (!Entities().Valid(entry.first) || !running)
		{
			return true;
		}
		const auto bones = HandBonesOf(entry.first);
		if (!bones.has_value())
		{
			return false;
		}
		effects.Move(entry.second[0], bones->first);
		effects.Move(entry.second[1], bones->second);
		return false;
	});
}
