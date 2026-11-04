/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed runner: the commands to a creature: its moves, its actions, its leash, its fights and its mind.

#include <cmath>

#include <algorithm>
#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>

#include <MindFile.h>
#include <fmt/format.h>
#include <glm/gtx/vec_swizzle.hpp>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "3D/SkyDome.h"
#include "3D/TempleInteriorInterface.h"
#include "Camera/Camera.h"
#include "Camera/CameraModel.h"
#include "Commands/PlayerCommands.h"
#include "Common/FileDialog.h"
#include "Common/PortableMath.h"
#include "Creature/CaveCreature.h"
#include "Creature/CreatureActivity.h"
#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLearning.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureScriptPlay.h"
#include "Debug/FightAutopilotInterface.h"
#include "Debug/TownMiracles.h"
#include "ECS/Archetypes/ArenaArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/BuildingDamage.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureCopy.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureFriends.h"
#include "ECS/Components/CreatureHandGlows.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLeashRope.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Fire.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/Sky.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Workshop.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/ComputerPlayerStart.h"
#include "ECS/CreatureReactionNotice.h"
#include "ECS/Dances.h"
#include "ECS/Registry.h"
#include "ECS/ScriptFlocks.h"
#include "ECS/Systems/AbodeKnockSystemInterface.h"
#include "ECS/Systems/AdvisorSystemInterface.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/BoatLaunchSystemInterface.h"
#include "ECS/Systems/CameraBookmarkSystemInterface.h"
#include "ECS/Systems/ChatSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureCaveSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureFizzSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/CreatureWebPageSystemInterface.h"
#include "ECS/Systems/DanceSystemInterface.h"
#include "ECS/Systems/FightTranscriptSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/FootballSystemInterface.h"
#include "ECS/Systems/GameEndSystemInterface.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/GuidanceSystemInterface.h"
#include "ECS/Systems/HandDemoSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HelpMessageSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerDisciples.h"
#include "ECS/Systems/IncomingMessageSystemInterface.h"
#include "ECS/Systems/InspectorSystemInterface.h"
#include "ECS/Systems/LandSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/ParticleRuleSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/ProfileFolderSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/RewardSystemInterface.h"
#include "ECS/Systems/SaveGameSystemInterface.h"
#include "ECS/Systems/ScriptHighlightSystemInterface.h"
#include "ECS/Systems/ScriptMusicSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "ECS/Systems/SoundTagSystemInterface.h"
#include "ECS/Systems/TattooEditorSystemInterface.h"
#include "ECS/Systems/TattooMenuSystemInterface.h"
#include "ECS/Systems/TeleportSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "FileSystem/FileSystemInterface.h"
#include "Input/GameActionMapInterface.h"
#include "LHScriptX/Script.h"
#include "Locator.h"
#include "Magic/TownBelief.h"
#include "Resources/ResourcesInterface.h"
#include "Sound/SoundId.h"
#include "TestbedScenarioRunner.h"
#include "TestbedScenarioRunnerParts.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;
using namespace openblack::testbed_scenarios::runner_parts;
using openblack::ecs::components::Creature;
using openblack::ecs::components::CreatureHandGlows;
using openblack::ecs::components::CreatureMindState;
using openblack::ecs::components::CreatureNeeds;
using openblack::ecs::components::Transform;

namespace
{
/// A creature follows another this far behind, for a creature of size 1
constexpr float k_FollowDistance = 25.0f;

std::string_view MoveResultName(ecs::systems::CreatureLocomotionSystemInterface::MoveResult result)
{
	using MoveResult = ecs::systems::CreatureLocomotionSystemInterface::MoveResult;
	switch (result)
	{
	case MoveResult::InvalidDestination:
		return "nowhere to stand there";
	case MoveResult::Busy:
		return "it can't walk";
	case MoveResult::Started:
	default:
		return "on its way";
	}
}

/// Why the leash was refused, for the readout
std::string RefusedText(const ecs::systems::LeashSystemInterface& leashes)
{
	const auto refused = leashes.LastRefusal();
	return refused.has_value() ? fmt::format("refused: {}", creature_leash::Describe(refused->why)) : "can't";
}
} // namespace

void Runner::PointAtBody(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const ecs::components::Transform>(creature);
	const auto* mesh = registry.TryGet<const ecs::components::Mesh>(creature);
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (transform == nullptr || mesh == nullptr || !meshes.Contains(mesh->id) || !Locator::windowing::has_value())
	{
		return;
	}
	// The middle of the body's bounding box, as it stands now
	const auto model = creature::PlacementMatrix(transform->position, transform->rotation, transform->scale);
	const auto middle = glm::vec3(model * glm::vec4(meshes.Handle(mesh->id)->GetBoundingBox().Center(), 1.0f));
	const auto size = glm::vec2(Locator::windowing::value().GetSize());
	glm::vec3 pixel;
	if (Locator::camera::value().ProjectWorldToScreen(middle, {0.0f, 0.0f, size.x, size.y}, pixel))
	{
		_stage->SetMousePosition(glm::ivec2(glm::round(glm::vec2(pixel))));
	}
}

std::string Runner::GiveObjectCommand(entt::entity creature, const Command& command)
{
	if (!Locator::creatureObjectActionSystem::has_value())
	{
		return "no hands";
	}
	auto& hands = Locator::creatureObjectActionSystem::value();
	const auto& land = Locator::terrainSystem::value();
	const auto point = MapPoint(_middle, command.point);
	const glm::vec3 onLand {point.x, land.GetHeightAt(point), point.y};
	const auto object = command.globe.has_value() ? GlobeAt(*command.globe) : ObjectAt(command.object);
	switch (command.kind)
	{
	case Kind::PickUp:
		return object.has_value() ? std::string(Started(hands.PickUp(creature, *object))) : "it is gone";
	case Kind::PutDown:
		return std::string(Started(hands.PutDown(creature)));
	case Kind::Discard:
		return std::string(Started(hands.Discard(creature)));
	case Kind::EatHeld:
		return std::string(Started(hands.EatHeld(creature)));
	case Kind::Examine:
		return std::string(Started(hands.Keep(creature, creature_object_actions::k_FirstKeepAnimation + command.value)));
	case Kind::ThrowAt:
		// At about the height of a creature's middle
		return std::string(Started(hands.Throw(creature, onLand + glm::vec3(0.0f, CreatureHeight(1.0f) * 0.5f, 0.0f))));
	case Kind::KnockDown:
		return object.has_value() ? std::string(Started(hands.Destroy(creature, *object))) : "it is gone";
	case Kind::PointAt:
		return std::string(Started(hands.PointAt(creature, onLand)));
	default:
		return {};
	}
}

std::string Runner::GiveHandCommand(entt::entity creature, const Command& command)
{
	if (!Locator::creatureHandSystem::has_value())
	{
		return "no hand";
	}
	auto& hand = Locator::creatureHandSystem::value();
	switch (command.kind)
	{
	case Kind::HandStroke:
		return hand.Stroke(creature, static_cast<creature_feedback::BodyPart>(command.bodyPart)) ? "stroked" : "busy";
	case Kind::HandSlap:
		return hand.Slap(creature, command.slapHeight, command.gentle, command.sweepsRight) ? "slapped" : "busy";
	case Kind::HandLock:
	{
		const auto own =
		    Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer() : PlayerNames::PLAYER_ONE;
		hand.DoLock(own, creature);
		return hand.IsLocked(creature) ? "held" : "busy";
	}
	case Kind::HandLetGo:
	{
		if (hand.GetCreature() == creature)
		{
			const auto sum = hand.GetFeedbackSum();
			hand.Release();
			return fmt::format("{:+.2f}", sum);
		}
		// A hold that the scenario took with no touch of the body ends as the turn ends it
		const auto own =
		    Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer() : PlayerNames::PLAYER_ONE;
		if (hand.GetLockedCreature(own) == creature)
		{
			hand.DoUnlock(own, creature);
			return "let go of the hold";
		}
		return "not held";
	}
	default:
		return {};
	}
}

std::string Runner::GiveLeashCommand(entt::entity creature, const Command& command)
{
	if (!Locator::leashSystem::has_value())
	{
		return "no leashes";
	}
	auto& leashes = Locator::leashSystem::value();
	switch (command.kind)
	{
	case Kind::PutOnLeash:
		// It must know the learning leash before any, and the leash itself
		leashes.SetKnown(creature, LeashType::Rope, true);
		leashes.SetKnown(creature, command.leash, true);
		if (leashes.IsLeashed(creature))
		{
			return leashes.ChangeType(creature, command.leash) ? "changed" : "can't";
		}
		return leashes.PutOn(creature, command.leash) ? "on" : RefusedText(leashes);
	case Kind::MakeLeashable:
		return leashes.SetLeashable(creature, true) ? "leashable" : RefusedText(leashes);
	case Kind::HandTapLeash:
		// As the player's Action button tapping it does
		if (!leashes.TapCreature(command.player, creature))
		{
			return RefusedText(leashes);
		}
		return leashes.IsLeashed(creature) ? "on" : "off";
	case Kind::LeashShake:
		// The game shakes a leash off with a scribble drawn with the empty hand
		return DrawGesture(GestureType::Scribble);
	case Kind::LeashKey:
	{
		// As the player pressing the key does, through the same call the controls make
		constexpr std::array k_Keys {creature_leash::LeashKey::Leash, creature_leash::LeashKey::PreviousLeash,
		                             creature_leash::LeashKey::NextLeash};
		const auto before = leashes.TypeOf(creature);
		if (!leashes.PressKey(command.player, k_Keys.at(command.value)))
		{
			return RefusedText(leashes);
		}
		const auto after = leashes.TypeOf(creature);
		return after == LeashType::None ? "off" : before == after ? "same" : creature_leash::Name(after);
	}
	case Kind::TieLeash:
		if (const auto object = ObjectAt(command.object))
		{
			return leashes.HandTie(creature, *object) ? "tied" : "can't";
		}
		return "it is gone";
	case Kind::TieLeashToDesireFlag:
	{
		auto& registry = Locator::entitiesRegistry::value();
		std::optional<entt::entity> found;
		registry.Each<const ecs::components::TownDesireFlag>(
		    [&](entt::entity flag, const ecs::components::TownDesireFlag& shown) {
			    if (!found.has_value() && static_cast<size_t>(shown.desire) == command.value)
			    {
				    found = flag;
			    }
		    });
		if (!found.has_value())
		{
			return "no flag";
		}
		return leashes.HandTie(creature, *found) ? "tied" : "can't";
	}
	case Kind::TieLeashToWorshipSiteFlag:
	{
		auto& registry = Locator::entitiesRegistry::value();
		std::optional<entt::entity> found;
		registry.Each<const ecs::components::WorshipSiteFlag>(
		    [&](entt::entity flag, const ecs::components::WorshipSiteFlag& shown) {
			    if (!found.has_value() && static_cast<size_t>(shown.need) == command.value)
			    {
				    found = flag;
			    }
		    });
		if (!found.has_value())
		{
			return "no flag";
		}
		return leashes.HandTie(creature, *found) ? "tied" : "can't";
	}
	case Kind::LeashOrderAt:
	{
		const auto point = MapPoint(_middle, command.point);
		const glm::vec3 onLand {point.x, Locator::terrainSystem::value().GetHeightAt(point), point.y};
		return leashes.OrderAt(command.player, onLand) ? "taken" : "refused";
	}
	case Kind::LeashOrderOn:
		if (const auto object = ObjectAt(command.object))
		{
			return leashes.OrderOn(command.player, *object) ? "taken" : "refused";
		}
		return "it is gone";
	case Kind::HangLeashPosts:
	{
		constexpr float k_Apart = 6.0f;
		constexpr float k_Up = 5.0f;
		const auto middle = MapPoint(_middle, command.point);
		std::array<glm::vec3, 3> points {};
		for (size_t i = 0; i < points.size(); ++i)
		{
			const glm::vec2 at {middle.x + ((static_cast<float>(i) - 1.0f) * k_Apart), middle.y};
			points.at(i) = {at.x, Locator::terrainSystem::value().GetHeightAt(at) + k_Up, at.y};
		}
		leashes.PlacePosts(command.player, points);
		return "hung";
	}
	case Kind::TapLeashPost:
	{
		std::optional<entt::entity> post;
		Locator::entitiesRegistry::value().Each<const ecs::components::LeashPost>(
		    [&post, &command](entt::entity entity, const ecs::components::LeashPost& at) {
			    if (at.owner == command.player && at.type == creature_leash::k_Types.at(command.value))
			    {
				    post = entity;
			    }
		    });
		return post.has_value() && leashes.TapPost(command.player, *post) ? "tapped" : "no such leash";
	}
	case Kind::LeashTapBuilding:
	{
		const auto object = ObjectAt(command.object);
		if (!object.has_value())
		{
			return "it is gone";
		}
		if (leashes.TapThing(command.player, *object))
		{
			return "the leash takes the tap";
		}
		if (!Locator::abodeKnockSystem::has_value())
		{
			return "no knock";
		}
		const auto& at = Locator::entitiesRegistry::value().Get<const ecs::components::Transform>(*object).position;
		return Locator::abodeKnockSystem::value().Tap(*object, at, true) ? "knocked" : "not knocked";
	}
	case Kind::UntieLeash:
		// As the hand unties it, the leash goes back to the hand with the attach sound
		leashes.UntieToHand(creature);
		leashes.PlayHandAttachSound(creature);
		return {};
	case Kind::TakeOffLeash:
		leashes.TakeOff(creature);
		return {};
	case Kind::ConfineToHome:
		leashes.ConfineToHome(creature, command.radius);
		return {};
	default:
		return {};
	}
}

std::string Runner::PutLoadedArena(const Command& command)
{
	const auto first = CreatureAt(command.creature);
	const auto second = CreatureAt(command.value);
	if (!first.has_value() || !second.has_value())
	{
		return "no creatures";
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto middle = (registry.Get<const ecs::components::Transform>(*first).position +
	                     registry.Get<const ecs::components::Transform>(*second).position) *
	                    0.5f;
	const glm::ivec2 place {map_coords::ToFixed(middle.x), map_coords::ToFixed(middle.z)};
	const auto arena = ecs::archetypes::ArenaArchetype::CreateLoaded(place, command.radius);
	return fmt::format("arena {} of radius {:.1f}", entt::to_integral(arena), command.radius);
}

std::string Runner::GiveFightCommand(entt::entity creature, const Command& command)
{
	if (!Locator::creatureFightSystem::has_value())
	{
		return "no fights";
	}
	auto& fights = Locator::creatureFightSystem::value();
	switch (command.kind)
	{
	case Kind::StartFight:
		if (const auto opponent = CreatureAt(command.value))
		{
			constexpr std::array<std::string_view, 6> k_Results {"started",  "no opponent", "busy",
			                                                     "too weak", "no arena",    "refused"};
			return std::string(k_Results.at(static_cast<size_t>(fights.StartFight(creature, *opponent))));
		}
		return "it is gone";
	case Kind::FightBlow:
	{
		constexpr std::array k_Bands {creature_fight::Band::High, creature_fight::Band::Mid, creature_fight::Band::Low};
		// As a click held for the charge, then let go
		const bool queued = fights.QueueMove(creature, creature_fight::AttackMove(k_Bands.at(command.value)), true);
		fights.ReleaseCharge(creature, command.chargeMs);
		return queued ? fmt::format("{:.0f} ms", command.chargeMs) : "not fighting";
	}
	case Kind::FightBlock:
		return fights.QueueMove(creature, creature_fight::BlockMove(), true) ? "" : "not fighting";
	case Kind::FightStep:
		return fights.QueueMove(creature, creature_fight::StepMove(static_cast<creature_fight::Step>(command.value)), true)
		           ? ""
		           : "not fighting";
	case Kind::FightSpecial:
		return fights.QueueMove(creature, {.kind = creature_fight::Move::Kind::Special}, true) ? "" : "not fighting";
	case Kind::FightAuto:
		fights.SetAutoFighting(creature, command.value != 0);
		return fights.IsFighting(creature) ? "" : "not fighting";
	case Kind::FightSpell:
		return fights.QueueMove(
		           creature, {.kind = creature_fight::Move::Kind::Spell, .value = static_cast<uint32_t>(command.value)}, false)
		           ? ""
		           : "not fighting";
	case Kind::FightView:
		fights.FlewTo(Locator::entitiesRegistry::value().Get<const ecs::components::Transform>(creature).position);
		return {};
	case Kind::FightAutopilot:
	{
		auto* autopilot = Locator::inspector::has_value() ? Locator::inspector::value().GetFightAutopilot() : nullptr;
		if (autopilot == nullptr)
		{
			return "no fight autopilot: start the game with the inspector (--inspect-port 0)";
		}
		return autopilot->Start({.policy = static_cast<fight_autopilot::Policy>(command.value),
		                         .seed = 1,
		                         .stage = std::nullopt,
		                         .sequence = {},
		                         .enterView = true});
	}
	case Kind::FightLessonCheck:
	{
		if (!Locator::fightTranscript::has_value())
		{
			return "no fight transcript: start the game with the inspector (--inspect-port 0)";
		}
		const auto& recorder = Locator::fightTranscript::value().GetRecorder();
		const auto fight = recorder.FightOf(entt::to_integral(creature));
		if (!fight.has_value())
		{
			return "failed: no fight is recorded for the creature";
		}
		const auto check = fight_autopilot::CheckLesson(*recorder.Find(*fight), entt::to_integral(creature));
		const auto at = [](const std::optional<uint64_t>& index) {
			return index.has_value() ? fmt::format("{}", *index) : std::string("none");
		};
		return fmt::format("{}: hit at record {}, hits at {}, block at {}, step at {} of fight {}",
		                   check.InOrder() ? "passed" : "failed", at(check.hit), at(check.hits), at(check.block),
		                   at(check.step), *fight);
	}
	case Kind::KnockOut:
		fights.KnockOut(creature);
		return {};
	case Kind::BringRound:
		fights.Resurrect(creature);
		return {};
	case Kind::FightAbort:
		fights.AbortFight(creature);
		return {};
	case Kind::TieLeashToCreature:
		if (const auto other = CreatureAt(command.value); other.has_value() && Locator::leashSystem::has_value())
		{
			return Locator::leashSystem::value().HandTie(creature, *other) ? "tied" : "can't";
		}
		return "it is gone";
	default:
		return {};
	}
}

void Runner::GiveCreatureCommand(const Command& command)
{
	const auto entity = CreatureAt(command.creature);
	if (!entity.has_value() || !Locator::creatureLocomotionSystem::has_value() || !Locator::creatureMindSystem::has_value())
	{
		return;
	}
	const auto& setup = _scenario->creatures[command.creature];
	const auto who = setup.label.empty() ? fmt::format("creature {}", command.creature) : std::string(setup.label);
	// Lying out cold, it does nothing it is told until it comes round
	if (const auto* needs = Locator::entitiesRegistry::value().TryGet<const CreatureNeeds>(*entity);
	    needs != nullptr && needs->rest == CreatureNeeds::Rest::Unconscious && command.kind != Kind::BringRound)
	{
		Log(fmt::format("{:.1f}s: {} {}: out cold", _seconds, who, Name(command.kind)));
		return;
	}
	auto& locomotion = Locator::creatureLocomotionSystem::value();
	auto& minds = Locator::creatureMindSystem::value();
	const auto point = MapPoint(_middle, command.point);
	std::string result;
	switch (command.kind)
	{
	case Kind::WalkTo:
	case Kind::RunTo:
		result =
		    MoveResultName(locomotion.MoveTo(*entity, point, command.kind == Kind::RunTo ? Pace::Run : Pace::Walk, 0.0f, 1.0f));
		break;
	case Kind::ScriptPlay:
		result = minds.CarryOutForScript(
		             *entity, creature_script_play::Agenda({.animation = static_cast<int32_t>(command.value),
		                                                    .plays = static_cast<uint32_t>(std::lround(command.amount))}))
		             ? "playing"
		             : "can't";
		break;
	case Kind::AnimalFlees:
	{
		const auto animal = ObjectAt(command.object);
		result = animal.has_value() && Locator::animalSystem::has_value() &&
		                 Locator::animalSystem::value().SetupFleeFromObject(*animal, *entity, Reaction::FleeFromObject)
		             ? "flees"
		             : "can't";
		break;
	}
	case Kind::DoAction:
	{
		// A creature moves a totem only when its town has a worship site built
		if (const auto centre = ObjectAt(command.object); command.onTotem && centre.has_value())
		{
			BuildWorshipSiteOf(*centre);
		}
		const auto* tables = minds.GetTables();
		const auto action = tables != nullptr ? creature_mind_tables::FindAction(*tables, command.action) : std::nullopt;
		const auto target = command.atCreature.has_value()                 ? CreatureAt(*command.atCreature)
		                    : command.onTemple && !_temples.empty()        ? std::optional(_temples.front())
		                    : command.onGlobe && command.globe.has_value() ? GlobeAt(*command.globe)
		                    : command.onTotem                              ? TotemAt(command.object)
		                    : command.onPiece                              ? PieceNearest(*entity)
		                                                                   : ObjectAt(command.object);
		const auto with = command.globe.has_value() && !command.onGlobe ? GlobeAt(*command.globe)
		                  : command.usesCreature.has_value()            ? CreatureAt(*command.usesCreature)
		                  : command.with.has_value()                    ? ObjectAt(*command.with)
		                                                                : std::nullopt;
		result = !action.has_value()                                     ? "no such action"
		         : !target.has_value()                                   ? "no such object"
		         : minds.ScriptDoAction(*entity, *action, *target, with) ? fmt::format("doing {}", command.action)
		                                                                 : "can't";
		break;
	}
	case Kind::GodOrder:
	{
		const auto* tables = minds.GetTables();
		const auto action = tables != nullptr ? creature_mind_tables::FindAction(*tables, command.action) : std::nullopt;
		const auto target = command.atCreature.has_value() ? CreatureAt(*command.atCreature) : std::nullopt;
		result = !action.has_value()                             ? "no such action"
		         : !target.has_value()                           ? "no such creature"
		         : minds.ObeyGodOrder(*entity, *action, *target) ? fmt::format("ordered {}", command.action)
		                                                         : "can't";
		break;
	}
	case Kind::MakeFriends:
		if (const auto other = CreatureAt(command.value))
		{
			auto& registry = Locator::entitiesRegistry::value();
			for (const auto each : {*entity, *other})
			{
				if (!registry.AllOf<ecs::components::CreatureFriends>(each))
				{
					registry.Assign<ecs::components::CreatureFriends>(each);
				}
			}
			creature_activity::MakeFriends(
			    registry.Get<ecs::components::CreatureFriends>(*entity).friends, entt::to_integral(*entity),
			    registry.Get<ecs::components::CreatureFriends>(*other).friends, entt::to_integral(*other));
			result = "friends";
		}
		else
		{
			result = "no such creature";
		}
		break;
	case Kind::OnlyDesire:
		if (command.value < creature_desires::k_DesireCount)
		{
			// For the seconds given (amount), as a script gives them
			minds.SetOnlyDesire(*entity, static_cast<creature_desires::Desire>(command.value), command.amount);
			result = fmt::format("only {}", creature_desires::Name(static_cast<creature_desires::Desire>(command.value)));
		}
		else
		{
			result = "no such desire";
		}
		break;
	case Kind::ScriptReaction:
		ecs::creature_reaction_notice::SetTakesNotice(Locator::entitiesRegistry::value(), *entity, command.value != 0);
		result = command.value != 0 ? "takes notice" : "takes no notice";
		break;
	case Kind::SwapMinds:
		if (const auto other = CreatureAt(command.value))
		{
			result = minds.SwapMinds(*entity, *other) ? "swapped" : "can't";
		}
		else
		{
			result = "no such creature";
		}
		break;
	case Kind::WalkHome:
		if (const auto* leash = Locator::entitiesRegistry::value().TryGet<const ecs::components::CreatureLeash>(*entity);
		    leash != nullptr && leash->home.has_value())
		{
			result =
			    MoveResultName(locomotion.MoveTo(*entity, glm::vec2(leash->home->x, leash->home->z), Pace::Walk, 0.0f, 1.0f));
		}
		else
		{
			result = "it has no home";
		}
		break;
	case Kind::Follow:
		if (const auto leader = CreatureAt(command.value))
		{
			const auto size = ShownSize(Locator::entitiesRegistry::value().Get<Creature>(*entity));
			result = MoveResultName(locomotion.Follow(*entity, *leader, k_FollowDistance * std::max(size, 0.5f), Pace::Walk));
		}
		break;
	case Kind::FleeFrom:
		result = MoveResultName(locomotion.FleeFrom(*entity, point, Pace::Run));
		break;
	case Kind::TurnToFace:
		result = locomotion.TurnToFace(*entity, point) ? "turning" : "can't";
		break;
	case Kind::FaceCamera:
		result = locomotion.TurnToFace(*entity, glm::xz(Locator::camera::value().GetOrigin())) ? "turning" : "can't";
		break;
	case Kind::Stop:
		locomotion.Stop(*entity);
		break;
	case Kind::StopAtOnce:
		locomotion.StopAtOnce(*entity);
		break;
	case Kind::HoldRouteSearch:
		if (auto* walk = Locator::entitiesRegistry::value().TryGet<ecs::components::CreatureLocomotion>(*entity))
		{
			walk->searchHeld = command.value != 0;
		}
		break;
	case Kind::TurnToFaceWhenStill:
		if (locomotion.TurnToFace(*entity, point))
		{
			result = "turning";
		}
		else
		{
			_turnsAsked.emplace_back(*entity, point);
			result = "waits until it stands";
		}
		break;
	case Kind::ScriptPlace:
		locomotion.PlaceWithoutWalking(*entity, {point.x, Locator::terrainSystem::value().GetHeightAt(point), point.y});
		break;
	case Kind::PlayAction:
		result = minds.PlayAction(*entity, command.value) ? "playing" : "busy";
		break;
	case Kind::PlayGesture:
		result = minds.PlayGesture(*entity, command.value) ? "playing" : "busy";
		break;
	case Kind::PullFace:
		minds.PullFace(*entity, command.value);
		break;
	case Kind::CreatureDrawGesture:
		result = minds.DrawGesture(*entity, static_cast<uint32_t>(command.value)) ? "drawing" : "busy";
		break;
	case Kind::ShowFeeling:
		if (const auto face = minds.ShowFeeling(*entity, static_cast<creature_face::Cue>(command.value)))
		{
			result = creature_face::Name(face->face);
		}
		break;
	case Kind::SitDown:
		result = minds.SitDown(*entity) ? "sitting" : "busy";
		break;
	case Kind::StandUp:
		minds.StandUp(*entity);
		break;
	case Kind::Sleep:
		result = minds.Sleep(*entity) ? "started" : "can't";
		break;
	case Kind::Wake:
		minds.Wake(*entity);
		break;
	case Kind::Eat:
		result = minds.Eat(*entity, std::nullopt) ? "started" : "nothing to eat";
		break;
	case Kind::Drink:
		result = minds.Drink(*entity) ? "started" : "no water near";
		break;
	case Kind::Poo:
		result = minds.Poo(*entity) ? "started" : "can't";
		break;
	case Kind::ForceAction:
		result =
		    minds.ForcePlan(
		        *entity,
		        {.desire = creature_desires::Desire::ObeyPlayer, .action = command.action, .object = *entity, .ordered = true})
		        ? fmt::format("forced {}", command.action)
		        : "can't";
		break;
	case Kind::Fart:
		// A fart is always a plan for the desire to poo
		result = minds.ForcePlan(*entity, {.desire = creature_desires::Desire::Poo, .action = "Fart"}) ? "started" : "can't";
		break;
	case Kind::Puke:
		result = minds.Puke(*entity) ? "started" : "can't";
		break;
	case Kind::Faint:
		result = minds.Faint(*entity) ? "started" : "can't";
		break;
	case Kind::Stroke:
	case Kind::Slap:
		// As a whole session of the hand on the creature would, a full reward or punishment as the hand lets go
		minds.ReceiveFeedback(*entity, command.kind == Kind::Stroke ? 1.0f : -1.0f);
		break;
	case Kind::PickUp:
	case Kind::PutDown:
	case Kind::Discard:
	case Kind::EatHeld:
	case Kind::Examine:
	case Kind::ThrowAt:
	case Kind::KnockDown:
	case Kind::PointAt:
		result = GiveObjectCommand(*entity, command);
		break;
	case Kind::HandStroke:
	case Kind::HandSlap:
	case Kind::HandLetGo:
	case Kind::HandLock:
		result = GiveHandCommand(*entity, command);
		break;
	case Kind::PutOnLeash:
	case Kind::TieLeash:
	case Kind::TieLeashToDesireFlag:
	case Kind::TieLeashToWorshipSiteFlag:
	case Kind::UntieLeash:
	case Kind::TakeOffLeash:
	case Kind::ConfineToHome:
	case Kind::MakeLeashable:
	case Kind::HandTapLeash:
	case Kind::LeashKey:
	case Kind::LeashShake:
	case Kind::LeashOrderAt:
	case Kind::LeashOrderOn:
	case Kind::HangLeashPosts:
	case Kind::TapLeashPost:
	case Kind::LeashTapBuilding:
		result = GiveLeashCommand(*entity, command);
		break;
	case Kind::StartFight:
	case Kind::FightBlow:
	case Kind::FightBlock:
	case Kind::FightStep:
	case Kind::FightSpecial:
	case Kind::FightAuto:
	case Kind::FightSpell:
	case Kind::FightView:
	case Kind::FightAutopilot:
	case Kind::FightLessonCheck:
	case Kind::KnockOut:
	case Kind::BringRound:
	case Kind::FightAbort:
	case Kind::TieLeashToCreature:
		result = GiveFightCommand(*entity, command);
		break;
	case Kind::CreatureKey:
	case Kind::DoubleClick:
	case Kind::CameraKeys:
	case Kind::ClearCameraView:
	case Kind::LeaveCreatureMode:
	case Kind::SavedFollowCamera:
	case Kind::SavedTurningCamera:
	case Kind::SavedFreeCamera:
	case Kind::SavedBelowLandCamera:
	case Kind::OpenCreatureCave:
	case Kind::ApplyTattoo:
	case Kind::RemoveTattoo:
	case Kind::OpenTattooEditor:
		result = GiveCreatureModeCommand(*entity, command);
		break;
	case Kind::GiveHandGlows:
	{
		auto& registry = Locator::entitiesRegistry::value();
		auto& glows = registry.AllOf<CreatureHandGlows>(*entity) ? registry.Get<CreatureHandGlows>(*entity)
		                                                         : registry.Assign<CreatureHandGlows>(*entity);
		for (size_t place = 0; place < creature_hand_glow::k_PlaceCount; ++place)
		{
			creature_hand_glow::Set(glows.glows, static_cast<creature_hand_glow::Place>(place), 1.0f,
			                        static_cast<float>(command.value));
		}
		// A creature with glows that have power makes the hum of the creed, as after a load of the glows
		if (command.value > 0 && Locator::soundTagSystem::has_value())
		{
			Locator::soundTagSystem::value().AttachObjectSound(*entity, static_cast<entt::id_type>(audio::SoundId::G_Creed_01),
			                                                   true);
		}
		result = fmt::format("four glows of power {}", command.value);
		break;
	}
	case Kind::GiveCreedPower:
	{
		// Each shipped script that gives a creed power gives this scale
		constexpr float k_ScriptScale = 5.0f;
		const auto place = creature_hand_glow::PlaceOf(static_cast<int32_t>(command.value));
		if (!place.has_value() || !Locator::creatureAnimationSystem::has_value())
		{
			result = "no such place";
			break;
		}
		Locator::creatureAnimationSystem::value().SetCreedPower(*entity, *place, k_ScriptScale, command.amount);
		result = fmt::format("a creed power of {} at place {}", command.amount, command.value);
		break;
	}
	case Kind::AttachScriptSound:
	{
		if (!Locator::soundTagSystem::has_value())
		{
			result = "no sound tags";
			break;
		}
		const auto sound = static_cast<entt::id_type>(command.value);
		const auto tag = Locator::soundTagSystem::value().AttachObjectSound(*entity, sound, false);
		result = fmt::format("sound {} attached as tag {}", sound, entt::to_integral(tag));
		break;
	}
	case Kind::WriteWebPage:
		if (Locator::creatureWebPageSystem::has_value())
		{
			Locator::creatureWebPageSystem::value().WritePage(*entity);
			Locator::creatureWebPageSystem::value().FlushWrites();
			result = "written";
		}
		else
		{
			result = "no web page system";
		}
		break;
	case Kind::BookmarkCreature:
		// The player keeps the creature in a bookmark, the hand over it, and the camera follows the bookmark
		if (Locator::cameraBookmarkSystem::has_value() && Locator::camera::has_value())
		{
			auto& bookmarks = Locator::cameraBookmarkSystem::value();
			const auto& camera = Locator::camera::value();
			bookmarks.SetBookmark(command.value, camera.GetFocus(), *entity, camera.GetOrigin());
			bookmarks.ToggleFollow(command.value);
			result = fmt::format("bookmark {} on the creature, followed", command.value + 1);
		}
		else
		{
			result = "no bookmarks";
		}
		break;
	case Kind::HandGive:
	{
		const bool given = Locator::handGrabSystem::has_value() && Locator::handGrabSystem::value().GiveHeldTo(*entity);
		// A sweep of the pointer can now move the held hand away, as a player calls off the gift
		_sweepMovesHeldHand = given && _heldHand.has_value();
		result = given ? "given" : "the hand gives nothing";
		break;
	}
	case Kind::SetHour:
	case Kind::RestoreDeviceInDomeFollow:
	case Kind::RestoreDevice:
	case Kind::PutLoadedArena:
	case Kind::HoldSeed:
	case Kind::DrawGesture:
	case Kind::SummonSeed:
	case Kind::PressKey:
	case Kind::HandTakeFireBall:
	case Kind::HandTapObject:
	case Kind::HandTake:
	case Kind::HandTakeShownSeed:
	case Kind::HandApply:
	case Kind::HandCastOnDispenser:
	case Kind::HandCastOnCreature:
	case Kind::CloseHeldMiracle:
	case Kind::JoinFootball:
	case Kind::HandPour:
	case Kind::HandRelease:
	case Kind::SetReportLatency:
	case Kind::AttachObjectScriptSound:
	case Kind::SayLine:
	case Kind::PlayGuidanceVoice:
	case Kind::ScriptRelease:
	case Kind::FillSiteIcons:
	case Kind::BuildWorshipSites:
	case Kind::ScriptBuildsWorshipSites:
	case Kind::PressPlainKey:
	case Kind::PlainKeyDown:
	case Kind::PlainKeyRepeat:
	case Kind::PlainKeyUp:
	case Kind::ChargeIconWithSeedHeld:
	case Kind::TakeIconSeed:
	case Kind::TapSiteIcon:
	case Kind::TapTempleEntrance:
	case Kind::RemoveObject:
	case Kind::FinishBuilding:
	case Kind::FillBuiltAmount:
	case Kind::ScaffoldStartsSite:
	case Kind::DestroyScaffoldSite:
	case Kind::ScaffoldClearsGround:
	case Kind::PauseGame:
	case Kind::SetGameSpeed:
	case Kind::RemoveTeleportStone:
	case Kind::MakeDance:
	case Kind::OpenMultiplayerBox:
	case Kind::EndBox:
	case Kind::SkirmishBox:
	case Kind::OtherGodHand:
	case Kind::ObserverCamera:
	case Kind::OwnGod:
	case Kind::JoinOnline:
	case Kind::BringCreature:
	case Kind::AddIncomingMessage:
	case Kind::Chat:
	case Kind::ComputerPersonality:
	case Kind::ComputerUntaughtSends:
	case Kind::HandGripTotem:
	case Kind::SetAlignment:
	case Kind::Advisor:
	case Kind::DestroyTemple:
	case Kind::SetOpenClose:
	case Kind::LayGateStone:
	case Kind::WideScreen:
	case Kind::SetInterfaceLevel:
	case Kind::FlyToBookmark:
	case Kind::TabToTown:
	case Kind::PlayFallingSpellFilm:
	case Kind::PlayIntroFilm:
	case Kind::RunHelpMessage:
	case Kind::EndGuidanceWaits:
	case Kind::SetTownBeliefInRival:
	case Kind::GiveLandTown:
	case Kind::ProfileFolderCheck:
	case Kind::OwnTattooPicture:
	case Kind::OpenTattooMenu:
	case Kind::TattooEditorAsNetworkGame:
	case Kind::ShowDemoPing:
	case Kind::OpenControlsBox:
	case Kind::OpenMenuPage:
	case Kind::PressMenuKey:
	case Kind::PlayCustomMusic:
	case Kind::ShowCustomMusicControls:
	case Kind::SaveGame:
	case Kind::LoadGame:
	case Kind::QuickSave:
	case Kind::ContinueGame:
	case Kind::PlayLogoEffect:
	case Kind::StopScript:
	case Kind::ClickScroll:
	case Kind::ClickCreatureAt:
	case Kind::CameraLooksAt:
	case Kind::HandTakeNear:
	case Kind::ClickCreatureNear:
	case Kind::TapRewardNear:
	case Kind::RunScript:
	case Kind::CallNative:
	case Kind::StartScriptMusic:
	case Kind::RunText:
	case Kind::ReadTip:
	case Kind::LookAtTempleScroll:
	case Kind::LookAtSaveGamePicture:
	case Kind::PlayHandDemo:
	case Kind::Reframe:
	case Kind::RecordChallenge:
	case Kind::EnterTemple:
	case Kind::PlayBoatFilm:
	// The mouse commands are given before a creature is looked for
	case Kind::PointerTo:
	case Kind::PointerPress:
	case Kind::PointerRelease:
	case Kind::PointerSweep:
	case Kind::WheelTurn:
	case Kind::PointerOnOpponent:
	case Kind::PointerOnCaveCreature:
	case Kind::PointerOnCreature:
	case Kind::PointerOnAltar:
	case Kind::PointerOnRope:
	case Kind::PointerOnObject:
		break;
	case Kind::SetDesire:
	case Kind::SetPhase:
	case Kind::RewardIf:
	case Kind::SetMiracleSightings:
		result = TeachMind(*entity, command);
		break;
	case Kind::SetAlight:
	{
		// With a value of 1, the scenario's object catches fire in place of the creature
		const auto burning = command.value == 1 ? ObjectAt(command.object) : std::optional(*entity);
		if (Locator::fireSystem::has_value() && burning.has_value())
		{
			Locator::fireSystem::value().SetOnFire(*burning, 0.0f);
		}
		result = Locator::fireSystem::has_value() && burning.has_value() ? "alight" : "no fire";
		break;
	}
	case Kind::SetFizz:
		if (Locator::creatureFizzSystem::has_value())
		{
			Locator::creatureFizzSystem::value().SetFizz(*entity, command.amount, static_cast<float>(command.value) / 1000.0f,
			                                             false);
		}
		result = Locator::creatureFizzSystem::has_value() ? fmt::format("fizzes to {:.2f}", command.amount) : "no fizz";
		break;
	case Kind::SetBodyLook:
	{
		auto* body = Locator::entitiesRegistry::value().TryGet<ecs::components::Creature>(*entity);
		if (body == nullptr)
		{
			result = "no creature";
			break;
		}
		auto& look = command.value == 0 ? body->alignment : body->fatness;
		look = command.amount;
		result = fmt::format("{} {:.2f}", command.value == 0 ? "alignment" : "fatness", command.amount);
		break;
	}
	case Kind::SetFightLean:
	{
		auto& registry = Locator::entitiesRegistry::value();
		auto* found = registry.TryGet<ecs::components::CreatureFightRecord>(*entity);
		auto& record = found != nullptr ? *found : registry.Assign<ecs::components::CreatureFightRecord>(*entity);
		record.tendency = command.amount;
		result = fmt::format("leans {:+.2f}", record.tendency);
		break;
	}
	case Kind::SeeSkill:
		minds.SeeSkill(Locator::entitiesRegistry::value().Get<Transform>(*entity).position, command.value);
		break;
	case Kind::SeeMiracle:
		minds.SeeMiracle(Locator::entitiesRegistry::value().Get<Transform>(*entity).position, command.value);
		break;
	case Kind::KnowMiracle:
		minds.KnowMiracle(*entity, command.value);
		break;
	case Kind::CastMiracle:
	{
		const auto target = command.atCreature.has_value() ? CreatureAt(*command.atCreature) : ObjectAt(command.object);
		result = !target.has_value()                                                       ? "nothing to cast at"
		         : minds.TellCast(*entity, static_cast<MagicType>(command.value), *target) ? ""
		                                                                                   : "can't";
		break;
	}
	case Kind::PlayerDid:
	{
		const auto& land = Locator::terrainSystem::value();
		minds.PlayerDid(command.value, glm::vec3(point.x, land.GetHeightAt(point), point.y), std::nullopt, std::nullopt);
		break;
	}
	case Kind::PlayerDidOn:
	{
		const auto object = ObjectAt(command.object);
		const auto* at = object.has_value() ? Locator::entitiesRegistry::value().TryGet<const Transform>(*object) : nullptr;
		if (at == nullptr)
		{
			result = "no such object";
			break;
		}
		minds.PlayerDid(command.value, at->position, object, std::nullopt);
		break;
	}
	}
	Log(fmt::format("{:.1f}s: {} {}{}{}", _seconds, who, Name(command.kind), result.empty() ? "" : ": ", result));
}

std::string Runner::GiveCreatureModeCommand(entt::entity creature, const Command& command)
{
	if (!Locator::creatureModeSystem::has_value() || !Locator::creatureCaveSystem::has_value())
	{
		return "no creature mode";
	}
	auto& mode = Locator::creatureModeSystem::value();
	auto& cave = Locator::creatureCaveSystem::value();
	const auto following = [&mode] {
		const auto followed = mode.GetThing();
		return followed.has_value() ? fmt::format("following entity {}", static_cast<uint32_t>(*followed))
		                            : std::string("the camera is the player's");
	};
	switch (command.kind)
	{
	case Kind::CreatureKey:
		mode.PressCreatureKey();
		return following();
	case Kind::DoubleClick:
	{
		// Two presses of the left button on the creature, a fifth of a second apart
		const auto now = static_cast<uint32_t>(_seconds * 1000.0f) + 1;
		mode.Press({.milliseconds = now, .creature = creature});
		mode.Press({.milliseconds = now + 200, .creature = creature});
		return following();
	}
	case Kind::CameraKeys:
	{
		constexpr std::array<glm::ivec2, 4> k_Directions {{{-1, 0}, {1, 0}, {0, -1}, {0, 1}}};
		const auto direction = k_Directions.at(std::min<size_t>(command.value, k_Directions.size() - 1));
		mode.HoldKeys(direction.x, direction.y, !command.ctrl, command.ctrl, command.amount);
		return mode.IsActive() ? "" : "not in Creature Mode";
	}
	case Kind::ClearCameraView:
		mode.ClearView();
		if (const auto view = mode.GetView(); view.has_value())
		{
			return fmt::format("heading {:.2f}, pitch {:.2f}", view->yaw, view->pitch);
		}
		return "not in Creature Mode";
	case Kind::LeaveCreatureMode:
		mode.Leave();
		return following();
	case Kind::SavedFollowCamera:
	case Kind::SavedTurningCamera:
	case Kind::SavedFreeCamera:
	case Kind::SavedBelowLandCamera:
		// The follow camera can follow one of the scenario's objects or its first flock of birds in place of the creature
		if (command.kind == Kind::SavedFollowCamera && (command.value & k_SavedCameraOnFlock) != 0)
		{
			return _birdFlocks.empty() ? "no flock" : GiveSavedCameraCommand(_birdFlocks.front(), command);
		}
		return GiveSavedCameraCommand(
		    command.with.has_value() && *command.with < _objects.size() ? _objects[*command.with] : creature, command);
	case Kind::OpenCreatureCave:
		cave.Open();
		cave.GetScreen().page = static_cast<creature_cave::Page>(command.value);
		cave.GetScreen().pageRequested = true;
		return cave.GetCreature().has_value() ? "" : "the player has no creature";
	case Kind::ApplyTattoo:
		return cave.ApplyTattoo(static_cast<uint8_t>(command.bodyPart), static_cast<uint8_t>(command.value),
		                        glm::u8vec3(180, 30, 30))
		           ? ""
		           : "can't";
	case Kind::RemoveTattoo:
		return cave.RemoveTattoo(static_cast<uint8_t>(command.bodyPart)) ? "" : "nothing there";
	case Kind::OpenTattooEditor:
	{
		const auto caveCreature = cave.GetCreature();
		if (!caveCreature.has_value() || !Locator::tattooEditorSystem::has_value())
		{
			return "the player has no creature";
		}
		const auto& transform = Locator::entitiesRegistry::value().Get<const ecs::components::Transform>(*caveCreature);
		Locator::tattooEditorSystem::value().Open(*caveCreature,
		                                          creature_tattoo_editor::CaveView(transform.position, transform.scale.x));
		return "";
	}
	default:
		return {};
	}
}

std::string Runner::TeachMind(entt::entity entity, const Command& command)
{
	auto* mind = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(entity);
	if (mind == nullptr)
	{
		return "no mind";
	}
	switch (command.kind)
	{
	case Kind::SetDesire:
		if (mind->desires.has_value() && command.value < creature_desires::k_DesireCount)
		{
			auto& state = mind->desires->desires.at(command.value);
			state.activated = true;
			state.suppressedTurns = 0;
			state.value = command.amount * std::max(state.max, 0.0f);
			return fmt::format("{} {:.2f}", creature_desires::Name(static_cast<creature_desires::Desire>(command.value)),
			                   state.value);
		}
		return "no desires yet";
	case Kind::SetPhase:
		mind->developmentPhase = static_cast<uint32_t>(command.value);
		return fmt::format("stage {}", command.value);
	case Kind::SetMiracleSightings:
	{
		if (!mind->learnt.has_value())
		{
			return "nothing learnt yet";
		}
		auto& knowledge = mind->learnt->knowledge;
		if (command.value >= knowledge.miraclesSeen.size() || command.value >= knowledge.miraclesKnown.size())
		{
			return "no such miracle";
		}
		knowledge.miraclesSeen.at(command.value).count = static_cast<uint32_t>(command.amount);
		knowledge.miraclesKnown.at(command.value) = true;
		return fmt::format("miracle {} seen {} times", command.value, knowledge.miraclesSeen.at(command.value).count);
	}
	case Kind::RewardIf:
		// From now on each thing it does to something is judged as soon as it is done
		mind->trainer = static_cast<uint32_t>(command.value);
		return fmt::format("trained: stroked for a {}, slapped for anything else",
		                   creature_tree::BeliefName(static_cast<uint32_t>(command.value)));
	default:
		return {};
	}
}

void Runner::LoadMindFile(entt::entity entity, std::string_view name)
{
	constexpr std::string_view k_Chosen = "chosen:";
	constexpr std::string_view k_Game = "game:";
	std::filesystem::path gameFolder;
	if (Locator::filesystem::has_value())
	{
		gameFolder = Locator::filesystem::value().GetPath<filesystem::Path::CreatureMind>(true);
	}
	std::filesystem::path path;
	if (name.starts_with(k_Chosen))
	{
		// The mind file last opened with the spawner's file dialog, else the game's own mind of that name
		std::error_code error;
		const auto chosen = file_dialog::RememberedPath("creature-mind-file");
		if (chosen.has_value() && std::filesystem::is_regular_file(*chosen, error))
		{
			path = *chosen;
		}
		else if (!gameFolder.empty())
		{
			path = gameFolder / name.substr(k_Chosen.size());
		}
	}
	else if (name.starts_with(k_Game) && !gameFolder.empty())
	{
		path = gameFolder / name.substr(k_Game.size());
	}
	auto data = std::make_shared<creaturemind::MindFileData>();
	const auto result = path.empty() ? creaturemind::MindResult::ErrCantOpen : creaturemind::ReadFile(path, *data);
	if (result == creaturemind::MindResult::Success && Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().LoadMind(entity, data);
	}
	Log(fmt::format("mind file {}: {}", path.empty() ? std::string(name) : path.filename().string(),
	                creaturemind::ResultToStr(result)));
}
