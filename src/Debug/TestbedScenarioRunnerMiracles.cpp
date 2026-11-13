/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed runner: the miracles, the dispensers, the rewards, the vortices and the particles of a scenario.

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <system_error>

#include <fmt/format.h>
#include <glm/geometric.hpp>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "3D/TempleInteriorInterface.h"
#include "Camera/Camera.h"
#include "Common/PortableMath.h"
#include "Creature/CreatureMindTables.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/Vortex.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/ComputerPlayerStart.h"
#include "ECS/Registry.h"
#include "ECS/ScriptFlocks.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/CreatureCaveSystemInterface.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/GestureEventsInterface.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/MagicShieldSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/ParticleRuleSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/RewardSystemInterface.h"
#include "ECS/Systems/ScriptHighlightSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/Systems/VortexSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "LHScriptX/Script.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Magic/ScriptCast.h"
#include "Magic/SpellRules.h"
#include "Sound/GameMusic.h"
#include "TestbedDispenserGrid.h"
#include "TestbedScenarioRunner.h"
#include "TestbedScenarioRunnerParts.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;
using namespace openblack::testbed_scenarios::runner_parts;
using openblack::ecs::components::Creature;
using openblack::ecs::components::Transform;

namespace
{
/// A miracle cast by hand: the button goes down this long after the seed is in the hand, so that the hand is over its
/// point first, and a thrown one is let go this long after
constexpr float k_HandPressWait = 0.2f;
constexpr float k_HandThrowSeconds = 0.4f;
} // namespace

namespace openblack::testbed_scenarios::runner_parts
{
std::string_view HandResultName(ecs::systems::MagicSystemInterface::HandResult result)
{
	using HandResult = ecs::systems::MagicSystemInterface::HandResult;
	switch (result)
	{
	case HandResult::None:
		return "nothing";
	case HandResult::TookMiracle:
		return "took a miracle";
	case HandResult::Readied:
		return "armed";
	case HandResult::Cast:
		return "cast";
	case HandResult::CastHeld:
		return "cast, held";
	case HandResult::NotReady:
		return "not ready";
	case HandResult::CantCastThere:
		return "can't cast there";
	case HandResult::Released:
		return "let go";
	case HandResult::Discarded:
		return "dropped";
	case HandResult::NoCircle:
		return "no circle drawn";
	case HandResult::PoweredUp:
		return "powered up";
	case HandResult::Stored:
		return "stored";
	}
	return "";
}
} // namespace openblack::testbed_scenarios::runner_parts

void Runner::UpdateThrows()
{
	if (!Locator::dynamicsSystem::has_value() || !Locator::entitiesRegistry::has_value() ||
	    !Locator::terrainSystem::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	auto& dynamics = Locator::dynamicsSystem::value();
	for (size_t i = 0; i < _throwsDue.size() && i < _scenario->throws.size(); ++i)
	{
		auto& due = _throwsDue.at(i);
		if (!due.has_value() || _seconds < *due)
		{
			continue;
		}
		const auto& setup = _scenario->throws.at(i);
		const auto& pool = setup.reward ? _rewards : _objects;
		const auto object = setup.object < pool.size() ? pool.at(setup.object) : entt::null;
		// A chest that is not given yet waits
		if (setup.reward && object == entt::null)
		{
			continue;
		}
		// What broke or went is thrown no more
		if (!registry.Valid(object) || !registry.AllOf<ecs::components::Transform>(object))
		{
			due.reset();
			continue;
		}
		// Still flying from the last throw: it is thrown once it lands
		if (registry.AllOf<ecs::components::InPhysics>(object))
		{
			continue;
		}
		auto& transform = registry.Get<ecs::components::Transform>(object);
		// The point can be from where a creature of the scenario stands now
		auto origin = _middle;
		if (setup.fromCreature.has_value() && *setup.fromCreature < _creatures.size() &&
		    registry.Valid(_creatures.at(*setup.fromCreature)))
		{
			if (const auto* at = registry.TryGet<const ecs::components::Transform>(_creatures.at(*setup.fromCreature)))
			{
				origin = {at->position.x, at->position.z};
			}
		}
		const auto point =
		    setup.from.has_value() ? MapPoint(origin, *setup.from) : glm::vec2(transform.position.x, transform.position.z);
		transform.position = {point.x, Locator::terrainSystem::value().GetHeightAt(point) + setup.height, point.y};
		registry.SetDirty();
		const auto result = dynamics.LetGoFromHand(object, {.velocity = setup.velocity, .player = PlayerNames::PLAYER_ONE});
		if (!result.accepted)
		{
			Log(fmt::format("{:.1f}s: object {} wasn't thrown", _seconds, setup.object));
		}
		due = setup.repeatSeconds.has_value() ? std::optional(*due + *setup.repeatSeconds) : std::nullopt;
	}
}

void Runner::PlaceDispensers(const Scenario& scenario)
{
	if (!KeepsDispenserGrid(scenario))
	{
		testbed_dispensers::RemoveGrid();
	}
	if (!Locator::magicSystem::has_value() || !Locator::terrainSystem::has_value())
	{
		return;
	}
	auto& magic = Locator::magicSystem::value();
	for (const auto& dispenser : scenario.dispensers)
	{
		const auto point = MapPoint(_middle, dispenser.offset);
		const float ground = Locator::terrainSystem::value().GetHeightAt(point);
		if (dispenser.bubbleHeight.has_value())
		{
			_globes.push_back(magic.CreateOneOffSeedFor({point.x, ground + *dispenser.bubbleHeight, point.y}, dispenser.type));
		}
		else if (dispenser.reward)
		{
			_dispensers.push_back(PlaceRewardDispenser(dispenser, {point.x, ground, point.y}));
		}
		else
		{
			const auto made = magic.CreateDispenser({point.x, ground, point.y}, dispenser.type, 0.0f);
			if (dispenser.charged && made != entt::null)
			{
				magic.ChargeDispenser(made);
			}
			_dispensers.push_back(made);
		}
	}
}

entt::entity Runner::PlaceRewardDispenser(const DispenserSetup& setup, glm::vec3 position)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& magic = Locator::magicSystem::value();
	entt::entity town = entt::null;
	if (!setup.noTown)
	{
		const auto id = ScenarioTown(_middle);
		town = registry.Context().towns.at(id);
	}
	// As the reward script does: the dispenser, its miracle with the building's own period, then turned on
	const auto made = magic.CreateScriptDispenser(position, AbodeInfo::NorseSpellDispenser, 0.0f, 1.0f, town);
	if (made == entt::null)
	{
		return entt::null;
	}
	if (setup.noTown)
	{
		// The script gives a dispenser the player's first town when it names none, so take it from its town
		auto& dispenser = registry.Get<ecs::components::SpellDispenser>(made);
		if (registry.Valid(dispenser.town))
		{
			std::erase(registry.Get<ecs::components::Town>(dispenser.town).abodes, made);
			Locator::townSystem::value().StructuresChanged(dispenser.town);
		}
		dispenser.town = entt::null;
	}
	const auto& info = Locator::infoConstants::value().abode.at(static_cast<size_t>(AbodeInfo::NorseSpellDispenser));
	magic.SetDispenserMagic(made, setup.type, static_cast<uint32_t>(info.timeEachMobileObjectTakesToProduce));
	Log(fmt::format("dispenser: period {} turns, wood value {}, builders {}, desire to be repaired {}",
	                info.timeEachMobileObjectTakesToProduce, info.woodValue, info.maxVillagerNeededToBuild,
	                info.desireToBeRepaired));
	magic.SetDispenserActive(made, true);
	if (setup.globeTaken)
	{
		magic.Remove(registry.Get<const ecs::components::SpellDispenser>(made).orb);
	}
	if (setup.life < 1.0f)
	{
		ecs::world_objects::ReduceLife(made, 1.0f - setup.life, std::nullopt);
	}
	return made;
}

void Runner::UpdateMiracles(float seconds)
{
	if (!Locator::magicSystem::has_value())
	{
		return;
	}
	auto& magic = Locator::magicSystem::value();
	for (size_t i = 0; i < _miracles.size(); ++i)
	{
		auto& miracle = _miracles.at(i);
		const auto& setup = _scenario->miracles.at(i);
		if (setup.byHand)
		{
			// A cast on a creature takes the creature under the pointer, so the pointer stays on the creature's body
			if (setup.target == MiracleCast::Target::Creature && (miracle.pressAt.has_value() || miracle.releaseAt.has_value()))
			{
				if (const auto creature = CreatureAt(setup.creature))
				{
					PointAtBody(*creature);
				}
			}
			// The button goes down once the hand is over the point, and comes up after the hold, the hand moving meanwhile
			if (miracle.pressAt.has_value() && _seconds >= *miracle.pressAt)
			{
				magic.PressAction();
				Log(fmt::format("Pressed: {}", HandResultName(magic.GetLastHandResult())));
				miracle.pressAt.reset();
				miracle.releaseAt = _seconds + setup.holdSeconds.value_or(k_HandThrowSeconds);
			}
			if (miracle.releaseAt.has_value())
			{
				miracle.hand += setup.throwVelocity * seconds;
				magic.DriveHand(HandFrameFor(i, miracle.hand));
				if (_seconds >= *miracle.releaseAt)
				{
					magic.ReleaseAction();
					Log(fmt::format("Let go: {}", HandResultName(magic.GetLastHandResult())));
					miracle.releaseAt.reset();
					if (setup.sweepSeconds.has_value())
					{
						miracle.sweepUntil = _seconds + *setup.sweepSeconds;
					}
					else
					{
						magic.DriveHand(std::nullopt);
					}
					// The hand drops a seed that it keeps, for the next go. The drop is a shake of the hand, which goes as a
					// command after the cast. Thus the turn does the cast first, and each computer of a network game
					// drops the seed at the same turn.
					if (magic.IsHandBusy() && Locator::gestureEvents::has_value())
					{
						Locator::gestureEvents::value().Inject({.kind = ecs::systems::GestureEvent::Kind::Shake,
						                                        .gesture = GestureType::None,
						                                        .centre = miracle.hand,
						                                        .radius = 0.0f,
						                                        .powerUpLevel = -1});
					}
				}
			}
			if (miracle.sweepUntil.has_value())
			{
				// The hand sweeps on over the land as the flock pours out of it
				miracle.hand += setup.throwVelocity * seconds;
				magic.DriveHand(HandFrameFor(i, miracle.hand));
				if (_seconds >= *miracle.sweepUntil)
				{
					miracle.sweepUntil.reset();
					magic.DriveHand(std::nullopt);
				}
			}
			if (miracle.nextAt.has_value() && _seconds >= *miracle.nextAt)
			{
				StartHandCast(i);
				miracle.nextAt =
				    setup.repeatSeconds.has_value() ? std::optional(*miracle.nextAt + *setup.repeatSeconds) : std::nullopt;
			}
			continue;
		}
		if (miracle.letGoAt.has_value() && _seconds >= *miracle.letGoAt)
		{
			Locator::magicSystem::value().CloseDown(miracle.spell);
			miracle.letGoAt.reset();
		}
		if (miracle.breakAt.has_value() && _seconds >= *miracle.breakAt)
		{
			miracle.breakAt.reset();
			if (Locator::magicShieldSystem::has_value() && Locator::magicShieldSystem::value().HasObject(miracle.spell))
			{
				Locator::magicShieldSystem::value().ShieldStruck(miracle.spell, true);
				Locator::magicSystem::value().CloseDown(miracle.spell);
				Log("Shield broken");
			}
		}
		if (!miracle.nextAt.has_value() || _seconds < *miracle.nextAt)
		{
			continue;
		}
		miracle.spell = CastMiracle(i);
		Log(fmt::format("Cast {}{}", static_cast<int>(setup.type), miracle.spell == entt::null ? ", which failed" : ""));
		if (setup.holdSeconds.has_value() && miracle.spell != entt::null)
		{
			miracle.letGoAt = _seconds + *setup.holdSeconds;
		}
		if (setup.breakSeconds.has_value() && miracle.spell != entt::null)
		{
			miracle.breakAt = _seconds + *setup.breakSeconds;
		}
		miracle.nextAt = setup.repeatSeconds.has_value() ? std::optional(*miracle.nextAt + *setup.repeatSeconds) : std::nullopt;
	}
}

entt::entity Runner::CastMiracle(size_t index)
{
	if (!Locator::terrainSystem::has_value() || !Locator::infoConstants::has_value())
	{
		return entt::null;
	}
	const auto& setup = _scenario->miracles.at(index);
	const auto& info = Locator::infoConstants::value();
	const auto& land = Locator::terrainSystem::value();
	auto& magic = Locator::magicSystem::value();
	// What a seed of it in the hand would cast: a sizing miracle at its usual size
	const auto seed = magic::FindFirstSpellSeedForMagicType(info, setup.type).value_or(SpellSeedType::None);
	float size = 1.0f;
	if (const auto* radius = magic::GetMagicInfoAs<GMagicRadiusSpellInfo>(info, setup.type))
	{
		size = radius->radiusForNormalCost;
	}
	const auto cast = magic::SeedCastData(info, setup.type, seed, 1.0f, size);
	std::optional<entt::entity> target;
	auto point2 = MapPoint(_middle, setup.point);
	glm::vec3 point {point2.x, land.GetHeightAt(point2), point2.y};
	if (setup.target == MiracleCast::Target::Creature)
	{
		target = CreatureAt(setup.creature);
		if (!target.has_value())
		{
			return entt::null;
		}
		point = Locator::entitiesRegistry::value().Get<Transform>(*target).position;
	}
	const auto hand2 = MapPoint(_middle, setup.handOffset);
	const glm::vec3 hand {hand2.x, land.GetHeightAt(hand2) + setup.handHeight, hand2.y};
	// As a script casts it: the neutral player's, from the hand's place, on the creature when its seed is cast on things
	if (setup.script.has_value())
	{
		const auto scripted = magic::MakeScriptCast(magic::GetMagicEffectInfo(info, setup.type).initialChants, point, hand,
		                                            setup.script->radius, setup.script->seconds, setup.script->curl);
		const bool onObject =
		    target.has_value() && seed != SpellSeedType::None && magic::GetSpellSeedInfo(info, seed).castOnObject != 0;
		return onObject ? magic.CastOnObject(setup.type, PlayerNames::NEUTRAL, *target, scripted.cast, scripted.info)
		                : magic.CastAtPoint(setup.type, PlayerNames::NEUTRAL, scripted.point, scripted.cast, scripted.info);
	}
	const auto toward = point - hand;
	const particles::ProcessInfo process {
	    .handPosition = hand,
	    .cameraForward = glm::length(toward) > 0.0f ? glm::normalize(toward) : glm::vec3(0.0f, 0.0f, 1.0f),
	    .direction = setup.throwVelocity,
	};
	return target.has_value() ? magic.CastOnObject(setup.type, setup.player, *target, cast, process)
	                          : magic.CastAtPoint(setup.type, setup.player, point, cast, process);
}

std::optional<ecs::systems::MagicSystemInterface::HandFrame> Runner::HandFrameFor(size_t index, glm::vec3 hand) const
{
	const auto& setup = _scenario->miracles.at(index);
	const auto& land = Locator::terrainSystem::value();
	const auto point2 = MapPoint(_middle, setup.point);
	glm::vec3 point {point2.x, land.GetHeightAt(point2), point2.y};
	glm::vec3 aim = point;
	if (setup.target == MiracleCast::Target::Creature)
	{
		const auto creature = CreatureAt(setup.creature);
		if (!creature.has_value())
		{
			return std::nullopt;
		}
		point = Locator::entitiesRegistry::value().Get<Transform>(*creature).position;
		// At the middle of its body
		aim = point + glm::vec3(0.0f, k_HandAimHeight, 0.0f);
	}
	// Looking down at it from over the hand
	const auto eye = hand + glm::vec3(0.0f, k_HandEyeHeight, 0.0f);
	const auto toward = aim - eye;
	const auto direction = glm::length(toward) > 0.0f ? glm::normalize(toward) : glm::vec3(0.0f, -1.0f, 0.0f);
	return ecs::systems::MagicSystemInterface::HandFrame {
	    .handPosition = hand,
	    .point = point,
	    .rayOrigin = eye,
	    .rayDirection = direction,
	    .cameraForward = direction,
	    .overWorld = true,
	};
}

void Runner::StartHandCast(size_t index)
{
	if (!Locator::terrainSystem::has_value() || !Locator::infoConstants::has_value())
	{
		return;
	}
	const auto& setup = _scenario->miracles.at(index);
	auto& miracle = _miracles.at(index);
	const auto& info = Locator::infoConstants::value();
	auto& magic = Locator::magicSystem::value();
	// The first player's hand gets the seed, so it drops the seed that it holds. Each computer does this before the
	// same turn.
	if (magic.IsHandBusyOf(PlayerNames::PLAYER_ONE))
	{
		magic.DiscardHeldSeedOf(PlayerNames::PLAYER_ONE);
	}
	const auto seed = magic::FindFirstSpellSeedForMagicType(info, setup.type);
	if (!seed.has_value())
	{
		return;
	}
	if (setup.casterAlignment.has_value() && Locator::alignmentSystem::has_value())
	{
		Locator::alignmentSystem::value().SetPlayerAlignment(PlayerNames::PLAYER_ONE, *setup.casterAlignment);
	}
	const auto level = magic::GetPowerUpGesture(magic::GetSpellSeedInfo(info, *seed), setup.type).level;
	auto held = entt::entity {entt::null};
	if (setup.fromWorship && setup.fromSite)
	{
		// The first worship site of the player gives the seed
		auto site = entt::entity {entt::null};
		Locator::entitiesRegistry::value().Each<const ecs::components::WorshipSite>(
		    [&site](entt::entity entity, const ecs::components::WorshipSite& data) {
			    if (site == entt::null && data.player == PlayerNames::PLAYER_ONE)
			    {
				    site = entity;
			    }
		    });
		held = magic.SummonSeedAtSite(site, *seed, level);
	}
	else
	{
		held = setup.fromWorship ? magic.SummonSeed(PlayerNames::PLAYER_ONE, *seed, level)
		                         : magic.GiveSeedToHand(PlayerNames::PLAYER_ONE, *seed, level, 1.0f);
	}
	if (held == entt::null)
	{
		Log("The hand couldn't take the seed");
		return;
	}
	const auto& land = Locator::terrainSystem::value();
	const auto hand2 = MapPoint(_middle, setup.handOffset);
	miracle.hand = {hand2.x, land.GetHeightAt(hand2) + setup.handHeight, hand2.y};
	magic.DriveHand(HandFrameFor(index, miracle.hand));
	if (setup.circleRadius.has_value() && Locator::gestureEvents::has_value())
	{
		const auto point2 = MapPoint(_middle, setup.point);
		Locator::gestureEvents::value().Inject({.kind = ecs::systems::GestureEvent::Kind::Circle,
		                                        .gesture = GestureType::Circle,
		                                        .centre = {point2.x, land.GetHeightAt(point2), point2.y},
		                                        .radius = *setup.circleRadius,
		                                        .powerUpLevel = -1});
	}
	// A seed from worship waits until it is ready
	const float wait = setup.holdBeforePress.has_value() ? *setup.holdBeforePress
	                   : setup.fromWorship               ? info.spellSystem.delayBeforeSeedActive + k_HandPressWait
	                                                     : k_HandPressWait;
	miracle.pressAt = _seconds + wait;
	Log(fmt::format("Seed of {} in the hand{}", static_cast<int>(setup.type), setup.fromWorship ? ", from worship" : ""));
}

uint32_t Runner::StartParticle(size_t index) const
{
	if (!Locator::particleRuleSystem::has_value() || !Locator::terrainSystem::has_value())
	{
		return ecs::systems::ParticleRuleSystemInterface::k_NoEffect;
	}
	const auto& particle = _scenario->particles.at(index);
	const auto point = MapPoint(_middle, particle.offset);
	const glm::vec3 position {point.x, Locator::terrainSystem::value().GetHeightAt(point) + particle.height, point.y};
	auto& particles = Locator::particleRuleSystem::value();
	const auto effect = particle.file.empty() ? particles.Start(particle.type, position, particle.magnitude)
	                                          : particles.Start(particle.file, position, particle.magnitude);
	particles.SetPlayer(effect, particle.player);
	particles.SetDrawPath(effect, particle.path);
	if (particle.targetsCreatures)
	{
		for (const auto creature : _creatures)
		{
			particles.AddTarget(effect, creature);
		}
	}
	return effect;
}

void Runner::WriteCrossingFile(std::span<const std::string_view> lines)
{
	// Where the profile keeps it, as a vortex out of the last land writes it
	if (!Locator::playerProfileSystem::has_value())
	{
		return;
	}
	const auto folder = Locator::playerProfileSystem::value().SavedGamesFolder();
	if (!folder.has_value())
	{
		return;
	}
	std::error_code error;
	std::filesystem::create_directories(*folder, error);
	std::ofstream file(*folder / "vortex.txt", std::ios::out | std::ios::trunc | std::ios::binary);
	for (const auto line : lines)
	{
		file << line << '\n';
	}
	Log(fmt::format("Wrote {} lines to the crossing file in {}", lines.size(), folder->string()));
}

void Runner::UpdateVortices()
{
	if (!Locator::vortexSystem::has_value() || !Locator::terrainSystem::has_value())
	{
		return;
	}
	auto& vortices = Locator::vortexSystem::value();
	for (size_t i = 0; i < _vortices.size(); ++i)
	{
		const auto& setup = _scenario->vortices.at(i);
		auto& running = _vortices.at(i);
		if (running.vortex == entt::null && _seconds >= setup.delaySeconds)
		{
			const auto point = MapPoint(_middle, setup.offset);
			if (!setup.crossingLines.empty())
			{
				WriteCrossingFile(setup.crossingLines);
			}
			running.vortex = vortices.Create({point.x, 0.0f, point.y}, setup.type, 0.0f);
			if (!setup.crossingLines.empty() && running.vortex != entt::null)
			{
				// The scenario also gives its crossing lines to the vortex. Thus a world with no profile, such as the
				// server, throws out the same things. These lines are the same as the lines that the game read from the
				// file.
				auto& lines = Locator::entitiesRegistry::value().Get<ecs::components::Vortex>(running.vortex).throwingOut.lines;
				lines.assign(setup.crossingLines.begin(), setup.crossingLines.end());
			}
			if (setup.crowdForSeconds.has_value() && Locator::scriptObjects::has_value())
			{
				// A script makes the crowd at the vortex and keeps it in a variable, as the Land 2 arrival does
				auto& scripts = Locator::scriptObjects::value();
				running.crowd = ecs::script_flocks::Create(Locator::entitiesRegistry::value(), map_coords::FromMetres(point));
				scripts.Register(running.crowd, true);
				scripts.AddReference(running.crowd);
			}
			if (setup.gatheringOffset.has_value() || running.crowd != entt::null)
			{
				const auto gathering = MapPoint(_middle, setup.gatheringOffset.value_or(setup.offset));
				vortices.SetArrival(running.vortex, {.town = entt::null,
				                                     .gatheringPlace = {gathering.x, 0.0f, gathering.y},
				                                     .flockDistance = setup.flockDistance,
				                                     .flockRadius = setup.flockRadius,
				                                     .flock = running.crowd});
			}
			Log(fmt::format("Vortex {} opened at {:.0f}, {:.0f}", i, point.x, point.y));
		}
		if (running.crowd != entt::null && !running.crowdBrokenUp && setup.crowdForSeconds.has_value() &&
		    _seconds >= setup.delaySeconds + *setup.crowdForSeconds && Locator::scriptObjects::has_value())
		{
			// The script breaks up the crowd and keeps the empty flock; its newcomers go back to the town's life
			running.crowdBrokenUp = Locator::scriptObjects::value().Disband(running.crowd);
			Log(fmt::format("Vortex {} crowd broken up", i));
		}
		if (running.vortex != entt::null && !running.fading && setup.fadeOutAfterSeconds.has_value() &&
		    _seconds >= setup.delaySeconds + *setup.fadeOutAfterSeconds)
		{
			running.fading = vortices.StartFadeOut(running.vortex);
			Log(fmt::format("Vortex {} fading out", i));
		}
	}
}

void Runner::UpdateRewards()
{
	if (!Locator::rewardSystem::has_value() || !Locator::terrainSystem::has_value())
	{
		return;
	}
	const auto player =
	    Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
	for (size_t i = 0; i < _rewards.size(); ++i)
	{
		const auto& setup = _scenario->rewards.at(i);
		if (_rewards.at(i) != entt::null || _seconds < setup.delaySeconds)
		{
			continue;
		}
		const auto point = MapPoint(_middle, setup.offset);
		const glm::vec3 at {point.x, Locator::terrainSystem::value().GetHeightAt(point), point.y};
		const auto town =
		    setup.inTown ? Locator::entitiesRegistry::value().Context().towns.at(ScenarioTown(_middle)) : entt::null;
		_rewards.at(i) = Locator::rewardSystem::value().Create(at, setup.type, player, town, setup.fromSky);
		if (setup.opened && !setup.fromSky)
		{
			Locator::rewardSystem::value().Tap(_rewards.at(i), player);
		}
		Log(fmt::format("Reward chest {} (kind {}) given at {:.0f}, {:.0f}", i, static_cast<int>(setup.type), point.x,
		                point.y));
	}
}

void Runner::UpdateParticles(float seconds)
{
	if (!Locator::particleRuleSystem::has_value())
	{
		return;
	}
	auto& particles = Locator::particleRuleSystem::value();
	for (size_t i = 0; i < _particles.size(); ++i)
	{
		auto& running = _particles.at(i);
		const auto restart = _scenario->particles.at(i).restartSeconds;
		running.seconds += seconds;
		// An effect that has ended, or whose time is up, starts again
		if (!particles.IsRunning(running.effect) || (restart > 0.0f && running.seconds >= restart))
		{
			particles.CloseDown(running.effect);
			running = {StartParticle(i), 0.0f};
		}
	}
}

void Runner::LogMiracles()
{
	if (!_scenario->logMiraclesEvery.has_value() || _seconds < _nextMiracleLog || !Locator::terrainSystem::has_value())
	{
		return;
	}
	_nextMiracleLog = _seconds + *_scenario->logMiraclesEvery;
	const auto& land = Locator::terrainSystem::value();
	Locator::entitiesRegistry::value().Each<const ecs::components::Spell>(
	    [&](entt::entity, const ecs::components::Spell& spell) {
		    const glm::vec2 xz(spell.position.x, spell.position.z);
		    Log(fmt::format("{:.1f} s: miracle {} at ({:.2f}, {:.2f}), land {:.2f}", _seconds,
		                    static_cast<int>(spell.magicType), xz.x - _middle.x, xz.y - _middle.y, land.GetHeightAt(xz)));
	    });
}
