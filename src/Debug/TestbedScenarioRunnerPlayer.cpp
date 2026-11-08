/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed runner: the commands that go through the commands of the player.

#include <algorithm>
#include <array>
#include <filesystem>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

#include <MindFile.h>
#include <SDL_events.h>
#include <SDL_keycode.h>
#include <fmt/format.h>
#include <glm/geometric.hpp>

#include "3D/LandIslandInterface.h"
#include "Camera/Camera.h"
#include "Commands/PlayerCommands.h"
#include "Commands/TurnCodec.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureScriptPlay.h"
#include "Debug/ComputerActions.h"
#include "ECS/Components/BuildingDamage.h"
#include "ECS/Components/Construction.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/MagicForest.h"
#include "ECS/Components/MagicPile.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/ObserverInterfaces.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/Workshop.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/CreatureFromFile.h"
#include "ECS/Dances.h"
#include "ECS/ObserverInterfaceRules.h"
#include "ECS/Registry.h"
#include "ECS/ScriptFlocks.h"
#include "ECS/Systems/ChatSystemInterface.h"
#include "ECS/Systems/ComputerPlayerSystemInterface.h"
#include "ECS/Systems/CreatureCaveSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/DanceSystemInterface.h"
#include "ECS/Systems/FootballSystemInterface.h"
#include "ECS/Systems/GameEndSystemInterface.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/GuidanceSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HelpSpeechSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerDance.h"
#include "ECS/Systems/Implementations/VillagerDisciples.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/ParticleRuleSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/ResourceStoreSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "ECS/Systems/SoundTagSystemInterface.h"
#include "ECS/Systems/TeleportSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "ECS/Systems/WorkshopSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/TempleConstruction.h"
#include "ECS/WorldObjects.h"
#include "FileSystem/FileSystemInterface.h"
#include "Gui/GameTexts.h"
#include "Gui/TextDatabase.h"
#include "Help/VillageVoices.h"
#include "InfoConstants.h"
#include "Input/GameActionMapInterface.h"
#include "Input/InjectedInput.h"
#include "Input/InputDeviceInterface.h"
#include "LHScriptX/Script.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Sound/HeaderOverrides.h"
#include "Sound/SoundRequestInterface.h"
#include "TestbedScenarioRunner.h"
#include "TestbedScenarioRunnerParts.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;
using namespace openblack::testbed_scenarios::runner_parts;
using openblack::ecs::components::Transform;

namespace
{
/// How high above its hand another god reports its camera, in metres
constexpr float k_OtherGodCameraAbove = 30.0f;
} // namespace

std::string Runner::GivePlayerCommand(const Command& command)
{
	switch (command.kind)
	{
	case Kind::HoldSeed:
	{
		if (!Locator::magicSystem::has_value())
		{
			return "no miracles";
		}
		auto& magic = Locator::magicSystem::value();
		magic.DiscardHeldSeedOf(command.player);
		const auto seed = static_cast<SpellSeedType>(command.value);
		const auto given = magic.GiveSeedToHand(command.player, seed, magic::k_BasePowerUpLevel, 1.0f);
		return given != entt::null ? fmt::format("seed {} in the hand", command.value) : "the hand isn't free";
	}
	case Kind::DrawGesture:
		return DrawGesture(static_cast<GestureType>(command.value));
	case Kind::PressKey:
		if (!Locator::gameActionSystem::has_value())
		{
			return "no keys";
		}
		Locator::gameActionSystem::value().QueuePress(static_cast<input::BindableActionMap>(command.value));
		return {};
	case Kind::SummonSeed:
	{
		if (!Locator::magicSystem::has_value())
		{
			return "no miracles";
		}
		auto& magic = Locator::magicSystem::value();
		magic.DiscardHeldSeedOf(command.player);
		const auto seed = static_cast<SpellSeedType>(command.value);
		const auto given = magic.SummonSeed(command.player, seed, magic::k_BasePowerUpLevel);
		return given != entt::null ? fmt::format("seed {} summoned to the hand", command.value) : "the hand isn't free";
	}
	case Kind::HandTakeFireBall:
		return HandTakeFireBall();
	case Kind::HandTapObject:
		return HandTapObject(command.object);
	case Kind::JoinFootball:
	{
		const auto object = ObjectAt(command.object);
		auto& registry = Locator::entitiesRegistry::value();
		const auto* villager = object.has_value() ? registry.TryGet<const ecs::components::Villager>(*object) : nullptr;
		if (villager == nullptr || !Locator::footballSystem::has_value())
		{
			return "no villager to join the football";
		}
		return Locator::footballSystem::value().Join(villager->town, *object) ? "joined the football" : "the town has no pitch";
	}
	case Kind::ScriptRelease:
	{
		const auto object = ObjectAt(command.object);
		if (!object.has_value() || !Locator::scriptObjects::has_value())
		{
			return "nothing to release";
		}
		Locator::scriptObjects::value().ReleaseFromScript(*object);
		return Locator::entitiesRegistry::value().AllOf<ecs::components::ScriptControlled>(*object) ? "still controlled"
		                                                                                            : "released";
	}
	case Kind::PauseGame:
		Locator::time::value().SetPaused(command.value != 0);
		return command.value != 0 ? "paused" : "running";
	case Kind::SetGameSpeed:
		Locator::time::value().SetSpeed(command.amount);
		return fmt::format("speed {}", command.amount);
	case Kind::FinishBuilding:
	{
		const auto object = ObjectAt(command.object);
		if (!object.has_value())
		{
			return "nothing to finish";
		}
		ecs::construction::SetBuilt(*object, 1.0f);
		return "finished";
	}
	case Kind::FillBuiltAmount:
	{
		const auto object = ObjectAt(command.object);
		auto* progress =
		    object.has_value() ? Locator::entitiesRegistry::value().TryGet<ecs::components::BuildProgress>(*object) : nullptr;
		if (progress == nullptr)
		{
			return "nothing going up";
		}
		progress->built = 1.0f;
		return "all of it built, not finished";
	}
	case Kind::ScaffoldStartsSite:
	{
		const auto object = ObjectAt(command.object);
		if (!object.has_value() || !Locator::workshopSystem::has_value())
		{
			return "no scaffold";
		}
		return Locator::workshopSystem::value().ForceBuild(*object, PlayerNames::PLAYER_ONE) ? "the site starts"
		                                                                                     : "no building on offer";
	}
	case Kind::ScaffoldClearsGround:
	{
		const auto object = ObjectAt(command.object);
		const auto* scaffold =
		    object.has_value() ? Locator::entitiesRegistry::value().TryGet<const ecs::components::Scaffold>(*object) : nullptr;
		if (scaffold == nullptr || !scaffold->destroysThingsInWay || !Locator::workshopSystem::has_value())
		{
			return "no scaffold that clears its ground";
		}
		Locator::workshopSystem::value().LetGoByHand(*object, false);
		return "the scaffold clears its ground";
	}
	case Kind::DestroyScaffoldSite:
	{
		const auto object = ObjectAt(command.object);
		const auto* scaffold =
		    object.has_value() ? Locator::entitiesRegistry::value().TryGet<const ecs::components::Scaffold>(*object) : nullptr;
		if (scaffold == nullptr || !Locator::entitiesRegistry::value().Valid(scaffold->site))
		{
			return "the scaffold is on no site";
		}
		ecs::world_objects::DestroyedByEffect(scaffold->site, {.killer = std::nullopt, .weight = 0.0f});
		return "the building of the site is destroyed";
	}
	case Kind::RemoveObject:
	{
		const auto object = ObjectAt(command.object);
		if (!object.has_value())
		{
			return "nothing to remove";
		}
		ecs::world_objects::Remove(*object);
		return "removed";
	}
	case Kind::GiveMagicTreesToStore:
	{
		const auto store = ObjectAt(command.object);
		if (!store.has_value() || !Locator::resourceStoreSystem::has_value())
		{
			return "no store";
		}
		auto& registry = Locator::entitiesRegistry::value();
		std::vector<entt::entity> trees;
		registry.Each<const ecs::components::MagicForest>([&trees, &command](const ecs::components::MagicForest& forest) {
			if (trees.empty() && forest.player == command.player)
			{
				trees = forest.trees;
			}
		});
		auto& stores = Locator::resourceStoreSystem::value();
		const auto before = stores.GetResource(*store, ResourceType::Wood);
		size_t given = 0;
		while (trees.size() > command.value)
		{
			const auto tree = trees.back();
			trees.pop_back();
			if (registry.Valid(tree) && stores.TakeObject(*store, tree, command.player))
			{
				++given;
			}
		}
		return fmt::format("{} trees give {} wood to the store, {} trees are left", given,
		                   stores.GetResource(*store, ResourceType::Wood) - before, trees.size());
	}
	case Kind::OwnGod:
	{
		if (!Locator::handSystem::has_value())
		{
			return "no hands";
		}
		auto& hands = Locator::handSystem::value();
		std::ignore = hands.MakeHandsOf(command.player);
		hands.SetOwnPlayer(command.player);
		_ownGodChanged = true;
		return fmt::format("this computer is the god of player {}", static_cast<int>(command.player));
	}
	case Kind::OtherGodHand:
	{
		if (!Locator::handSystem::has_value() || !Locator::terrainSystem::has_value())
		{
			return "no hands";
		}
		auto& hands = Locator::handSystem::value();
		if (command.player == hands.GetOwnPlayer())
		{
			return "the player is the god of this computer";
		}
		std::ignore = hands.MakeHandsOf(command.player);
		// The hand takes the thing before the first report, which asks the influence at no point
		std::string held = "the hand holds nothing";
		if (command.heldSeed.has_value() && Locator::magicSystem::has_value())
		{
			const auto seed = Locator::magicSystem::value().GiveSeedToHand(command.player, *command.heldSeed, 0, 1.0f);
			held = seed != entt::null ? "the hand holds the seed" : "the hand can't hold the seed";
		}
		else if (const auto object = ObjectAt(command.object); object.has_value() && Locator::handGrabSystem::has_value())
		{
			held = Locator::handGrabSystem::value().TakeIntoHandOf(command.player, *object) ? "the hand holds the thing"
			                                                                                : "the hand can't take the thing";
		}
		const auto at = MapPoint(_middle, command.point);
		const glm::vec3 ground {at.x, Locator::terrainSystem::value().GetHeightAt(at), at.y};
		const auto hand = ground + glm::vec3(0.0f, command.radius, 0.0f);
		hands.Report(command.player, {.collidePoint = ground,
		                              .handPoint = hand,
		                              .camera = hand + glm::vec3(0.0f, k_OtherGodCameraAbove, 0.0f),
		                              .focus = ground});
		return held;
	}
	case Kind::ObserverCamera:
	{
		if (!Locator::handSystem::has_value() || !Locator::terrainSystem::has_value() || !Locator::playerSystem::has_value() ||
		    !Locator::gameEndSystem::has_value())
		{
			return "no hands";
		}
		if (command.value == 0 || command.value > turn_codec::k_MostObserverNumber)
		{
			return "no observer number";
		}
		// The god of the observers comes in as the turns bring it: it gets its player and its hands
		const auto player = Locator::playerSystem::value().EnsurePlayer(command.player);
		std::ignore = Locator::handSystem::value().MakeHandsOf(command.player);
		Locator::gameEndSystem::value().ObserversJoined(command.player);
		auto& registry = Locator::entitiesRegistry::value();
		auto* observers = registry.TryGet<ecs::components::ObserverInterfaces>(player);
		if (observers == nullptr)
		{
			observers = &registry.Assign<ecs::components::ObserverInterfaces>(player);
		}
		const auto number = static_cast<uint8_t>(command.value);
		observer_interfaces::Come(*observers, number, gui::ToUtf16(fmt::format("Observer {}", command.value)));
		const auto at = MapPoint(_middle, command.point);
		const glm::vec3 ground {at.x, Locator::terrainSystem::value().GetHeightAt(at), at.y};
		const auto hand = ground + glm::vec3(0.0f, command.radius, 0.0f);
		observer_interfaces::Report(*observers, number, hand, hand + glm::vec3(0.0f, k_OtherGodCameraAbove, 0.0f));
		return fmt::format("observer {} of player {}", command.value, static_cast<int>(command.player));
	}
	case Kind::ComputerPersonality:
		if (!Locator::computerPlayerSystem::has_value())
		{
			return "no computer players";
		}
		Locator::computerPlayerSystem::value().SetPersonality(command.player, command.action, command.amount);
		return fmt::format("{} weighs {}", command.action, command.amount);
	case Kind::ComputerUntaughtSends:
		return debug::computer_actions::SetUntaughtSends(command.player, command.value, static_cast<uint32_t>(command.amount))
		           ? fmt::format("{} sends for kind {}", command.amount, command.value)
		           : "no computer mind";
	case Kind::BringCreature:
	{
		if (!Locator::filesystem::has_value())
		{
			return "no files";
		}
		// The game's own mind file takes the place of the file that the god's player sends before the start
		auto& fileSystem = Locator::filesystem::value();
		const auto path = fileSystem.GetPath<filesystem::Path::CreatureMind>(true) / std::string(command.mindFile);
		const auto bytes = creaturemind::ReadFileBytes(path).value_or(std::vector<uint8_t> {});
		const auto brought = ecs::creature_from_file::Bring(command.player, bytes);
		return brought.creature.has_value() ? fmt::format("creature {}", entt::to_integral(*brought.creature))
		                                    : fmt::format("no creature, reason {}", static_cast<int>(brought.refusal));
	}
	case Kind::RemoveTeleportStone:
	{
		if (!Locator::teleportSystem::has_value())
		{
			return "no teleport";
		}
		auto& teleport = Locator::teleportSystem::value();
		const auto& registry = Locator::entitiesRegistry::value();
		const auto at = MapPoint(_middle, command.point);
		const auto stones = teleport.GetStones(PlayerNames::PLAYER_ONE);
		const auto nearest = std::ranges::min_element(stones, {}, [&registry, &at](entt::entity stone) {
			const auto& position = registry.Get<const ecs::components::Transform>(stone).position;
			return glm::distance(glm::vec2(position.x, position.z), at);
		});
		if (nearest == stones.end())
		{
			return "no stone";
		}
		teleport.RemoveStone(*nearest);
		return "removed";
	}
	case Kind::MakeDance:
	{
		const auto centre = ObjectAt(command.object);
		if (!centre.has_value() || !Locator::danceSystem::has_value())
		{
			return "no place";
		}
		auto& registry = Locator::entitiesRegistry::value();
		const auto& place = registry.Get<const ecs::components::Transform>(*centre).position;
		const auto dance =
		    Locator::danceSystem::value().Create(static_cast<DanceInfo>(command.value), place, *centre, 0, true, entt::null);
		if (dance == entt::null)
		{
			return "no dance";
		}
		uint32_t dancers = 0;
		for (const auto object : _objects)
		{
			if (object == *centre || !registry.Valid(object) || !registry.AllOf<ecs::components::Villager>(object))
			{
				continue;
			}
			// The villager joins the dance as a script's attach does
			if (ecs::dances::AddDancer(registry, dance, object, ecs::villager_dance::DanceSex(object)))
			{
				Locator::livingActionSystem::value().VillagerSetScriptState(object, VillagerStates::InDance);
				++dancers;
			}
		}
		if (command.ctrl)
		{
			// The dance wants the camera of every god, so that every computer of a network game sets the same world and
			// turns its own camera
			for (auto& hand : registry.Get<ecs::components::Dance>(dance).hands)
			{
				hand.cameraWanted = true;
			}
		}
		return fmt::format("{} dancers{}", dancers, command.ctrl ? ", the dance wants its camera" : "");
	}
	case Kind::HandGripTotem:
	{
		const auto totem = TotemAt(command.object);
		if (!totem.has_value() || !Locator::villageTotemSystem::has_value())
		{
			return "no totem";
		}
		BuildWorshipSiteOf(*ObjectAt(command.object));
		auto& totems = Locator::villageTotemSystem::value();
		if (!totems.Grip(*totem, command.player))
		{
			return fmt::format("the hand can't take hold of the totem: {}", totems.WhyNotGrip(*totem, command.player));
		}
		totems.LetGo();
		return "gripped and let go";
	}
	case Kind::HandTake:
	{
		const auto object = command.globe.has_value() ? GlobeAt(*command.globe) : ObjectAt(command.object);
		if (!object.has_value() || !Locator::handGrabSystem::has_value())
		{
			return "nothing to take";
		}
		auto& hand = Locator::handGrabSystem::value();
		const auto why = hand.WhyNotTake(*object);
		return hand.TakeIntoHand(*object) ? std::string("taken into the hand") : fmt::format("the hand can't take it: {}", why);
	}
	case Kind::HandTakeShownSeed:
	{
		// The first seed of the player that shows over its miracle
		std::optional<entt::entity> seed;
		Locator::entitiesRegistry::value().Each<const ecs::components::SpellSeed, const ecs::components::Mesh>(
		    [&seed, &command](entt::entity entity, const ecs::components::SpellSeed& component, const ecs::components::Mesh&) {
			    if (!seed.has_value() && component.followsSpell && component.player == command.player)
			    {
				    seed = entity;
			    }
		    });
		if (!seed.has_value() || !Locator::handGrabSystem::has_value() || !Locator::magicSystem::has_value())
		{
			return "no seed shows over a miracle";
		}
		// The take of a debug tool: the hand takes the seed wherever the pointer is, also outside the influence
		Locator::handGrabSystem::value().TakeIntoHandOf(command.player, *seed);
		const auto held = Locator::magicSystem::value().GetHeldSeedOf(command.player);
		return held == seed ? fmt::format("the hand takes back seed {}", entt::to_integral(*seed))
		                    : fmt::format("the hand did not keep seed {}", entt::to_integral(*seed));
	}
	case Kind::HandApply:
	{
		const auto object = ObjectAt(command.object);
		if (!object.has_value() || !Locator::handGrabSystem::has_value())
		{
			return "nothing to use the held thing on";
		}
		const auto player =
		    Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
		Locator::handGrabSystem::value().DoApply(player, *object);
		return "the hand uses what it holds on the object";
	}
	case Kind::HandCastOnDispenser:
	case Kind::HandCastOnCreature:
	{
		const auto target =
		    command.kind == Kind::HandCastOnDispenser
		        ? (command.value < _dispensers.size() ? std::optional(_dispensers[command.value]) : std::nullopt)
		        : CreatureAt(command.value);
		if (!target.has_value() || !Locator::magicSystem::has_value())
		{
			return "no such thing to cast on";
		}
		auto& magic = Locator::magicSystem::value();
		const auto seed = magic.GetHeldSeedOf(command.player);
		if (!seed.has_value())
		{
			return "the hand holds no miracle";
		}
		magic.DoCast(command.player, player_commands::HandCast {.object = *target});
		const auto& registry = Locator::entitiesRegistry::value();
		const auto* after = registry.TryGet<const ecs::components::SpellSeed>(*seed);
		return fmt::format("cast on thing {}: the seed is {}, linked to miracle {}", entt::to_integral(*target),
		                   magic.GetHeldSeedOf(command.player) == seed ? "still in the hand" : "out of the hand",
		                   after != nullptr ? entt::to_integral(after->spell) : entt::to_integral(entt::entity {entt::null}));
	}
	case Kind::HandCastAtPoint:
	{
		if (!Locator::magicSystem::has_value() || !Locator::terrainSystem::has_value())
		{
			return "no magic";
		}
		auto& magic = Locator::magicSystem::value();
		const auto seed = magic.GetHeldSeedOf(command.player);
		if (!seed.has_value())
		{
			return "the hand holds no miracle";
		}
		const auto at = MapPoint(_middle, command.point);
		const glm::vec3 point {at.x, Locator::terrainSystem::value().GetHeightAt(at), at.y};
		magic.DoCast(command.player,
		             player_commands::HandCast {.point = point, .handPosition = point + glm::vec3(0.0f, 15.0f, 0.0f)});
		const auto* after = Locator::entitiesRegistry::value().TryGet<const ecs::components::SpellSeed>(*seed);
		return fmt::format("cast at ({:.1f}, {:.1f}): the seed is {}, linked to miracle {}", at.x, at.y,
		                   magic.GetHeldSeedOf(command.player) == seed ? "still in the hand" : "out of the hand",
		                   after != nullptr ? entt::to_integral(after->spell) : entt::to_integral(entt::entity {entt::null}));
	}
	case Kind::CloseHeldMiracle:
	{
		if (!Locator::magicSystem::has_value())
		{
			return "no magic";
		}
		auto& magic = Locator::magicSystem::value();
		const auto seed = magic.GetHeldSeedOf(command.player);
		const auto& registry = Locator::entitiesRegistry::value();
		const auto* held = seed.has_value() ? registry.TryGet<const ecs::components::SpellSeed>(*seed) : nullptr;
		if (held == nullptr || held->spell == entt::null)
		{
			return "no miracle is locked on in the hand";
		}
		magic.CloseDown(held->spell);
		magic.DoCastUnlock(command.player, *seed);
		return magic.GetHeldSeedOf(command.player) == seed ? "closed and let go: the hand keeps the seed"
		                                                   : "closed and let go: the seed goes";
	}
	case Kind::AttachObjectScriptSound:
	{
		const auto object = ObjectAt(command.object);
		if (!object.has_value() || !Locator::soundTagSystem::has_value())
		{
			return "nothing to attach a sound to";
		}
		const auto sound = static_cast<entt::id_type>(command.value);
		const auto tag = Locator::soundTagSystem::value().AttachObjectSound(*object, sound, false);
		return fmt::format("sound {} attached to the object as tag {}", sound, entt::to_integral(tag));
	}
	case Kind::HandPour:
	{
		if (!Locator::resourceStoreSystem::has_value())
		{
			return "no stores";
		}
		const auto at = MapPoint(_middle, command.point);
		const glm::vec3 point {at.x, Locator::terrainSystem::value().GetHeightAt(at), at.y};
		const auto amount = static_cast<uint32_t>(command.value);
		// What a store or pile didn't take is left as a new loose pile
		auto& registry = Locator::entitiesRegistry::value();
		const auto loose = [&registry] { return registry.Size<ecs::components::MagicPile>(); };
		const auto before = loose();
		Locator::resourceStoreSystem::value().PourAt(ResourceType::Wood, point, amount, false, command.player, false);
		return fmt::format("{} wood poured at ({:.1f}, {:.1f}): {}", amount, at.x, at.y,
		                   loose() > before ? "left on the land as a new pile" : "taken in");
	}
	case Kind::SayLine:
	{
		if (!Locator::helpSpeechSystem::has_value())
		{
			return "no speech";
		}
		const auto text = static_cast<uint32_t>(command.value);
		if (!command.ctrl)
		{
			Locator::helpSpeechSystem::value().Say(text, audio::SpeechVoice::First, std::nullopt);
			return fmt::format("line of text {} said, heard everywhere", text);
		}
		const auto at = MapPoint(_middle, command.point);
		const glm::vec3 point {at.x, Locator::terrainSystem::value().GetHeightAt(at), at.y};
		Locator::helpSpeechSystem::value().Say(text, audio::SpeechVoice::First, point);
		return fmt::format("line of text {} said at ({:.1f}, {:.1f})", text, at.x, at.y);
	}
	case Kind::PlayGuidanceVoice:
	{
		if (!Locator::sounds::has_value() || !Locator::sounds::value().HasAudio())
		{
			return "no audio";
		}
		const auto sample = fmt::format("Guidance.sad/{}", command.value);
		audio::SoundEffectOptions options {.pitchPercent = guidance::k_VoicePitch,
		                                   .volume = guidance::k_VoiceVolume,
		                                   .voicePlayType = pack::AudioBankLoop::Once};
		if (command.ctrl)
		{
			const auto at = MapPoint(_middle, command.point);
			options.position = glm::vec3 {at.x, Locator::terrainSystem::value().GetHeightAt(at), at.y};
			options.minDistance = command.radius * guidance::k_VoiceNearShare;
			options.maxDistance = command.radius;
			options.callerWins = audio::k_CallerDistancesWin;
		}
		const auto started = Locator::sounds::value().StartSoundEffect(resources::HashIdentifier(sample), options);
		return fmt::format("{} asked for at volume {} and pitch {}%{}{}", sample, guidance::k_VoiceVolume,
		                   guidance::k_VoicePitch, command.ctrl ? ", placed" : ", heard everywhere",
		                   started == entt::null ? ", not started" : "");
	}
	case Kind::FillSiteIcons:
	{
		if (!Locator::infoConstants::has_value())
		{
			return "no info";
		}
		const auto& info = Locator::infoConstants::value();
		auto& registry = Locator::entitiesRegistry::value();
		size_t filled = 0;
		registry.Each<ecs::components::WorshipIcon>([&](ecs::components::WorshipIcon& icon) {
			const auto* site =
			    registry.Valid(icon.site) ? registry.TryGet<const ecs::components::WorshipSite>(icon.site) : nullptr;
			if (site == nullptr || site->player != command.player || icon.seed == SpellSeedType::None)
			{
				return;
			}
			const auto type = magic::GetMagicTypeFromPowerUpLevel(magic::GetSpellSeedInfo(info, icon.seed), icon.powerUp);
			icon.store = magic::GetChantsRequiredToCreate(info, type) * command.amount;
			++filled;
		});
		return fmt::format("{} icons filled to {:.0f}%", filled, command.amount * 100.0f);
	}
	case Kind::ChargeIconWithSeedHeld:
		return ChargeIconWithSeedHeld(command);
	case Kind::TakeIconSeed:
		return TakeIconSeed(command);
	case Kind::TapSiteIcon:
		return TapSiteIcon(command);
	case Kind::TapTempleEntrance:
	{
		if (!Locator::playerCommandSystem::has_value())
		{
			return "no hand";
		}
		auto& registry = Locator::entitiesRegistry::value();
		std::optional<entt::entity> found;
		registry.Each<const ecs::components::TempleEntrance>(
		    [&registry, &found, &command](entt::entity entity, const ecs::components::TempleEntrance& entrance) {
			    const auto* temple =
			        registry.Valid(entrance.temple) ? registry.TryGet<const ecs::components::Temple>(entrance.temple) : nullptr;
			    if (temple != nullptr && temple->owner == command.player)
			    {
				    found = entity;
			    }
		    });
		if (!found.has_value())
		{
			return "no temple";
		}
		// The same command as the hand sends when the Action button taps the entrance
		Locator::playerCommandSystem::value().Submit(player_commands::HandTap {.object = *found});
		return "tapped";
	}
	case Kind::PressPlainKey:
	{
		// The key goes down and comes up again, each as an event of the window
		const auto scancode = static_cast<SDL_Scancode>(command.value);
		for (const bool down : {true, false})
		{
			SDL_Event key {};
			key.type = down ? SDL_KEYDOWN : SDL_KEYUP;
			key.key.windowID = Locator::windowing::has_value() ? Locator::windowing::value().GetID() : 0;
			key.key.state = down ? SDL_PRESSED : SDL_RELEASED;
			key.key.keysym.scancode = scancode;
			key.key.keysym.sym = input::Device().KeyFromScancode(scancode);
			key.key.keysym.mod = KMOD_NONE;
			input::MarkInjected(key);
			input::Device().PushEvent(key);
		}
		return fmt::format("key {} pressed", input::Device().ScancodeName(scancode));
	}
	case Kind::PlainKeyDown:
	case Kind::PlainKeyRepeat:
	case Kind::PlainKeyUp:
	{
		// The key stays held after its event until the scenario lets go of it, though the keyboard does not hold it
		const auto scancode = static_cast<SDL_Scancode>(command.value);
		const bool down = command.kind != Kind::PlainKeyUp;
		SDL_Event key {};
		key.type = down ? SDL_KEYDOWN : SDL_KEYUP;
		key.key.windowID = Locator::windowing::has_value() ? Locator::windowing::value().GetID() : 0;
		key.key.state = down ? SDL_PRESSED : SDL_RELEASED;
		key.key.repeat = command.kind == Kind::PlainKeyRepeat ? 1 : 0;
		key.key.keysym.scancode = scancode;
		key.key.keysym.sym = input::Device().KeyFromScancode(scancode);
		key.key.keysym.mod = KMOD_NONE;
		input::MarkInjected(key);
		Locator::gameActionSystem::value().HoldScriptedKey(static_cast<int>(scancode), down);
		input::Device().PushEvent(key);
		constexpr std::array<std::string_view, 3> k_Done {"down", "repeated", "let go"};
		return fmt::format("key {} {}", input::Device().ScancodeName(scancode),
		                   k_Done.at(static_cast<size_t>(command.kind) - static_cast<size_t>(Kind::PlainKeyDown)));
	}
	case Kind::BuildWorshipSites:
	{
		if (!Locator::worshipSiteSystem::has_value())
		{
			return "no worship sites";
		}
		auto& sites = Locator::worshipSiteSystem::value();
		auto& registry = Locator::entitiesRegistry::value();
		std::vector<entt::entity> unbuilt;
		registry.Each<const ecs::components::WorshipSite>(
		    [&](entt::entity site, const ecs::components::WorshipSite& component) {
			    if (component.player == command.player && !sites.IsBuilt(site))
			    {
				    unbuilt.push_back(site);
			    }
		    });
		for (const auto site : unbuilt)
		{
			sites.BuildBy(site, 1.0f);
		}
		return fmt::format("{} worship sites built", unbuilt.size());
	}
	case Kind::ScriptBuildsWorshipSites:
	{
		auto& registry = Locator::entitiesRegistry::value();
		std::vector<entt::entity> sites;
		registry.Each<const ecs::components::WorshipSite>(
		    [&](entt::entity site, const ecs::components::WorshipSite& component) {
			    if (component.player == command.player)
			    {
				    sites.push_back(site);
			    }
		    });
		for (const auto site : sites)
		{
			ecs::construction::SetBuilt(site, command.amount);
		}
		return fmt::format("{} worship sites set to {}", sites.size(), command.amount);
	}
	case Kind::HandRelease:
		if (!Locator::handGrabSystem::has_value() || !Locator::handGrabSystem::value().LetGoAt(command.velocity))
		{
			return "the hand holds nothing";
		}
		return command.velocity == glm::vec3(0.0f) ? "put down" : "thrown";
	case Kind::SetReportLatency:
		if (!Locator::handSystem::has_value())
		{
			return "no hands";
		}
		Locator::handSystem::value().FixReportLatency(static_cast<uint32_t>(command.value));
		return fmt::format("the reports of the hand come {} ms late", command.value);
	default:
		return {};
	}
}
