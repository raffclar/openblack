/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "HandSystem.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <fstream>
#include <tuple>
#include <utility>

#include <L3DFile.h>
#include <LNDFile.h>
#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <spdlog/spdlog.h>

#include "3D/AllMeshes.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandIslandInterface.h"
#include "Camera/Camera.h"
#include "Camera/CameraModel.h"
#include "Common/HelpText.h"
#include "Debug/DebugEnv.h"
#include "ECS/Abodes.h"
#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Archetypes/HandArchetype.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/LandscapeVortex.h"
#include "ECS/Components/MagicTeleport.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/NotDrawn.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/HandPressChain.h"
#include "ECS/Influence/Influence.h"
#include "ECS/ObjectFlags.h"
#include "ECS/ObjectResources.h"
#include "ECS/Physics/FromHand.h"
#include "ECS/Physics/ParticleCarriedObjects.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/Rocks.h"
#include "ECS/ScriptHighlight.h"
#include "ECS/SeaCells.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "ECS/Systems/HandTap.h"
#include "ECS/ToBeDeleted.h"
#include "FileSystem/FileSystemInterface.h"
#include "Game.h"
#include "GameClock.h"
#include "Graphics/Texture2D.h"
#include "HandSystemDetail.h"
#include "Help/HelpProfile.h"
#include "Help/ToolTips.h"
#include "InfoConstants.h"
#include "Input/GamePackets.h"
#include "Input/HandDemo.h"
#include "Input/InterfaceActive.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Windowing/WindowingInterface.h"
#include "Worship/Citadel.h"
#include "Worship/Worship.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace openblack::ecs::systems::hand_detail
{
void PlaySample(audio::SoundId id)
{
	// a 2D sample with no owner, not tracked, mode 3, no loops
	audio::PlayOptions options;
	options.sound = static_cast<entt::id_type>(id);
	options.track = false;
	options.mode = 3;
	options.loops = 0;
	audio::PlaySoundEffect(options);
}

entt::entity PlaySample3D(audio::SoundId id, glm::vec3 point)
{
	audio::PlayOptions options;
	options.sound = static_cast<entt::id_type>(id);
	options.is3D = true;
	options.track = false;
	options.position = point;
	return audio::sample_play::AsEntity(audio::PlaySoundEffect(options));
}

/// The landscape cell under the point does not have the water bit (off the map or without a block: not land)
bool IsLand(glm::vec3 point)
{
	return sea_cells::IsLand(point);
}
} // namespace openblack::ecs::systems::hand_detail

using namespace openblack::ecs::systems::hand_detail;

namespace
{
/// What the hand keeps for its test hooks between frames, in the debug hooks' store (Locator::debugHooks)
struct HandSystemDebugHooksState
{
	bool debugHooksRan {false}; // the environment test hooks have run (once, on the first land)
};

HandSystemDebugHooksState& HandSystemDebugHooksData()
{
	if (!Locator::debugHooks::has_value())
	{
		std::fputs("ecs::systems::HandSystem: no debug hooks in the locator (Locator::debugHooks)\n", stderr);
		std::abort();
	}
	return Locator::debugHooks::value().Get<HandSystemDebugHooksState>();
}
} // namespace

bool HandSystem::OfferScreenObject(uint32_t id, float depth) noexcept
{
	// while gripping a screen object every offer is accepted, the pending one unchanged; otherwise the nearer offer
	// wins, the pending depth starting above every offer when it is empty
	if (_screenGrip)
	{
		return true;
	}
	if (_screenOffer && !(_screenOfferDepth > depth))
	{
		return false;
	}
	_screenOffer = id;
	_screenOfferDepth = depth;
	return true;
}

bool HandSystem::Initialize() noexcept
{
	// one hand per player; the original's is mirrored by its left/right toggle at load (pending: not mirrored here);
	// its left-handed mode toggles the same hand (pending). Side::Right stays entt::null
	_hands[static_cast<size_t>(Side::Left)] =
	    HandArchetype::Create(glm::vec3(0.0f), glm::half_pi<float>(), 0.0f, glm::half_pi<float>(), 0.01f, false);

	LoadAnimations();
	LoadMorphMeshes();
	RegisterPhysicsHandlers();
	RegisterTapHandlers();
	RegisterPacketHandlers();
	// the normal state's entry: the up Zoomer at (0, 1, 0)
	_up.SetPosition(glm::vec3(0.0f, 1.0f, 0.0f));
	// the tooltip builder asks the hand's state for its tooltip every turn
	help::tooltips::SetStateSubmitter([this]() { SubmitToolTips(); });
	return false;
}

void HandSystem::RegisterTapHandlers() noexcept
{
	// rocks: tappable when taller than 0.7; a tap splits it in two
	hand_tap::Register(
	    &Rocks::IsRock, [](entt::entity rock, const pot_resource::Dropper&) { return Rocks::ValidToTap(rock); },
	    [](entt::entity rock, const pot_resource::Dropper&, glm::vec3 handPos) -> uint32_t {
		    Rocks::Tap(rock, handPos);
		    return 1;
	    });
	// abodes: always tappable; a tap knocks on the roof
	hand_tap::Register<Abode>(
	    [](entt::entity abode, const pot_resource::Dropper&) { return abodes::InterfaceValidToTap(abode); },
	    [](entt::entity abode, const pot_resource::Dropper& is, glm::vec3 handPos) -> uint32_t {
		    // the hand's own position for the knock, the status's point for the sound; TapFn is a plain function
		    // pointer, so the hand comes from the hand service instead of a capture
		    const auto hand = Locator::handSystem::value().GetPlayerHands()[static_cast<size_t>(Side::Left)];
		    const auto& handTransform = Locator::entitiesRegistry::value().Get<const Transform>(hand);
		    abodes::InterfaceTap(abode, handPos, is.isMyInterface, handTransform.position);
		    return 1;
	    });
	// spell icons and one-shot spell seeds
	const auto worshipValid = [](entt::entity object, const pot_resource::Dropper& is) {
		return worship::InterfaceValidToTap(object, is.player);
	};
	const auto worshipTap = [](entt::entity object, const pot_resource::Dropper& is, glm::vec3) -> uint32_t {
		return static_cast<uint32_t>(worship::InterfaceTap(object, is.player));
	};
	hand_tap::Register<SpellIcon>(worshipValid, worshipTap);
	hand_tap::Register<OneOffSpellSeed>(worshipValid, worshipTap);
	// a "Did you know?" scroll; the tapping status is the local one when it is the local interface. It is the one class
	// tapped outside the player's influence too
	hand_tap::Register<ScriptHighlight>(
	    [](entt::entity thing, const pot_resource::Dropper&) { return ecs::script_highlight::InterfaceValidToTap(thing); },
	    [](entt::entity thing, const pot_resource::Dropper& is, glm::vec3) -> uint32_t {
		    return static_cast<uint32_t>(ecs::script_highlight::InterfaceTap(thing, is.isMyInterface));
	    },
	    false);
}

bool HandSystem::InInfluence() const noexcept
{
	// the interface's in-influence flag: the player's influence at the action point (interface type, allies) > 0
	return _interactionPoint.has_value() && influence::CalculatePlayerInfluence(PlayerNames::PLAYER_ONE, *_interactionPoint,
	                                                                            influence::CalcType::Interface) > 0.0f;
}

bool HandSystem::ValidForPlaceInHand(entt::entity object) const noexcept
{
	// a rock when its 2D radius <= 3.6; a spell icon never (a one-shot orb always)
	if (Rocks::IsRock(object))
	{
		return Rocks::ValidForPlaceInHand(object);
	}
	auto& registry = Locator::entitiesRegistry::value();
	// a spell seed and a magic teleport by their seed; a big forest always
	if (registry.AnyOf<SpellSeed, MagicTeleport>(object))
	{
		return SeedToPlaceInHand(object) != entt::null;
	}
	// a temple's entrance and a script highlight never (they keep an object's default)
	return !registry.AnyOf<SpellIcon, CitadelEntrance, ScriptHighlight>(object);
}

bool HandSystem::SendTap(entt::entity object) noexcept
{
	// sending a tap: (in the influence || the object does not need it) && valid to tap && not flagged cannot be picked
	// up -> a tap packet, whose handler checks validity again and taps. Every ported class needs the influence but the
	// script highlight. The cannot-be-picked-up flag is SET_ID_PICKUPABLE 169's.
	// The tap first remembers the object ((not ported) not a reward under the leash), whatever happens next
	RememberTapped(object);
	const auto is = InterfaceStatus();
	if (!hand_tap::SendsTap(InInfluence(), hand_tap::NeedsInfluence(object), hand_tap::ValidToTap(object, is),
	                        object_flags::IsCannotBePickedUp(object)))
	{
		return false;
	}
	// the tap packet: applied at the next turn's start (HandTurn.cpp ApplyTap)
	game_packets::Push({game_packets::Type::Tap, object});
	return true;
}

std::array<entt::entity, static_cast<size_t>(HandSystemInterface::Side::_Count)> HandSystem::GetPlayerHands() const noexcept
{
	return _hands;
}

std::array<std::optional<glm::vec3>, static_cast<size_t>(HandSystemInterface::Side::_Count)>
HandSystem::GetPlayerHandPositions() const noexcept
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto hands = GetPlayerHands();
	std::array<std::optional<glm::vec3>, static_cast<size_t>(Side::_Count)> result {};
	for (size_t i = 0; i < hands.size(); ++i)
	{
		if (registry.Valid(hands[i]) && registry.AllOf<Transform>(hands[i]))
		{
			result[i] = registry.Get<Transform>(hands[i]).position;
		}
	}
	// TODO(#693): Hand Getter should return an optional if the hand doesn't have a valid position
	// When the position is zero, it probably means it's not on the map (e.g. mouse is in the sky)
	if (result[static_cast<size_t>(Side::Left)] == glm::zero<glm::vec3>())
	{
		result[static_cast<size_t>(Side::Left)] = std::nullopt;
	}
	if (result[static_cast<size_t>(Side::Right)] == glm::zero<glm::vec3>())
	{
		result[static_cast<size_t>(Side::Right)] = std::nullopt;
	}
	return result;
}

void HandSystem::LoadAnimations() noexcept
{
	auto logger = spdlog::get("game");
	try
	{
		auto& fileSystem = Locator::filesystem::value();
		const auto& mesh = Locator::resources::value().GetMeshes().Handle(Hand::k_MeshId);
		if (!mesh || !mesh->IsBoned())
		{
			SPDLOG_LOGGER_WARN(logger, "Hand mesh is not loaded or has no bones: hand stays in bind pose");
			return;
		}
		const auto hbnPath = fileSystem.GetPath<filesystem::Path::Data>() / "CTR" / "hh.HBN";
		const auto specsDirectory = fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Data>());
		auto animator = std::make_unique<HandAnimator>();
		if (animator->Load(resources::LoadBlob(Locator::resources::value().GetBlobs(), hbnPath), specsDirectory,
		                   mesh->GetBoneMatrices(), mesh->GetBoneParents()))
		{
			_animator = std::move(animator);
			LoadGeometry();
		}
		static const debug_env::Variable k_HandInfo("OPENBLACK_HAND_INFO");
		if (k_HandInfo.Get() != nullptr)
		{
			const auto& pots = Locator::infoConstants::value().pot;
			for (size_t i = 0; i < pots.size(); ++i)
			{
				const auto& p = pots[i];
				SPDLOG_LOGGER_INFO(logger,
				                   "PotInfo {}: potType={} resource={} maxInPot={} next={} initial={} perTurn={} perTurnEnd={} "
				                   "maxPick={} ramp={} mesh={}",
				                   i, static_cast<int>(p.potType), static_cast<int>(p.resourceType), p.maxAmountInPot,
				                   static_cast<int>(p.nextPotForResource), p.amountPickedUpInitially, p.amountPickedUpPerTurn,
				                   p.amountPickedUpPerTurnEnd, p.maxAmountCanBePickedUp, p.multiPickUpRampTime,
				                   static_cast<int>(p.meshId));
			}
			const auto& trees = Locator::infoConstants::value().tree;
			for (size_t i = 0; i < trees.size(); ++i)
			{
				const auto& t = trees[i];
				SPDLOG_LOGGER_INFO(
				    logger,
				    "TreeInfo {} '{}': mesh={} growing={} burning={} strength={} defence={} startLife={} wood={} food={} "
				    "weight={} carried={} minSize={} maxSize={} grows={} growth={} immersion={} collide={} "
				    "helpInHand={} maxTrees={}",
				    i, t.debugString.data(), static_cast<int>(t.normal), static_cast<int>(t.growing),
				    static_cast<int>(t.burning), t.strength, t.defence, t.startLife, t.woodValue, t.foodValue, t.weight,
				    static_cast<int>(t.carriedType), t.minSize, t.maxSize, t.growsAfterNumGameTurns, t.growthAmount,
				    static_cast<int>(t.immersion), static_cast<int>(t.collideSound), static_cast<int>(t.helpInHand),
				    t.maxNumTreesCanProduce);
			}
		}
		// Debug: OPENBLACK_HAND_ANIM=<C node> forces an animation (e.g. Cgrip) instead of the gameplay state.
		static const debug_env::Variable k_HandAnim("OPENBLACK_HAND_ANIM");
		if (const char* forced = k_HandAnim.Get(); forced != nullptr && _animator)
		{
			_override = forced;
		}
		// Debug: OPENBLACK_HAND_DUMP=<file> writes the evaluated bone matrices of every clip for validation.
		static const debug_env::Variable k_HandDump("OPENBLACK_HAND_DUMP");
		if (const char* dump = k_HandDump.Get(); dump != nullptr && _animator)
		{
			std::ofstream out(dump);
			out << "{";
			bool firstClip = true;
			for (const auto& clip : _animator->ListClips())
			{
				for (const auto& [t, lr, fb] : {std::tuple {0.0f, 0.0f, 0.0f}, {137.0f, 0.0f, 0.0f}, {0.0f, 0.6f, -0.4f}})
				{
					const auto key = fmt::format("{}@{}@{}@{}", clip.name, t, lr, fb);
					out << (firstClip ? "" : ",") << "\n\"" << key << "\":[";
					firstClip = false;
					const auto mats = _animator->Evaluate(clip.name, t, lr, fb, clip.name == "Cgrip" ? 1.0f : 0.0f);
					for (size_t b = 0; b < mats.size(); ++b)
					{
						const float* v = glm::value_ptr(mats[b]);
						out << (b ? "," : "") << "[";
						for (int k = 0; k < 16; ++k)
						{
							out << (k ? "," : "") << v[k];
						}
						out << "]";
					}
					out << "]";
				}
			}
			out << "\n}\n";
			SPDLOG_LOGGER_INFO(logger, "Hand animation dump written to {}", dump);
		}
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_ERROR(logger, "Failed to load hand animations: {}", e.what());
	}
}

void HandSystem::Update(std::chrono::microseconds dt, glm::vec2 mouseDelta, bool gripping, bool actionHeld) noexcept
{
	if (!_animator)
	{
		return;
	}
	const float seconds = static_cast<float>(dt.count()) / 1e6f;
	// the hand's draw preparation runs before its state machine: the good / evil morph
	UpdateMorphing();

	if (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f)
	{
		// the hand-move help event, from a mouse move with a pixel delta
		help_profile::Trigger(help_profile::Event::HandMove);
	}
	UpdateGripDust(seconds);
	// mouse smoothing: smooth += dt * vel, clamped to +-80 px of the mouse, vel = (vel + (mouse - smooth) * 20 dt)
	// * 0.03^dt. The mouse is in the game's pixels (ints): the window's pixels here, so the lean depends on the
	// resolution as in the original
	_mouse += mouseDelta; // (approximate) the sum of the deltas stands for the mouse's absolute position
	if (_handHidden)
	{
		// the draw preparation stops after the state switch while hidden: the smoothing does not run
	}
	else if (_smoothMouseValid && seconds > 0.0f)
	{
		_smoothMouse = glm::clamp(_smoothMouse + seconds * _smoothMouseVelocity, _mouse - 80.0f, _mouse + 80.0f);
		_smoothMouseVelocity = (_smoothMouseVelocity + (_mouse - _smoothMouse) * 20.0f * seconds) * std::pow(0.03f, seconds);
	}
	else
	{
		_smoothMouse = _mouse;
		_smoothMouseVelocity = glm::vec2(0.0f);
		_smoothMouseValid = true;
	}
	// The L lean poses: the _lr pose at (lag + 80) / 160 x its duration (truncated) with lag = clamp(smoothed x - mouse x,
	// -80, 80), only when |lag| > 0.0001; the _fb pose the same with clamp(mouse y - smoothed y), only when |smoothed y|
	// > 0.0001; each the difference from its middle frame ((count - 1) >> 1). In the camera state the sideways lag is the
	// other way (mouse x - smoothed x); (inferred) openblack's land grip stands for that state (the CAMERA state is
	// (pending)). (approximate) HandAnimator samples the frame at (v + 1) / 2 of the clip
	// instead of the ms time; skipping a pose is the same as its middle frame (no difference)
	const auto clampLag = [](float v) { return v > -80.0f ? (v < 80.0f ? v : 80.0f) : -80.0f; };
	const float lagX = clampLag(gripping ? _mouse.x - _smoothMouse.x : _smoothMouse.x - _mouse.x);
	const float lagY = clampLag(_mouse.y - _smoothMouse.y);
	_motion.x = std::abs(lagX) > 0.0001f ? lagX / 80.0f : 0.0f;
	_motion.y = std::abs(_smoothMouse.y) > 0.0001f ? lagY / 80.0f : 0.0f;
	_animator->SetMotion(_motion.x, _motion.y);

	// Test hooks driven by environment variables, once the landscape exists (HandDebugHooks.cpp).
	if (auto& ran = HandSystemDebugHooksData().debugHooksRan; !ran && Locator::terrainSystem::has_value())
	{
		ran = true;
		RunDebugHooks();
	}
	hand_detail::DumpEntityCounts();
	if (_testActionSeconds > 0.0f)
	{
		actionHeld = true;
		_testActionSeconds -= seconds;
	}
	// OPENBLACK_TEST_CAST: the synthetic presses of the action button (HandSpellSeed.cpp)
	actionHeld = TestCastActionHeld(seconds, actionHeld);

	// Pick up / drop with the action button (right). Only while not gripping the land.
	// a destroyed held object (openblack destroys at once while the deferred deletion is off) leaves the hand here; a
	// deleted one (Unavailable) at the turn's hand validation. (pending) the hand's game turn update also drops it the
	// same turn (not available -> thrown), for a deletion after step I
	if (_held && !Locator::entitiesRegistry::value().Valid(*_held))
	{
		_held.reset();
	}
	// the pending screen object becomes this frame's; then, unless gripping, the pending one is cleared for this frame's
	// offers. Before that, the offer is arbitrated (skipped while gripping): the object under the cursor against the
	// bubble, the nearer wins, then a land hit nearer than the bubble (the land's distance + 2.3) drops it. The bubble's
	// depth is its camera z, compared as it is with the ray distances, as the original does. (not ported) the second
	// land test
	bool screenWins = false;
	if (_screenOffer && !_screenGrip)
	{
		const bool objectNearer = _cursorObjectDistance && !(_screenOfferDepth < *_cursorObjectDistance);
		const bool landNearer = _cursorLandDistance && *_cursorLandDistance + 2.3f < _screenOfferDepth;
		if (objectNearer || landNearer)
		{
			_screenOffer.reset();
		}
		else
		{
			screenWins = true;
		}
	}
	_screenObject = _screenOffer;
	_screenObjectDepth = _screenOfferDepth;
	if (!_screenGrip)
	{
		_screenOffer.reset();
	}
	if (screenWins || _screenGrip)
	{
		_cursorObject.reset(); // the bubble is the collide, not the object
	}
	_hovered = (_held || _tug || gripping || _screenObject) ? std::nullopt : FindObjectUnderHand();
	const bool actionPressed = actionHeld && !_actionWasHeld;
	// the action press: with nothing collided, the object near the action's point becomes the collided object, and the
	// branches below test it. (approximate) the hover's class filter (FindObjectUnderHand) stands for the pick-up / tap
	// tests and the locked select's is the field / pile branches'; (inferred) every ported class is interactable. A fish
	// farm goes to the fish branch through the action's point
	std::optional<entt::entity> nearObject;
	if (actionPressed && !_held && !_tug && !gripping && !_hovered && !_cursorObject && _interactionPoint)
	{
		nearObject = FindObjectNearMapCoord(*_interactionPoint);
		if (nearObject)
		{
			_cursorObject = nearObject;
			_hovered = FindObjectUnderHand();
		}
	}
	// the creature under the hand this frame, as the hover and the press see it (Creature Mode reads it)
	if (Locator::creatureHandSystem::has_value())
	{
		const auto& registry = std::as_const(Locator::entitiesRegistry::value());
		const bool creature = _cursorObject && ecs::IsAvailable(*_cursorObject) && registry.AllOf<Creature>(*_cursorObject);
		Locator::creatureHandSystem::value().SetCreatureUnderHand(creature ? _cursorObject : std::nullopt);
	}
	const bool actionReleased = !actionHeld && _actionWasHeld;
	_actionWasHeld = actionHeld;
	// an empty hand pressed on a screen object -> the bubble grip action state; the release clears it and resets the
	// action state. (approximate) the original clears the whole flag word; only its grip bit is kept here
	if (_screenGrip && actionReleased)
	{
		_screenGrip = false;
	}
	const bool screenPress = actionPressed && !_held && !_tug && !gripping && _screenObject.has_value();
	if (screenPress)
	{
		_screenGrip = true;
		_screenOffer = _screenObject; // the frozen collide keeps it
		_screenOfferDepth = _screenObjectDepth;
	}
	// before the action states: the last thing tapped or clicked
	UpdateTapMemory(actionReleased);
	// the interface's in-influence flag: every ported class needs it for taps, locked selects and pick-ups
	const auto TapInInfluence = [this]() { return InInfluence(); };
	// the press takes the first branch that holds, in the hand's order (ECS/HandPressChain.h). Every fact is a read that
	// changes nothing; the ones that matter only with an empty hand over nothing are read only then, in that order
	auto branch = hand_press::Branch::None;
	if (actionPressed)
	{
		hand_press::Facts facts {
		    .screenPress = screenPress,
		    .held = _held.has_value(),
		    .hovered = _hovered.has_value(),
		    .hoveredIsField = _hovered && Locator::entitiesRegistry::value().AllOf<Field>(*_hovered),
		    .gripping = gripping,
		    .holdingSeed = IsHoldingSeed(),
		    .pickPressHeld = _pickPressHeld,
		    .creatureLockBusy = _creatureLock.has_value(),
		};
		if (hand_press::EmptyHandOverNothing(facts))
		{
			const bool cursorAvailable = _cursorObject && ecs::IsAvailable(*_cursorObject);
			facts.tapOnlyCursorObject = cursorAvailable && (abodes::InterfaceValidToTap(*_cursorObject) ||
			                                                worship::citadel::IsEntranceValidToTap(*_cursorObject) ||
			                                                ecs::script_highlight::InterfaceValidToTap(*_cursorObject));
			facts.cursorIsCreature = !facts.tapOnlyCursorObject && cursorAvailable &&
			                         std::as_const(Locator::entitiesRegistry::value()).AllOf<Creature>(*_cursorObject);
			facts.creatureTakesPress = facts.cursorIsCreature && CreatureTakesPress(*_cursorObject);
			facts.fishFarmInInfluence = !facts.tapOnlyCursorObject && !facts.cursorIsCreature && _interactionPoint &&
			                            TapInInfluence() && FishFarmUnderHand(*_interactionPoint).has_value();
		}
		branch = hand_press::Choose(facts);
	}
	switch (branch)
	{
	case hand_press::Branch::None:
		break;
	case hand_press::Branch::ScreenObject:
		// the bubble grip action state has no other work at the press
		break;
	case hand_press::Branch::Field:
	{
		// fields are a locked select: the press starts the scooping at once, in the influence. Out of it the field goes
		// on to the pick-up / tap path, where it is neither placeable nor tappable: nothing.
		_pickPressHeld =
		    TapInInfluence() && !object_flags::IsCannotBePickedUp(*_hovered) && FieldValidForLockedSelect(*_hovered);
		if (_pickPressHeld)
		{
			SendStartLockedSelect(*_hovered);
		}
		break;
	}
	case hand_press::Branch::Hovered:
	{
		// Piles cannot be tapped, so the locked select (scooping) starts at once, in the influence as above; out of it
		// nothing happens (the grab start taps, and a pile is not tappable).
		const auto source = PotInfoOf(*_hovered);
		if (source != PotInfo::_COUNT && source != PotInfo::HandWood && source != PotInfo::HandFood)
		{
			_pickPressHeld = TapInInfluence() && !object_flags::IsCannotBePickedUp(*_hovered);
			if (_pickPressHeld)
			{
				SendStartLockedSelect(*_hovered);
			}
		}
		else if (!ValidForPlaceInHand(*_hovered) || object_flags::IsCannotBePickedUp(*_hovered) || !TapInInfluence())
		{
			// an object that cannot go into the hand (a rock too big to lift, a spell icon, the SET_ID_PICKUPABLE flag)
			// or out of the influence is tapped at once: SendTap (refused out of the influence) -> the tap packet -> the
			// object's tap (a rock splits, a spell icon's own)
			SendTap(*_hovered);
			_hovered.reset();
		}
		else if (Locator::entitiesRegistry::value().AllOf<Tree>(*_hovered) &&
		         !(physics::PhysicsObjects::IsFlying(*_hovered) && !physics::particle_carried_objects::IsCarried(*_hovered)))
		{
			// a tuggable object is picked up with a tug at the press, so the tug starts at once, not after the 225 ms
			// grab threshold (the grab state picks it up once the tug lets it go)
			RenderHandPickUp(*_hovered); // with a tug
			BeginTug(*_hovered);
		}
		else
		{
			// the grab start (in the press's own frame): the frame's engine sample and the turn are kept, grab action
			// state. The object is free when it is in physics and not carried ((approximate) in physics =
			// PhysicsObjects::IsFlying, as VillagerDeath; carried = particle_carried_objects::IsCarried): the 225 ms timer, as
			// a forest. Otherwise RenderHandPickUp holds it with a tug, none for a seed. (not ported) a tuggable object without
			// a 3D object and the magic fireball's 225 ms
			auto& registry = Locator::entitiesRegistry::value();
			const auto entity = *_hovered;
			_pendingPick = entity;
			_pendingPressMs = game_clock::EngineFrameSampleMs();
			_pendingPressTurn = game_clock::Turn();
			_pendingTimer =
			    (physics::PhysicsObjects::IsFlying(entity) && !physics::particle_carried_objects::IsCarried(entity)) ||
			    registry.AllOf<BigForest>(entity);
			_pendingTugHold = !_pendingTimer && !registry.AllOf<SpellSeed>(entity);
			_pendingTugStarted = false;
			if (!registry.AllOf<BigForest>(entity))
			{
				RenderHandPickUp(entity);
			}
		}
		break;
	}
	case hand_press::Branch::TapOnly:
	{
		// the press sends the object under the cursor to the grab start when it can go into the hand or it is only
		// tappable (abodes are always tappable, so FindObjectUnderHand leaves them out: they are never hovered for a
		// pick-up). An abode cannot go into the hand, so it is tapped at once -> SendTap, which needs the hand inside the
		// influence -> the tap packet -> the abode's tap: knocking on the roof. A temple's entrance is the same, valid
		// to tap unless the script has locked it: its tap takes the local player inside its own temple. A "Did you
		// know?" sign is the same, valid to tap once it has a text, in the influence or not: its tap opens its bubble,
		// or closes it when it is that sign's.
		SendTap(*_cursorObject);
		break;
	}
	case hand_press::Branch::Creature:
	{
		// the player's own creature: the hand takes hold of it through a locked select, started at once as a creature
		// can't be tapped. (not ported) with the leash in the hand, the wait for a tap first
		SendStartLockedSelect(*_cursorObject);
		_creatureLock = *_cursorObject;
		_creatureLockState = {};
		_creatureLocked = false;
		break;
	}
	case hand_press::Branch::FishFarm:
	{
		// fish: the locked select starts at the press, like piles, in the influence
		SendStartLockedSelect(*FishFarmUnderHand(*_interactionPoint));
		_pickPressHeld = true;
		break;
	}
	case hand_press::Branch::Seed:
	{
		// a press while holding a spell seed: armed until the release (HAND_GESTURE), cast at once
		// (HAND_POSITION) or kept casting while held (IN_HAND); HandSpellSeed.cpp
		SeedActionPressed();
		break;
	}
	case hand_press::Branch::Held:
	{
		// a press while holding: the object under the hand takes the held one (a villager into a teleport stone:
		// HandApplyToObject.cpp); otherwise press the action button again, move and release to put it down or hurl it
		// (the release state).
		// out of the influence nothing happens
		if (TapInInfluence() && !HeldActionPressedOnObject(true))
		{
			_releaseArmed = true;
		}
		break;
	}
	}
	// the seed's apply states 8..11, and what the gesture system is told about the hand (HandSpellSeed.cpp)
	UpdateSeedAction(actionHeld);
	UpdateSeedInHand(actionHeld);
	if (actionReleased && _pickPressHeld)
	{
		// Releasing the press that picked it up ends the grab / scooping (the end locked select packet); the
		// object stays in the hand.
		_pickPressHeld = false;
		SendEndLockedSelect();
	}
	else if (actionReleased && _held && _releaseArmed)
	{
		_releaseArmed = false;
		// The release sends the holding spring's velocity (units per second, capped at 124) and every release takes the
		// same path: the physics from the hand decides thrown (vel.x^2 + vel.z^2 > 4) or put down, a hand pot |v|^2 <= 5
		// (HandHolding.cpp). Sent as two packets, applied at the next turn's start (HandTurn.cpp)
		SendRelease(_handVelocity);
	}
	// the creature's locked select: held once the turn locks it, let go at the release
	UpdateCreatureLock(actionReleased);
	if (_pendingPick)
	{
		// The tug state's update for the tug hold, in the draw: the TUG state change of the first draw after the press
		// zeroes the state blend with firstFrame set; each Update, firstFrame is cleared once the blend >= 0.13, and the
		// Update after clears the tug hold; then the blend += dt while < 0.13, dt = the camera time step x 0.001 (the
		// real frame time inside the citadel). (not ported) the blend does not advance while the hand is hidden (the
		// draw preparation's early return)
		if (_pendingTugHold)
		{
			if (!_pendingTugStarted)
			{
				_pendingTugStarted = true;
				_pendingTugFirst = true;
				_pendingTugBlend = 0.0f;
			}
			else if (_pendingTugFirst)
			{
				_pendingTugFirst = !(_pendingTugBlend >= 0.13f);
			}
			else
			{
				_pendingTugHold = false;
			}
			if (_pendingTugBlend < 0.13f)
			{
				const auto ms = game_clock::IsInsideCitadel() ? game_clock::FrameRealMs()
				                                              : game_clock::CameraFrameMs(hand_demo::IsPlaying());
				_pendingTugBlend += static_cast<float>(ms) * 0.001f;
			}
		}
		// The grab state, every frame (and every turn): elapsed = min(sample - press sample, (turn + 1) * 100 - press turn *
		// 100); first the pick-up (the timer path at elapsed >= 225 ms; else once the tug hold is over), then the release
		// (every object): stop the immersion, throw it when the hand holds it, and a tap only if elapsed <= 225 ms. A tap
		// does nothing for most objects, splits rocks taller than 0.7 and puts a one-shot orb's charged seed in the hand
		const uint32_t sinceSample = game_clock::EngineFrameSampleMs() - _pendingPressMs;
		const uint32_t sinceTurn = (game_clock::Turn() + 1u - _pendingPressTurn) * 100u;
		const uint32_t elapsed = std::min(sinceSample, sinceTurn);
		const bool pickNow = _pendingTimer ? elapsed >= 0xE1u : !_pendingTugHold;
		if (!ecs::IsAvailable(*_pendingPick))
		{
			_pendingPick.reset();
		}
		else if (!pickNow && !actionHeld)
		{
			RenderHandRelease();
			if (elapsed <= 0xE1u)
			{
				SendTap(*_pendingPick);
			}
			_pendingPick.reset();
		}
		else if (pickNow)
		{
			const auto entity = *_pendingPick;
			_pendingPick.reset();
			if (!TapInInfluence() || object_flags::IsCannotBePickedUp(entity))
			{
				// the generic pick-up of an object that cannot be picked up or out of the influence fails: the grab state
				// resets the action, nothing is picked up
			}
			else if (Locator::entitiesRegistry::value().AllOf<BigForest>(entity))
			{
				// not tuggable: the grab start sets the hold and the timer; after 225 ms the generic pick-up sends the place
				// in hand packet with the forest; its tree is made in the handler
				SendPlaceInHand(entity);
			}
			else if (Locator::entitiesRegistry::value().AllOf<Tree>(entity) &&
			         !(physics::PhysicsObjects::IsFlying(entity) && !physics::particle_carried_objects::IsCarried(entity)))
			{
				// a standing tree is tugged; one in physics (thrown, not landed yet) is caught like any flying object, unless
				// a particle system carries it (not free)
				BeginTug(entity);
			}
			else if (SendSeedOrStonePickup(entity, true))
			{
				// a spell seed over its spell or a teleport stone: the generic pick-up -> the place in hand packet with the
				// object (ApplyPlaceInHand: the seed in the hand and its spell closed)
			}
			else
			{
				// anything else held for 225 ms (a one-shot orb among them) is picked up itself: the generic pick-up, the
				// place in hand packet. (not verified) a release during the wait (the wait-pickup state only watches the
				// object's flag): here it clears _pickPressHeld
				SendPlaceInHand(entity);
			}
		}
	}
	UpdateTug(seconds, actionHeld);
	if (!_held && !_tug)
	{
		_releaseArmed = false;
		if (!actionHeld)
		{
			_pickPressHeld = false;
		}
	}
	// the locked select's particles and looping sound: from the start locked select packet's handler to its end (at
	// once)
	const bool multiPickup = _pickSource.has_value() && _held.has_value() && !_lockedSelectStopped;
	UpdatePickupSound(multiPickup);
	// the end of the interface's action processing (all its exits, after the power-up system): the interface hand state.
	// The power-up system itself runs on both paths, the turn's (the last frame's ms) and the frame's (this frame's):
	// hand_casting::ProcessTurn / Update. (not ported) the extra call per queued action and the playback path
	UpdateInterfaceHandState();
	// Once per game turn: the amount in the hand (the "amount" tooltip, forced), forced every turn of the scooping
	// (piles, fields, fish farms) and once when the locked select ends; its lifetime keeps it about 13 turns after that.
	// (The help system's turn is a step of the game logic loop)
	if (_toolTipTurn != game_clock::Turn())
	{
		_toolTipTurn = game_clock::Turn();
		const bool scooping = _pickSource.has_value() && _held.has_value();
		if ((scooping || _toolTipScooping) && _held &&
		    (PotInfoOf(*_held) == PotInfo::HandFood || PotInfoOf(*_held) == PotInfo::HandWood))
		{
			const auto& pot = Locator::entitiesRegistry::value().Get<const Pot>(*_held);
			help::tooltips::Force(helptext::k_ToolTipAmountInHand, static_cast<float>(pot.amount));
		}
		_toolTipScooping = scooping;
		// test hook OPENBLACK_TEST_TOOLTIP=<amount>: the amount forced every turn
		if (static const char* test = std::getenv("OPENBLACK_TEST_TOOLTIP"); test != nullptr)
		{
			help::tooltips::Force(helptext::k_ToolTipAmountInHand, static_cast<float>(std::atof(test)));
		}
	}
	// the tooltip's icon is deleted while paused. Its fade is input_prompt::Frame, with the real seconds next to the help
	// system's 3D draw
	help::tooltips::Frame();
	static const debug_env::Variable k_NoPickupPsys("OPENBLACK_NO_PICKUP_PSYS");
	UpdatePickupParticles(seconds, multiPickup && k_NoPickupPsys.Get() == nullptr);
	UpdateTestSplash(seconds);
	UpdateTestAbode(seconds);
	UpdateRootsAndPiles(seconds);
	archetypes::PotArchetype::UpdateSizes(seconds);

	// Gameplay state machine (original hand states): holding > gripping > can pick up > idle.
	// The normal state's clip (hndspec5.txt's order): Cwiggle at the entry ((not ported) Chold_fingers with the leash or
	// the leash action state); the object under the cursor (none if a LandscapeVortex), not the held one and with a 3D
	// object ((approximate) here a Mesh component, which fields and fish farms, drawn their own way, lack, or a temple's
	// entrance, whose 3D object is only collided), Ccan_pickup when the interface hand state is 9 (Can Pick up), else
	// Cstroke; later and on its own, a SpellIcon Ccan_pickup.
	// (not ported) Crotate / Cpitch (the camera mode's tricons) and the hand's own Crotate
	auto& clipRegistry = Locator::entitiesRegistry::value();
	// (openblack guard) Valid: the original's pointer is never a deleted object's
	const bool cursorValid = _cursorObject && clipRegistry.Valid(*_cursorObject);
	std::string clip = "Cwiggle";
	if (cursorValid && !clipRegistry.AllOf<LandscapeVortex>(*_cursorObject) && !(_held && *_held == *_cursorObject) &&
	    clipRegistry.AnyOf<Mesh, CitadelEntrance>(*_cursorObject))
	{
		clip = _interfaceHandState == 9 ? "Ccan_pickup" : "Cstroke";
	}
	if (cursorValid && clipRegistry.AllOf<SpellIcon>(*_cursorObject))
	{
		clip = "Ccan_pickup";
	}
	if (gripping)
	{
		clip = "Cgrip"; // the camera state's own clip
	}
	// The holding state's update: the pose and its frame (in ms) per hold type.
	//   ABOVE: Chold_above at dur * 0.5 * (1 - grip), grip = min(1, R / (3.2 s * 1.2))
	//   TREE / SIDE / VILLAGER: Chold_side at (dur >> 1) * grip, grip = min(1, R / (3.2 s))
	// The hand's state (the required state: held from the press, before the place in hand packet)
	const bool holding = (RenderHeld().has_value() || _tug.has_value()) && _override.empty() && _holdType != HoldType::None;
	if (_held && PotInfoOf(*_held) == PotInfo::HandFood)
	{
		// the hold radius of a food pile is its Get2DRadius x GetProportionRaised (ecs::object::Get2DRadius, inside
		// ComputeHoldParameters): the hand opens as the food in it grows.
		// Wood keeps a constant radius (wood piles have no Get2DRadius of their own).
		ComputeHoldParameters(*_held);
	}
	_animator->SetFrame(std::nullopt);
	if (holding && _holdType == HoldType::Magic && _animator->Has("Cwiggle"))
	{
		// MAGIC (a spell seed until it is ready): Cwiggle held at half its length
		clip = "Cwiggle";
		_animator->SetTime(static_cast<float>(_animator->GetDurationMs(clip) >> 1));
	}
	else if (holding && _holdType == HoldType::Above && _animator->Has("Chold_above"))
	{
		clip = "Chold_above";
		const float grip = std::min(1.0f, _holdRadius / (3.2f * _handScale * 1.2f));
		_animator->SetTime(static_cast<float>(_animator->GetDurationMs(clip)) * 0.5f * (1.0f - grip));
	}
	else if (holding && _holdType != HoldType::Above && _animator->Has("Chold_side"))
	{
		clip = "Chold_side";
		const float grip = std::min(1.0f, _holdRadius / (3.2f * _handScale));
		_animator->SetTime(static_cast<float>(_animator->GetDurationMs(clip) >> 1) * grip);
	}
	else
	{
		_animator->SetTime(std::nullopt);
	}
	if (!_override.empty())
	{
		clip = _override;
	}
	// special_hold (Cphile/Chorn) is not used for held objects: hand piles are SIDE, Chorn is the spell seed grain.
	// The original blends only at a change of the hand's state (Exit, Enter, the state blend start), 0.13 s, linear;
	// a clip change inside a state is not blended
	_animator->SetSpecialHold(std::nullopt, std::chrono::milliseconds(0));
	// the hand's dt: the camera time step, the real frame time inside the citadel. Outside a hand demo's playback the
	// camera time step is the real frame time too, as the original's: the creature's click is timed in it
	const auto handMs =
	    game_clock::IsInsideCitadel() ? game_clock::FrameRealMs() : game_clock::CameraFrameMs(hand_demo::IsPlaying());
	// the CREATURE state begins only once the creature is locked for the hand, (pending) and one more field of the
	// creature it also tests; leaving it sends the feedback (HandTurn.cpp)
	if (const auto state = UpdateCreatureFrame(handMs); state != _renderHandState)
	{
		_renderHandState = state;
		if (state == 9)
		{
			_playAnimMs = 0; // the play-anim state's Enter
		}
		_animator->StartStateBlend();
		_stateBlendFromWorld = _handWorld;
		// (not ported) the interaction camera of the CREATURE state
		_stateBlendFromPosition =
		    Locator::entitiesRegistry::value().Get<const Transform>(_hands[static_cast<size_t>(Side::Left)]).position;
	}
	// INVISIBLE (the required state for the interface's modes 1 / 2 / 12 / 25) -> its Enter hides the hand: not drawn,
	// and its shadow off; its Exit shows it again (the bones kept for the blend back). The draw preparation does nothing
	// more while hidden or with the interface inactive: no state Update, no pose. (not ported) the system cursor:
	// openblack has none
	// Only the INVISIBLE state hides (Enter -> Hide); an inactive interface alone only stops the update (inside the
	// citadel the required state is CITADEL first, and the hand stays drawn)
	// The same frame as the original: the required state reads the turn's interface hand state there too, and the
	// Enter's hide comes before
	// that frame's draw, as NotDrawn here before RenderingSystem::PrepareDraw (Game.cpp's order)
	_handHidden = _renderHandState == 0 || !interface_active::IsActive();
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto hand = _hands[static_cast<size_t>(Side::Left)];
		const bool invisible = _renderHandState == 0;
		if (invisible && !registry.AllOf<NotDrawn>(hand))
		{
			registry.Assign<NotDrawn>(hand);
			registry.SetDirty();
		}
		else if (!invisible && registry.AllOf<NotDrawn>(hand))
		{
			registry.Remove<NotDrawn>(hand);
			registry.SetDirty();
		}
	}
	if (_handHidden)
	{
		return;
	}
	// held to a creature: the hand strokes and slaps it, and rests on its body
	_creaturePose.reset();
	if (_renderHandState == 7 && Locator::creatureHandSystem::has_value() &&
	    Locator::creatureHandSystem::value().GetCreature().has_value())
	{
		_creaturePose = Locator::creatureHandSystem::value().Update(_mouseRayOrigin, _mouseRayDirection, _mouse,
		                                                            static_cast<float>(handMs) * 0.001f);
	}
	// The play-anim state's update: the clip at the time since Enter, which then goes on by the real frame ms; at the clip's
	// length the flag goes and the hand is back to NORMAL on the next frame
	if (_renderHandState == 9 && _playAnim && _override.empty() && _animator->Has(_playAnimClip))
	{
		clip = _playAnimClip;
		_animator->SetTime(static_cast<float>(_playAnimMs));
		_playAnimMs += game_clock::FrameRealMs();
		if (_playAnimMs >= _animator->GetDurationMs(clip))
		{
			_playAnim = false;
		}
	}
	if (clip != _animator->GetCurrentClip())
	{
		_animator->Play(clip, std::chrono::milliseconds(0));
	}
	// Grip drag lives mostly in the root translation of Lgrip_lr/Lgrip_fb.
	_animator->SetLayerTranslationScale(clip == "Cgrip" ? 1.0f : 0.0f);
	// Every clip applies its root bone under the hand's matrix, as the original
	_animator->SetRootLocked(false);
	_animator->Update(dt);
	// the blend after the state's Update, with the hand's dt (the camera time step x 0.001, the real frame time inside
	// the citadel)
	const auto ms =
	    game_clock::IsInsideCitadel() ? game_clock::FrameRealMs() : game_clock::CameraFrameMs(hand_demo::IsPlaying());
	_animator->AdvanceStateBlend(static_cast<float>(ms) * 0.001f);
}

int32_t HandSystem::RequiredHandState() const noexcept
{
	// the hand's required state (HAND_STATES)
	if (game_clock::IsInsideCitadel())
	{
		return 0xA; // CITADEL
	}
	const auto interfaceState = _interfaceHandState;
	const auto held = RenderHeld();
	if (held && interfaceState == 0x19)
	{
		return 0; // INVISIBLE
	}
	if (held)
	{
		// the status's locked select (_pickSource (inferred)) on a pile with an object held: HOLDING, before GRAIN / TUG
		// and without the 180 ms release impulse; INVISIBLE (mode 0x19) is above
		if (_pickSource && ecs::object_resources::IsPileResource(*_pickSource))
		{
			return 4; // HOLDING
		}
		auto& registry = Locator::entitiesRegistry::value();
		if (registry.Valid(*held) && registry.AllOf<SpellSeed>(*held))
		{
			return 8; // GRAIN
		}
		return _tug ? 3 : 4; // TUG while tugging, else HOLDING ((pending) the 180 ms release packet)
	}
	if (_playAnim || !_override.empty())
	{
		return 9; // PLAY_ANIM: StartFixedPosAnimation's flag, or openblack's forced animation
	}
	// the interface's hand state
	switch (interfaceState)
	{
	case 1:
	case 2:
	case 0xC:
	case 0x19:
		return 0; // INVISIBLE
	case 0xE:
		return 5; // TOTEM
	case 0x13:
		return _creatureInHand ? 7 : 1; // CREATURE with the creature, else NORMAL
	default:
		// (pending) CAMERA 2 in the camera mode with the tricons
		return 1; // NORMAL
	}
}

const std::vector<glm::mat4>* HandSystem::GetBoneMatrices() const noexcept
{
	return _animator ? &_animator->GetBoneMatrices() : nullptr;
}

std::vector<std::string> HandSystem::GetAnimationNames() const noexcept
{
	std::vector<std::string> names;
	if (_animator)
	{
		for (const auto& clip : _animator->ListClips())
		{
			if (!clip.name.empty() && clip.name.front() == 'C')
			{
				names.push_back(clip.name);
			}
		}
	}
	return names;
}

const std::string& HandSystem::GetCurrentAnimation() const noexcept
{
	static const std::string k_None;
	return _animator ? _animator->GetCurrentClip() : k_None;
}

void HandSystem::SetAnimationOverride(const std::string& name) noexcept
{
	_override = name;
}

void HandSystem::StartFixedPosAnimation(const std::string& clip, glm::vec3 point) noexcept
{
	// the fixed-position animation: the flag, the point and the clip; the time is Enter's
	_playAnim = true;
	_playAnimClip = clip;
	_playAnimPoint = point;
}
