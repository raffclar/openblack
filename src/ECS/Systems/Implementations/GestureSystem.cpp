/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "GestureSystem.h"

#include <stdexcept>
#include <utility>

#include <GestureFile.h>
#include <LHVM.h>
#include <entt/core/hashed_string.hpp>
#include <glm/common.hpp>
#include <glm/vector_relational.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "Commands/PlayerCommands.h"
#include "Creature/LeashRules.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/MiracleFxSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "ECS/Systems/SpellIconSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Gestures/GestureTrailBuilder.h"
#include "Help/HelpProfile.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Resources/ResourcesInterface.h"
#include "ScriptHeaders/ScriptStart.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using openblack::gesture::Purpose;

namespace
{
/// The game's templates in the resource caches
constexpr auto k_TemplatesId = entt::hashed_string("gestures/templates");

/// How long a recognised gesture shows in the debug window
constexpr float k_RecognisedShownSeconds = 10.0f;

/// The help counts an event of the player's input at the turn of its next report, if the help is there
void Help(uint32_t event)
{
	if (Locator::helpProfileSystem::has_value())
	{
		Locator::helpProfileSystem::value().Report(event);
	}
}

/// A scribble shakes the hand: the shake sound plays and a band flies off the hand
void ShakeHand()
{
	if (Locator::miracleFxSystem::has_value())
	{
		Locator::miracleFxSystem::value().HandShaken();
	}
}

/// Whether a gesture may ask the player's icons for the miracle of a seed
bool Requestable(PlayerNames player, SpellSeedType seed)
{
	return Locator::spellIconSystem::has_value() && Locator::spellIconSystem::value().ValidForRequest(player, seed);
}

/// The seed of the last miracle that went into the player's hand
SpellSeedType LastSeedGained(PlayerNames player)
{
	return Locator::spellIconSystem::has_value() ? Locator::spellIconSystem::value().LastSeedGained(player)
	                                             : SpellSeedType::None;
}

/// The request for a miracle goes as a command: the icon gives the miracle at the start of a later turn
void SendRequest(player_commands::Action request)
{
	if (Locator::playerCommandSystem::has_value())
	{
		Locator::playerCommandSystem::value().Submit(std::move(request));
	}
}

/// The shape a gesture's trail takes on the land, from its symbol's file; the circle's for a gesture without one
std::optional<gesture::TrailSymbol> TrailSymbolOf(GestureType gesture)
{
	if (!Locator::resources::has_value())
	{
		return std::nullopt;
	}
	const auto& paths = Locator::resources::value().GetCameraPaths();
	for (const auto type : {gesture, GestureType::Circle})
	{
		const auto id = fmt::format("symbol/PathSymbol{}", static_cast<uint32_t>(type));
		if (!paths.Contains(entt::hashed_string(id.c_str()).value()))
		{
			continue;
		}
		const auto path = paths.Handle(entt::hashed_string(id.c_str()));
		std::vector<glm::vec3> points;
		points.reserve(path->GetPoints().size());
		for (const auto& point : path->GetPoints())
		{
			points.push_back(point.position);
		}
		return gesture::MakeTrailSymbol(points);
	}
	return std::nullopt;
}
} // namespace

GestureSystem::GestureSystem() = default;

std::optional<gesture::TrailSymbol> GestureSystem::GetSymbol(GestureType gesture) const
{
	return TrailSymbolOf(gesture);
}
GestureSystem::~GestureSystem() = default;

std::vector<GestureEvent> GestureSystem::TakeEvents()
{
	return std::exchange(_events, {});
}

void GestureSystem::Inject(const GestureEvent& event)
{
	_events.push_back(event);
}

void GestureSystem::Reset()
{
	// The path being drawn is the hand's, not the land's: it is kept, and goes as usual once the camera has moved to
	// the new land
	_events.clear();
	_drawing.clear();
	_picker.Close();
	_seedTurnedOnLand = false;
	_selection.Close();
	_circleSeconds = 0.0f;
	_circleSeed = entt::null;
	_lastRecognised.reset();
	_requests.clear();
	// A new land: no hints, and the collars start unturned; the row's fade carries on
	_hints.hints.clear();
	_hints.collars = {};
}

void GestureSystem::ForgetPath()
{
	_recorder.Reset();
	_sinceSample = 0.0f;
}

void GestureSystem::SeedFirstTurnInHand()
{
	ForgetPath();
	_seedTurnedOnLand = true;
}

void GestureSystem::DrawPath(std::vector<glm::vec2> path, bool holdingAction)
{
	_drawing.assign(path.begin(), path.end());
	_drawingHoldsAction = holdingAction;
	// A drawing starts afresh, as the game's hand does once it has rested
	ForgetPath();
	_pause = 0.0f;
}

std::span<const gestures::GestureTemplate> GestureSystem::GetTemplates() const
{
	if (_templates == nullptr)
	{
		return {};
	}
	return _templates->GetTemplates();
}

void GestureSystem::LoadTemplates()
{
	if (_templatesTried || !Locator::resources::has_value() || !Locator::filesystem::has_value())
	{
		return;
	}
	_templatesTried = true;
	auto& cache = Locator::resources::value().GetGestureTemplates();
	try
	{
		if (!cache.Contains(k_TemplatesId.value()))
		{
			const auto path = Locator::filesystem::value().GetPath<filesystem::Path::Data>() / "Gestures.jty";
			cache.Load(k_TemplatesId.value(), resources::GestureTemplatesLoader::FromDiskTag {}, path);
		}
		_templates = cache.Handle(k_TemplatesId.value()).handle();
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Loaded {} gesture templates", _templates->GetTemplates().size());
	}
	catch (const std::runtime_error& error)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "No gestures can be recognised: {}", error.what());
	}
}

gesture::HandContext GestureSystem::ContextOf(const Frame& frame)
{
	const auto player = frame.player;
	gesture::HandContext context;
	context.actionHeld = frame.actionHeld || (!_drawing.empty() && _drawingHoldsAction);
	const auto* info = Locator::infoConstants::has_value() ? &Locator::infoConstants::value() : nullptr;
	if (info != nullptr)
	{
		const auto& spellSystem = info->spellSystem;
		context.specialMoveGesture = spellSystem.creatureSpecialMoveGesture;
		context.leashGesture = spellSystem.leashSelectionStart;
		context.leashGestures = spellSystem.leashSelectionGestures;
	}

	// The seed in the hand
	std::optional<entt::entity> seedEntity;
	if (Locator::magicSystem::has_value())
	{
		seedEntity = Locator::magicSystem::value().GetHeldSeed();
	}
	if (seedEntity.has_value() && info != nullptr)
	{
		const auto& registry = Locator::entitiesRegistry::value();
		if (const auto* seed = registry.TryGet<ecs::components::SpellSeed>(*seedEntity); seed != nullptr)
		{
			const auto& seedInfo = magic::GetSpellSeedInfo(*info, seed->seedType);
			// Only a seed made at an icon of the player's worship can be powered up
			context.seed = gesture::HandContext::Seed {.ready = seed->ready,
			                                           .cast = seed->hasCast,
			                                           .sizingGesture = seedInfo.sizingGesture,
			                                           .powerUpGestures = seedInfo.powerUpGestures,
			                                           .powerUp = seed->powerUp,
			                                           .canPowerUp = seed->hasIcon};
			context.inInfluence = Locator::magicSystem::value().IsHandInInfluence();
		}
	}
	// A thing other than a seed in the hand, which a scribble in the player's influence shakes out
	if (!seedEntity.has_value() && Locator::handGrabSystem::has_value())
	{
		const auto& hand = Locator::handGrabSystem::value();
		context.holdingObject = hand.GetHeld().has_value();
		if (context.holdingObject)
		{
			context.inInfluence = hand.IsInInfluence();
		}
	}
	// A circle is remembered for the seed it was drawn for
	if (!seedEntity.has_value() || *seedEntity != _circleSeed)
	{
		_circleSeconds = 0.0f;
	}
	context.circleRemembered = _circleSeconds > 0.0f;

	// The player's creature and its leash
	if (Locator::leashSystem::has_value())
	{
		const auto& leashes = Locator::leashSystem::value();
		if (const auto creature = leashes.PlayersCreature(player))
		{
			gesture::HandContext::Creature state;
			state.fighting =
			    Locator::creatureFightSystem::has_value() && Locator::creatureFightSystem::value().IsFighting(*creature);
			state.leashed = leashes.IsLeashed(*creature);
			state.tied = leashes.TiedTo(*creature).has_value();
			for (size_t i = 0; i < creature_leash::k_Types.size(); ++i)
			{
				state.knows.at(i) = leashes.Knows(*creature, creature_leash::k_Types.at(i));
			}
			state.worn = state.leashed ? leashes.TypeOf(*creature) : LeashType::None;
			const auto* picked = Locator::entitiesRegistry::value().TryGet<const ecs::components::CreatureLeash>(*creature);
			state.current = state.leashed ? state.worn : (picked != nullptr ? picked->selected : LeashType::Rope);
			context.creature = state;
		}
	}
	// The player's creature in a duel waits for its special move and the fight miracles it knows, while it takes moves
	if (const auto fighter =
	        Locator::creatureFightSystem::has_value() ? Locator::creatureFightSystem::value().PlayersFighter() : std::nullopt)
	{
		auto& state = context.creature.has_value() ? *context.creature : context.creature.emplace();
		const auto& registry = Locator::entitiesRegistry::value();
		const auto* fighting = registry.TryGet<const ecs::components::CreatureFighting>(*fighter);
		state.fighting = true;
		state.takesFightMoves = fighting != nullptr && fighting->fighter.queue.Size() < creature_fight::MoveQueue::k_Capacity;
		const auto* mind = registry.TryGet<const ecs::components::CreatureMindState>(*fighter);
		const auto* knowledge = mind != nullptr && mind->learnt.has_value() ? &mind->learnt->knowledge : nullptr;
		if (info != nullptr && knowledge != nullptr)
		{
			// The seeds marked for fights, by the miracle each casts unpowered, each with its seed for its hint
			for (size_t seedIndex = 0; seedIndex < info->spellSeed.size(); ++seedIndex)
			{
				const auto& seed = info->spellSeed.at(seedIndex);
				const auto magic = seed.magicTypes.front();
				const auto index = static_cast<size_t>(magic);
				if (seed.unknown0x17C != 0 && index < knowledge->miraclesKnown.size() && knowledge->miraclesKnown[index])
				{
					state.fightMiracles.push_back(
					    {.gesture = seed.gesture, .magic = magic, .seed = static_cast<SpellSeedType>(seedIndex)});
				}
			}
		}
	}
	context.pickerOpen = _picker.open;
	if (Locator::entitiesRegistry::has_value())
	{
		const auto& globals = Locator::entitiesRegistry::value().Context();
		context.hiddenScriptLand = globals.mapScriptGlobals.landNumber == 1 && !globals.skirmish;
	}

	// The miracle selection, and what the player's icons offer to the gestures that open it or call a miracle again
	if (info != nullptr)
	{
		context.ownSelectionGesture = info->spellSystem.selectionSystemGestureNonCreature;
		context.creatureSelectionGesture = info->spellSystem.selectionSystemGestureCreature;
		context.repeatGesture = info->spellSystem.selectionSystemGestureR;
	}
	if (Locator::spellIconSystem::has_value())
	{
		const auto& icons = Locator::spellIconSystem::value();
		context.handReady = icons.IsHandReady(player);
		context.ownSelectionOffered = !icons.SeedsForSelection(player, context.ownSelectionGesture).empty();
		context.creatureSelectionOffered = !icons.SeedsForSelection(player, context.creatureSelectionGesture).empty();
		context.lastSeed = LastSeedGained(player);
		context.repeatOffered = icons.ValidForRequest(player, context.lastSeed);
	}
	if (_selection.IsOpen())
	{
		context.selection =
		    gesture::HandContext::Selection {.openedWith = _selection.OpenedWith(), .waiting = _selection.Waiting()};
	}
	return context;
}

void GestureSystem::Update(const Frame& frame)
{
	LoadTemplates();
	const auto lastCursor = _lastCursor.value_or(frame.cursor);
	_lastCursor = frame.cursor;
	if (frame.view.screenSize.x > 0.0f && frame.view.screenSize.y > 0.0f)
	{
		_screenAspect = frame.view.screenSize.x / frame.view.screenSize.y;
	}
	if (_lastRecognised.has_value())
	{
		_lastRecognised->age += frame.seconds;
		if (_lastRecognised->age > k_RecognisedShownSeconds)
		{
			_lastRecognised.reset();
		}
	}
	_pause = std::max(_pause - frame.seconds, 0.0f);
	_circleSeconds = std::max(_circleSeconds - frame.seconds, 0.0f);

	// The leash picker closes once the leash is off, or after a while
	auto context = ContextOf(frame);
	const bool leashed = context.creature.has_value() && context.creature->leashed;
	const auto timeout =
	    Locator::infoConstants::has_value() ? Locator::infoConstants::value().spellSystem.leashSelectionSystemTimeOut : 25.0f;
	_picker.Update(frame.seconds, timeout, leashed);
	context.pickerOpen = _picker.open;
	StepSelection(context, frame);
	_requests = gesture::Requests(context);
	RefreshHints();
	// The chain follows the hand while it powers up the miracle in it, holds one that a circle sizes, or draws the
	// gestures of the open selection. Once a miracle did its first turn in the hand on this land, or the selection
	// opened on this land, the chain follows the hand while it holds any miracle.
	_gesturing = _selection.IsOpen() || gesture::ChainFollows(context.seed, _seedTurnedOnLand);

	// Moving the camera wipes the path being drawn, though not while the camera shakes. The fight view that shows a
	// fight in its arena, and the camera that follows the creature, let the player draw on while they move.
	const bool cameraMoved = _lastCameraEye.has_value() && *_lastCameraEye != frame.view.cameraEye;
	_lastCameraEye = frame.view.cameraEye;
	if (cameraMoved && !frame.cameraShaking && !frame.cameraLetsHandDraw)
	{
		_recorder.Reset();
	}

	// While the hand directs the fight, the held Move button wipes the path, and no gesture is recognised
	if (frame.moveHeldInFight)
	{
		ForgetPath();
		return;
	}

	// The hand's path is only drawn over the world; a path drawn for the testbed goes on regardless
	if (!frame.overWorld && _drawing.empty())
	{
		ForgetPath();
		return;
	}

	// Sampled at the steps of the input, not at the frames. A sample between two frames takes the cursor at its place
	// between them.
	for (const auto part : gesture::SamplePoints(_sinceSample, frame.steps))
	{
		SampleCursor(input_steps::Between(lastCursor, frame.cursor, part), frame, context);
	}
}

void GestureSystem::SampleCursor(glm::vec2 cursor, const Frame& frame, const gesture::HandContext& context)
{
	const auto toReference = frame.view.screenSize.y > 0.0f ? gesture::k_ReferenceHeight / frame.view.screenSize.y : 1.0f;
	glm::vec2 point = cursor * toReference;
	if (!_drawing.empty())
	{
		point = _drawing.front();
		_drawing.pop_front();
	}
	if (_pause > 0.0f)
	{
		return;
	}
	const auto land = frame.view.landAt ? frame.view.landAt(point / toReference) : std::nullopt;
	if (land.has_value())
	{
		_recorder.AddPoint(point, *land);
	}
	else
	{
		_recorder.AddPoint(point);
	}

	// The first gesture waited for that the path draws is acted on
	const auto templates = GetTemplates();
	if (templates.empty() || _requests.empty())
	{
		return;
	}
	const auto keys = _recorder.KeyPoints();
	auto requests = _requests;
	size_t next = 0;
	while (next < requests.size())
	{
		const auto request = requests.at(next++);
		const auto match = gesture::Recognise(templates, request.gesture, keys, _screenAspect);
		if (!match.has_value())
		{
			continue;
		}
		// The help counts some gestures, from what the hand held as the player drew them
		if (const auto event = gesture::HelpEventOf(request, context); event.has_value())
		{
			Help(*event);
		}
		if (Act(request, *match, frame))
		{
			break;
		}
		// A gesture of the selection that matched no miracle does nothing. The selection waits for no more of its
		// gestures this frame, and the hand tries the other gestures as it is now.
		std::vector<gesture::Request> rest;
		for (const auto& other : gesture::Requests(ContextOf(frame)))
		{
			if (other.purpose != Purpose::SelectMiracle && other.purpose != Purpose::CloseSelection)
			{
				rest.push_back(other);
			}
		}
		requests = std::move(rest);
		next = 0;
	}
}

void GestureSystem::StepSelection(gesture::HandContext& context, const Frame& frame)
{
	if (!_selection.IsOpen())
	{
		return;
	}
	// The selection does its step only while the hand draws over the world, and not while the creature takes fight
	// moves or the hand powers up its miracle
	const bool fightMoves = context.creature.has_value() && context.creature->takesFightMoves;
	const bool poweringUp = context.seed.has_value() && context.seed->ready && !context.seed->cast && context.seed->canPowerUp;
	if (frame.moveHeldInFight || (!frame.overWorld && _drawing.empty()) || fightMoves || poweringUp)
	{
		return;
	}
	// Without space in the hand, the selection closes
	if (!context.handReady)
	{
		_selection.Close();
		context.selection.reset();
		return;
	}
	// Time passes. When the time is up, the selection still waits for its gestures this frame, but the other gestures
	// act as if it were closed.
	const float timeout =
	    Locator::infoConstants::has_value() ? Locator::infoConstants::value().spellSystem.selectionSystemTimeOut : 30.0f;
	_selection.Update(frame.seconds, timeout);
	if (context.selection.has_value())
	{
		context.selection->open = _selection.IsOpen();
		// With no gesture to wait for, the selection closes and does nothing more this frame
		if (context.selection->waiting.empty())
		{
			_selection.Close();
			context.selection.reset();
		}
	}
}

bool GestureSystem::OpenSelection(GestureType opening, PlayerNames player)
{
	if (!Locator::spellIconSystem::has_value() || !Locator::infoConstants::has_value() || opening == GestureType::None)
	{
		return false;
	}
	const auto& info = Locator::infoConstants::value();
	std::vector<gesture::SelectionOffer> offers;
	for (const auto seed : Locator::spellIconSystem::value().SeedsForSelection(player, opening))
	{
		const auto& seedInfo = magic::GetSpellSeedInfo(info, seed);
		offers.push_back({.seed = seed, .chain = {seedInfo.selectionGesture, seedInfo.gesture, seedInfo.gestureStage2}});
	}
	if (!_selection.Open(opening, offers))
	{
		return false;
	}
	for (const auto& offer : offers)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The miracle selection offers seed {} for gesture {}",
		                   static_cast<int>(offer.seed), gesture::Name(offer.chain.at(1)));
	}
	// The selection closes the leash picker. From now on this land, the chain also follows every miracle in the hand.
	_picker.Close();
	_seedTurnedOnLand = true;
	return true;
}

bool GestureSystem::OpenSelectionByKey(PlayerNames player)
{
	const bool handReady = Locator::spellIconSystem::has_value() && Locator::spellIconSystem::value().IsHandReady(player);
	if (_selection.IsOpen() && !handReady)
	{
		_selection.Close();
	}
	if (_selection.IsOpen() || !handReady || !Locator::infoConstants::has_value())
	{
		return false;
	}
	return OpenSelection(Locator::infoConstants::value().spellSystem.selectionSystemGestureNonCreature, player);
}

bool GestureSystem::RepeatMiracleByKey(PlayerNames player)
{
	const bool handReady = Locator::spellIconSystem::has_value() && Locator::spellIconSystem::value().IsHandReady(player);
	if (_selection.IsOpen() && !handReady)
	{
		_selection.Close();
	}
	if (_selection.IsOpen() || !handReady || !Requestable(player, LastSeedGained(player)))
	{
		return false;
	}
	// No help and no trail, as for the R gesture
	SendRequest(player_commands::MiracleRepeat {});
	return true;
}

void GestureSystem::LayTrail(GestureType gesture, const gesture::Match& match, const Frame& frame) const
{
	if (!Locator::particleSystem::has_value() || !frame.view.trailPointUnder)
	{
		return;
	}
	const auto points = gesture::TrailLandPoints(_recorder);
	const auto symbol = TrailSymbolOf(gesture);
	if (points.size() < gesture::k_RecognitionShownPoints || !symbol.has_value())
	{
		return;
	}
	// The path's box in the screen's pixels
	const auto toPixels = frame.view.screenSize.y > 0.0f ? frame.view.screenSize.y / gesture::k_ReferenceHeight : 1.0f;
	auto box = gesture::GestureBox(_recorder, match);
	box.min *= toPixels;
	box.max *= toPixels;
	auto trail = gesture::BuildTrail(points, box, *symbol,
	                                 {.cameraForward = frame.view.cameraForward, .pointUnder = frame.view.trailPointUnder});
	if (trail.has_value())
	{
		Locator::particleSystem::value().AddGestureTrail(std::make_shared<particles::GestureTrail>(std::move(*trail)));
	}
}

void GestureSystem::ShowGestureOnLand(GestureType gesture, glm::vec3 corner, float size)
{
	if (gesture == GestureType::None || !Locator::particleSystem::has_value())
	{
		return;
	}
	const auto symbol = TrailSymbolOf(gesture);
	if (!symbol.has_value())
	{
		return;
	}
	const auto landHeight = [](float x, float z) {
		return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt({x, z}) : 0.0f;
	};
	Locator::particleSystem::value().AddGestureTrail(
	    std::make_shared<particles::GestureTrail>(gesture::LandGestureTrail(*symbol, corner, size, landHeight)));
}

bool GestureSystem::Act(const gesture::Request& request, const gesture::Match& match, const Frame& frame)
{
	const auto box = gesture::GestureBox(_recorder, match);
	const auto handAt = _recorder.Empty() ? glm::vec3(0.0f) : _recorder.At(_recorder.Count() - 1).world;
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Gesture recognised: {} ({}), template {}{}", gesture::Name(request.gesture),
	                   gesture::Name(request.purpose), match.templateIndex, match.mirrored ? " mirrored" : "");
	_lastRecognised = Recognised {.request = request, .match = match, .box = box, .number = ++_recognisedCount};

	// The gestures of the selection act first: what they do decides whether the trail shows and the hand rests
	bool shown = gesture::ShowsRecognition(request.purpose);
	bool forgets = true;
	std::optional<player_commands::Action> requested;
	switch (request.purpose)
	{
	case Purpose::OpenSelection:
		// A selection that has nothing to offer stays closed, and the hand keeps its path
		if (OpenSelection(request.gesture, frame.player))
		{
			Help(help::profile::k_GestureOpenSelection);
		}
		else
		{
			shown = false;
			forgets = false;
		}
		break;
	case Purpose::SelectMiracle:
	{
		const auto outcome =
		    _selection.Recognised(request.gesture, [&frame](SpellSeedType seed) { return Requestable(frame.player, seed); });
		if (outcome.kind == gesture::MiracleSelection::Outcome::Kind::Unmatched)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Gesture {} matched no miracle of the selection",
			                   gesture::Name(request.gesture));
			_requests = gesture::Requests(ContextOf(frame));
			return false;
		}
		Help(help::profile::k_GestureSelectionStep);
		if (outcome.kind == gesture::MiracleSelection::Outcome::Kind::Chosen)
		{
			Help(help::profile::k_GestureSelectionChosen);
			requested = player_commands::MiracleRequest {.seed = outcome.seed};
		}
		break;
	}
	case Purpose::CloseSelection:
		ShakeHand();
		_selection.Close();
		Help(help::profile::k_GestureCloseSelection);
		break;
	case Purpose::RepeatMiracle:
		Help(help::profile::k_GestureRepeatMiracle);
		requested = player_commands::MiracleRepeat {};
		break;
	default:
		break;
	}

	// Every gesture but a scribble leaves its trail on the land, heard as it appears, once enough of its path lies on
	// the land
	if (shown)
	{
		LayTrail(request.gesture, match, frame);
	}

	// The request goes as a command. At the turn, the icon of the miracle gives it, or starts to charge it, if it
	// still may.
	if (requested.has_value())
	{
		SendRequest(std::move(*requested));
	}

	auto* leashes = Locator::leashSystem::has_value() ? &Locator::leashSystem::value() : nullptr;
	// Set apart from its declaration: GCC on ARM otherwise takes the optional for possibly uninitialised
	std::optional<entt::entity> creature = std::nullopt;
	if (leashes != nullptr)
	{
		creature = leashes->PlayersCreature(frame.player);
	}
	const auto eventsBefore = _events.size();
	switch (request.purpose)
	{
	case Purpose::SizeCircle:
	{
		// The circle's middle on the land, and its size across the land at that depth
		const auto toPixels = frame.view.screenSize.y > 0.0f ? frame.view.screenSize.y / gesture::k_ReferenceHeight : 1.0f;
		const auto middle = frame.view.landAt ? frame.view.landAt(box.Centre() * toPixels) : std::nullopt;
		const auto centre = middle.value_or(_recorder.At(_recorder.PointsOfKeys(match.firstKey, match.lastKey).first).world);
		const auto edge = frame.view.atDepthOf ? frame.view.atDepthOf(gesture::CircleEdge(box) * toPixels, centre) : centre;
		_events.push_back({.kind = GestureEvent::Kind::Circle,
		                   .gesture = request.gesture,
		                   .centre = centre,
		                   .radius = gesture::CircleRadius(centre, edge, frame.view.cameraRight)});
		_circleSeconds = gesture::k_CircleSeconds;
		_circleSeed =
		    Locator::magicSystem::has_value() ? Locator::magicSystem::value().GetHeldSeed().value_or(entt::null) : entt::null;
		break;
	}
	case Purpose::PowerUp:
		_events.push_back({.kind = GestureEvent::Kind::PowerUp,
		                   .gesture = request.gesture,
		                   .centre = handAt,
		                   .powerUpLevel = request.powerUpLevel});
		break;
	case Purpose::DropSeed:
		_events.push_back({.kind = GestureEvent::Kind::Scribble, .gesture = request.gesture, .centre = handAt});
		break;
	case Purpose::ShakeOffLeash:
		ShakeHand();
		if (leashes != nullptr)
		{
			leashes->Shake(frame.player);
		}
		break;
	case Purpose::ShakeOffHeld:
		// The hand shakes and lets go of it where it is, never planted again
		ShakeHand();
		if (Locator::handGrabSystem::has_value())
		{
			Locator::handGrabSystem::value().ForceDrop();
		}
		break;
	case Purpose::LeashGesture:
		if (creature.has_value())
		{
			// An unleashed creature has the leash put on; knowing more than one, it may then be changed
			if (!leashes->IsLeashed(*creature))
			{
				leashes->Toggle(*creature);
			}
			int known = 0;
			for (const auto type : creature_leash::k_Types)
			{
				known += leashes->Knows(*creature, type) ? 1 : 0;
			}
			if (known > 1 && leashes->IsLeashed(*creature))
			{
				_picker.Open();
			}
		}
		break;
	case Purpose::PickLeash:
		if (creature.has_value())
		{
			leashes->ChangeType(*creature, request.leash);
		}
		_picker.Close();
		break;
	case Purpose::ClosePicker:
		ShakeHand();
		_picker.Close();
		break;
	case Purpose::FightSpecialMove:
		if (Locator::creatureFightSystem::has_value())
		{
			Locator::creatureFightSystem::value().GestureSpecialMove();
		}
		break;
	case Purpose::FightMiracle:
		if (Locator::creatureFightSystem::has_value())
		{
			Locator::creatureFightSystem::value().GestureSpell(request.magic);
		}
		break;
	case Purpose::HiddenScript:
		// Each square wave starts the script again. The script itself finds the phone box that it made before.
		if (Locator::vm::has_value())
		{
			Locator::vm::value().StartScript(std::string(gesture::k_HiddenScript), script::start::StartableScripts());
		}
		break;
	case Purpose::OpenSelection:
	case Purpose::SelectMiracle:
	case Purpose::CloseSelection:
	case Purpose::RepeatMiracle:
		break;
	}

	if (_events.size() > eventsBefore)
	{
		_lastRecognised->event = _events.back();
		const auto& event = _events.back();
		SPDLOG_LOGGER_INFO(
		    spdlog::get("game"), "Gesture event for the miracles: kind {}, at {:.1f}, {:.1f}, {:.1f}, radius {:.1f}, level {}",
		    static_cast<int>(event.kind), event.centre.x, event.centre.y, event.centre.z, event.radius, event.powerUpLevel);
	}

	// The hand forgets the path. After most gestures, the hand also stops for a short time.
	if (forgets)
	{
		ForgetPath();
		if (gesture::RestsAfter(request.purpose))
		{
			_pause = gesture::k_RecognisedPauseSeconds;
		}
	}
	_requests = gesture::Requests(ContextOf(frame));
	return true;
}

void GestureSystem::RefreshHints()
{
	if (!gesture_hints::Differs(_hints.hints, gesture_hints::HintsFor(_requests, nullptr)))
	{
		return;
	}
	const auto shapeOf = [](GestureType gesture) -> std::optional<gesture_hints::Shape> {
		const auto symbol = TrailSymbolOf(gesture);
		if (!symbol.has_value())
		{
			return std::nullopt;
		}
		return gesture_hints::Shape {.points = symbol->points, .length = symbol->length};
	};
	_hints.hints = gesture_hints::HintsFor(_requests, shapeOf);
}

void GestureSystem::StepHints(float seconds, bool wordsShowing)
{
	gesture_hints::Step(_hints, seconds, wordsShowing);
}
