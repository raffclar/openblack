/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "HandDemoSystem.h"

#include <HNDFile.h>
#include <SDL_events.h>
#include <SDL_mouse.h>
#include <entt/core/hashed_string.hpp>
#include <spdlog/spdlog.h>

#include "ECS/Components/Abode.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/ScriptFind.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/SpellIconSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/TempleConstruction.h"
#include "FileSystem/FileSystemInterface.h"
#include "Input/GameActionMapInterface.h"
#include "Input/InjectedInput.h"
#include "Input/InputDeviceInterface.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{
/// The game's time now, as the recordings count it: the time into the turn, which the frames play by
uint32_t GameTime()
{
	if (!Locator::time::has_value())
	{
		return 0;
	}
	const auto& time = Locator::time::value();
	return hand_demo::GameHundredths(time.GetTurn(), time.GetTurnFraction());
}

/// The time of the turn, as the recordings count it: the turns and the scripts see only this
uint32_t TurnTime()
{
	return Locator::time::has_value() ? hand_demo::GameHundredths(Locator::time::value().GetTurn(), 0.0f) : 0;
}

/// A demonstration's file, read once
std::shared_ptr<const hnd::HNDFile> LoadDemo(std::string_view name)
{
	if (!Locator::resources::has_value() || !Locator::filesystem::has_value())
	{
		return nullptr;
	}
	auto& cache = Locator::resources::value().GetHandDemos();
	const auto id = entt::hashed_string(std::string(name).c_str()).value();
	if (!cache.Contains(id))
	{
		const auto path =
		    Locator::filesystem::value().GetPath<filesystem::Path::Data>() / "HandDemo" / (std::string(name) + ".hnd");
		try
		{
			cache.Load(id, resources::HandDemoLoader::FromDiskTag {}, path);
		}
		catch (const std::runtime_error& error)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "The hand demonstration {} can't be read: {}", name, error.what());
			return nullptr;
		}
	}
	return cache.Handle(id).handle();
}

/// The game runs at its normal speed while a demonstration plays
void NormalSpeed()
{
	if (Locator::time::has_value())
	{
		Locator::time::value().SetSpeed(1.0f);
	}
}

void PushInjected(SDL_Event& event)
{
	event.motion.windowID = Locator::windowing::has_value() ? Locator::windowing::value().GetID() : 0;
	input::MarkInjected(event);
	input::Device().PushEvent(event);
}

/// The mouse button a recorded button message is: the one that grips the land, or the action button
std::optional<std::pair<uint8_t, bool>> RecordedButton(hnd::HNDMessage message)
{
	switch (message)
	{
	case hnd::HNDMessage::MoveButtonDown:
		return std::pair {static_cast<uint8_t>(SDL_BUTTON_LEFT), true};
	case hnd::HNDMessage::MoveButtonUp:
		return std::pair {static_cast<uint8_t>(SDL_BUTTON_LEFT), false};
	case hnd::HNDMessage::ActionButtonDown:
		return std::pair {static_cast<uint8_t>(SDL_BUTTON_RIGHT), true};
	case hnd::HNDMessage::ActionButtonUp:
		return std::pair {static_cast<uint8_t>(SDL_BUTTON_RIGHT), false};
	default:
		return std::nullopt;
	}
}
/// Where a thing is, as a recording's places are kept
map_coords::MapCoords PlaceOf(const ecs::Registry& registry, entt::entity thing)
{
	const auto& position = registry.Get<const ecs::components::Transform>(thing).position;
	return map_coords::FromMetres({position.x, position.z});
}

/// A recorded click on a thing lands on the same kind of thing near the same place in the world as it is now: the thing
/// under the hand if it is that, else the nearest such thing within reach of the recorded place; and on no thing when
/// the recording had none under the hand
void RetargetClick(const hnd::HNDRecord& record)
{
	if (!hand_demo::ClickCanRetarget(record.message) || !Locator::pickingSystem::has_value() ||
	    !Locator::entitiesRegistry::has_value() || !Locator::entitiesMap::has_value())
	{
		return;
	}
	auto& picking = Locator::pickingSystem::value();
	const auto& registry = Locator::entitiesRegistry::value();
	std::optional<hand_demo::ThingUnderHand> underHand;
	if (const auto object = picking.GetPick().object;
	    object.has_value() && registry.Valid(*object) && registry.AllOf<ecs::components::Transform>(*object))
	{
		// A thing the scripts don't see as one is of no kind a recording names
		underHand =
		    hand_demo::ThingUnderHand {.kind = ecs::script_find::KindOf(registry, *object).value_or(ecs::script_find::Kind {}),
		                               .at = PlaceOf(registry, *object)};
	}
	switch (hand_demo::TargetOfClick(record, underHand))
	{
	case hand_demo::ClickTarget::UnderHand:
		return;
	case hand_demo::ClickTarget::NoThing:
		picking.RetargetObject(std::nullopt, 0.0f);
		return;
	case hand_demo::ClickTarget::NearestOfKind:
		break;
	}
	const auto& map = Locator::entitiesMap::value();
	const auto found =
	    ecs::script_find::FindNearest(hand_demo::RecordedPlace(record), hand_demo::k_RetargetReach, [&](glm::ivec2 cell) {
		    std::vector<ecs::script_find::Candidate> candidates;
		    for (const auto thing : map.GetAllInCell(cell))
		    {
			    if (!registry.Valid(thing) || !registry.AllOf<ecs::components::Transform>(thing))
			    {
				    continue;
			    }
			    const auto kind = ecs::script_find::KindOf(registry, thing);
			    const auto at = PlaceOf(registry, thing);
			    if (kind.has_value() && hand_demo::TakesRecordedClick(record, *kind, at))
			    {
				    candidates.push_back({.entity = thing, .at = at});
			    }
		    }
		    return candidates;
	    });
	// None found near, the click stays on what is under the hand. The game measures a worship site from its totem; the
	// first land has no worship site.
	if (found != entt::null)
	{
		const auto* action = registry.TryGet<const ecs::components::LivingAction>(found);
		const bool dying =
		    registry.AllOf<ecs::components::Villager>(found) && action != nullptr && Locator::livingActionSystem::has_value() &&
		    Locator::livingActionSystem::value().VillagerGetState(*action, ecs::components::LivingAction::Index::Top) ==
		        VillagerStates::Dying;
		const bool building = registry.AllOf<ecs::components::Abode>(found);
		// A thing that the hand cannot click takes the click away from the thing under the hand, onto no thing
		if (!hand_demo::FoundThingTakesClick(
		        {.building = building, .built = building ? ecs::construction::BuiltOf(found) : 0.0f, .dyingVillager = dying}))
		{
			picking.RetargetObject(std::nullopt, 0.0f);
			return;
		}
		picking.RetargetObject(found, record.objectDistance);
		if (const auto logger = spdlog::get("game"); logger != nullptr)
		{
			SPDLOG_LOGGER_DEBUG(logger, "A replayed click lands on entity {}, of its recorded kind", entt::to_integral(found));
		}
	}
}
} // namespace

void HandDemoSystem::Play(std::string_view name, uint32_t task, bool pauseOnTrigger, bool withoutHandModify)
{
	// The hand lets go of what it holds first, unless the demonstration needs it (a miracle to cast)
	// A script starts the demonstration at the turn. It asks about the first player's hand, which ForceDrop empties.
	if (!withoutHandModify && Locator::handGrabSystem::has_value() &&
	    Locator::handGrabSystem::value().GetHeldOf(PlayerNames::PLAYER_ONE).has_value())
	{
		Locator::handGrabSystem::value().ForceDrop();
	}
	// The miracles that charge for the hand of this computer's player at the sites of the player's temple stop too
	if (hand_demo::StopsCharges(withoutHandModify) && Locator::playerSystem::has_value() &&
	    Locator::worshipSiteSystem::has_value() && Locator::spellIconSystem::has_value())
	{
		const auto local = Locator::playerSystem::value().GetLocalPlayer();
		if (const auto temple = Locator::worshipSiteSystem::value().TempleOf(local); temple != entt::null)
		{
			Locator::spellIconSystem::value().CancelChargesFor(temple, local);
		}
	}
	// A leash on the player's creature that is not tied to a thing comes off too
	if (Locator::leashSystem::has_value())
	{
		auto& leash = Locator::leashSystem::value();
		const auto player =
		    Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
		if (const auto creature = leash.PlayersCreature(player);
		    creature.has_value() &&
		    hand_demo::TakesOffLeash(withoutHandModify, leash.IsLeashed(*creature), leash.TiedTo(*creature).has_value()))
		{
			leash.TakeOff(*creature);
		}
	}
	if (_playback)
	{
		Stop();
	}
	NormalSpeed();
	_task = task;
	_name = std::string(name);
	// The cinema bars snap all the way in, and the interface takes the demonstration's input even in a cut scene
	if (Locator::cinematicDirectorSystem::has_value())
	{
		auto& director = Locator::cinematicDirectorSystem::value();
		if (!director.IsWideScreenOn())
		{
			director.SetWideScreen(true, 0);
		}
		director.SnapWideScreen();
		director.SetInterfaceActive(true);
	}
	_file = LoadDemo(name);
	if (_file != nullptr && !_file->records.empty())
	{
		if (Locator::gameActionSystem::has_value())
		{
			auto& actions = Locator::gameActionSystem::value();
			actions.SetScriptedPointer(input::GameActionInterface::ScriptedPointer {
			    .position = actions.GetPointerPosition(),
			    .buttons = 0,
			});
			actions.SetPlayerMouseBlocked(true);
		}
		// The demonstration starts at the time of the turn, and the frames play on from there
		std::vector<hand_demo::Played> played;
		_playback = std::make_unique<hand_demo::SplitPlayback>(_file->records, TurnTime(), pauseOnTrigger, played);
		_replaying = true;
		Apply(played);
	}
	// The script's start clears the mark, the first record's too
	_pauseOnTrigger = pauseOnTrigger;
	_triggerReached = false;
}

bool HandDemoSystem::IsPlaying(uint32_t task) const
{
	return _playback != nullptr && (task == 0 || task == _task);
}

bool HandDemoSystem::IsReplaying() const
{
	return _playback != nullptr && _replaying;
}

bool HandDemoSystem::TakeTrigger()
{
	const bool reached = std::exchange(_triggerReached, false);
	if (reached && _playback)
	{
		_playback->MarkRead();
	}
	return reached;
}

void HandDemoSystem::ProcessTurn()
{
	// The scripts see the demonstration play by the time of the turns alone. It ends when the turns have played its last
	// record, also for the frames.
	if (_playback && !_playback->AdvanceTurn(TurnTime(), _triggerReached))
	{
		Stop();
	}
}

void HandDemoSystem::Update()
{
	if (!_playback || !_replaying)
	{
		return;
	}
	NormalSpeed();
	std::vector<hand_demo::Played> played;
	const bool goesOn = _playback->AdvanceFrame(GameTime(), played);
	Apply(played);
	if (!goesOn)
	{
		EndReplay();
	}
}

void HandDemoSystem::EndReplay()
{
	// The player's mouse has the hand again, and the hand shows the camera's own hints
	_replaying = false;
	_camera.reset();
	_hints = 0;
	_hintAngle = 0.0f;
	_held = {};
	if (Locator::gameActionSystem::has_value())
	{
		auto& actions = Locator::gameActionSystem::value();
		actions.SetScriptedPointer(std::nullopt);
		actions.SetPlayerMouseBlocked(false);
	}
}

void HandDemoSystem::Apply(const std::vector<hand_demo::Played>& played)
{
	if (played.empty() || !Locator::gameActionSystem::has_value())
	{
		return;
	}
	auto& actions = Locator::gameActionSystem::value();
	auto pointer = actions.GetScriptedPointer().value_or(
	    input::GameActionInterface::ScriptedPointer {.position = actions.GetPointerPosition()});
	const auto screen = Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::ivec2(0);
	const float bars = Locator::cinematicDirectorSystem::has_value()
	                       ? Locator::cinematicDirectorSystem::value().GetWideScreenFraction()
	                       : 0.0f;
	for (const auto& [record, hints] : played)
	{
		if (record->message == hnd::HNDMessage::Move)
		{
			// The cursor moves, and the hand with it
			const auto to = hand_demo::CursorPixel(record->cursor, screen, bars);
			const auto moved = to - pointer.position;
			pointer.position = to;
			actions.SetScriptedPointer(pointer);
			SDL_Event event {};
			event.type = SDL_MOUSEMOTION;
			event.motion.state = pointer.buttons;
			event.motion.x = pointer.position.x;
			event.motion.y = pointer.position.y;
			event.motion.xrel = moved.x;
			event.motion.yrel = moved.y;
			PushInjected(event);
		}
		else if (const auto button = RecordedButton(record->message); button.has_value())
		{
			// A button goes down or up where the cursor last was. The hand is given back the throw it was ready to make
			// when this was recorded, so a recorded throw flies as it did when it was recorded, however the replayed
			// hand moved
			RetargetClick(*record);
			if (Locator::handGrabSystem::has_value())
			{
				Locator::handGrabSystem::value().SetRecordedThrow(hand_demo::RecordedThrowVelocity(*record));
			}
			const auto [which, down] = *button;
			pointer.buttons = down ? (pointer.buttons | SDL_BUTTON(which)) : (pointer.buttons & ~SDL_BUTTON(which));
			actions.SetScriptedPointer(pointer);
			SDL_Event event {};
			event.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
			event.button.button = which;
			event.button.state = down ? SDL_PRESSED : SDL_RELEASED;
			event.button.clicks = 1;
			event.button.x = pointer.position.x;
			event.button.y = pointer.position.y;
			PushInjected(event);
		}
		_camera = CameraPose {
		    .origin = {record->cameraPosition[0], record->cameraPosition[1], record->cameraPosition[2]},
		    .focus = {record->cameraFocus[0], record->cameraFocus[1], record->cameraFocus[2]},
		};
		_hints = hints;
		_hintAngle = record->hintValue;
		const auto held = hand_demo::AfterRecord({.move = _held.move, .action = _held.action}, record->message);
		_held = {.move = held.move, .action = held.action};
	}
}

void HandDemoSystem::Stop()
{
	if (!_playback)
	{
		return;
	}
	if (_replaying)
	{
		EndReplay();
	}
	_playback.reset();
	_file.reset();
	_pauseOnTrigger = false;
	_task = 0;
	// In a script's cut scene the interface is put away again
	if (Locator::cinematicDirectorSystem::has_value())
	{
		auto& director = Locator::cinematicDirectorSystem::value();
		if (director.IsWideScreenOn() && director.GetWideScreenOwner() != 0)
		{
			director.SetInterfaceActive(false);
		}
	}
}

void HandDemoSystem::TaskStopped(uint32_t task)
{
	if (IsPlaying(task) && task != 0)
	{
		Stop();
	}
}

void HandDemoSystem::Reset()
{
	Stop();
	_triggerReached = false;
	_name.clear();
}

std::optional<HandDemoSystemInterface::CameraPose> HandDemoSystem::GetCamera() const
{
	return _camera;
}

std::optional<HandDemoSystemInterface::Status> HandDemoSystem::GetStatus() const
{
	if (!_playback || _file == nullptr)
	{
		return std::nullopt;
	}
	return Status {
	    .name = _name,
	    .task = _task,
	    .record = _playback->TurnNext(),
	    .records = _file->records.size(),
	    .pauseOnTrigger = _pauseOnTrigger,
	    .triggerReached = _triggerReached,
	    .holding = _pauseOnTrigger && _triggerReached,
	    .replaying = _replaying,
	    .replayRecord = _playback->FrameNext(),
	    .replayHolding = _playback->FrameHeld(),
	};
}
