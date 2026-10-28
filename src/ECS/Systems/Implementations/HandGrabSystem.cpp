/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "HandGrabSystem.h"

#include <algorithm>
#include <string_view>
#include <utility>

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "ECS/AltarSacrifice.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/HandClicked.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/HandHeldSeed.h"
#include "ECS/Components/HeldDrawTrack.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "GameHandGrabWorld.h"
#include "Hand/HandClickRules.h"
#include "Hand/OtherHeldDraw.h"
#include "Help/HelpProfile.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using hand_grab::GrabKind;

namespace
{
/// The hand's pick-up sound
constexpr uint32_t k_PickUpSample = 10;
/// The hand cries of the people, as many of each as there are: a child's, a woman's and a man's
constexpr uint32_t k_ChildCries = 180;
constexpr uint32_t k_WomanCries = 194;
constexpr uint32_t k_ManCries = 187;
constexpr uint32_t k_CriesEach = 7;
/// A tree pulled up creaks, one of three
constexpr uint32_t k_TreeCreaks = 32;
constexpr uint32_t k_TreeCreaksEach = 3;

/// Writes what a turn did with the hand of a player to the log. The check tools count these lines for each player, up to a
/// turn.
void LogHandDeed(PlayerNames player, std::string_view deed, entt::entity object)
{
	if (const auto logger = spdlog::get("game"))
	{
		uint32_t turnNow = 0;
		if (Locator::time::has_value())
		{
			turnNow = Locator::time::value().GetTurn();
		}
		SPDLOG_LOGGER_INFO(logger, "Hand deed: the hand of player {} {} thing {} at turn {}", static_cast<int>(player), deed,
		                   entt::to_integral(object), turnNow);
	}
}

/// The statics with kinds of their own the hand never takes: lanterns, bonfires, shields, vortices and teleport stones
bool StaticOfItsOwn(MobileStaticInfo type)
{
	switch (type)
	{
	case MobileStaticInfo::StreetLantern:
	case MobileStaticInfo::CountryLantern:
	case MobileStaticInfo::Bonfire:
	case MobileStaticInfo::PhysicalShield:
	case MobileStaticInfo::Vortex:
	case MobileStaticInfo::Teleport:
		return true;
	default:
		return false;
	}
}

/// Where a line meets a plane, none when it runs along it
std::optional<glm::vec3> LineMeetsPlane(glm::vec3 origin, glm::vec3 direction, glm::vec3 point, glm::vec3 normal)
{
	const float along = glm::dot(direction, normal);
	if (along == 0.0f)
	{
		return std::nullopt;
	}
	return origin + direction * (glm::dot(point - origin, normal) / along);
}

/// Whether the hand holds the thing: also while it waits for the turn that lets go of it
[[nodiscard]] bool Holds(const HandGrab& grab)
{
	return grab.state == HandGrab::State::Holding || grab.state == HandGrab::State::ReadyToThrow ||
	       grab.state == HandGrab::State::LettingGo;
}
} // namespace

HandGrabSystem::HandGrabSystem()
    : HandGrabSystem(std::make_unique<GameHandGrabWorld>())
{
}

HandGrabSystem::HandGrabSystem(std::unique_ptr<hand_grab::HandGrabWorldInterface> world)
    : _world(std::move(world))
{
}

HandGrabSystem::~HandGrabSystem() = default;

bool HandGrabSystem::Exists(entt::entity object) const
{
	return object != entt::null && _world->Entities().Valid(object);
}

HandGrab* HandGrabSystem::Grab()
{
	const auto hand = _world->Hand();
	if (!Exists(hand))
	{
		return nullptr;
	}
	auto& registry = _world->Entities();
	if (auto* grab = registry.TryGet<HandGrab>(hand))
	{
		return grab;
	}
	return &registry.Assign<HandGrab>(hand);
}

const HandGrab* HandGrabSystem::Grab() const
{
	const auto hand = _world->Hand();
	return Exists(hand) ? _world->Entities().TryGet<const HandGrab>(hand) : nullptr;
}

GrabKind HandGrabSystem::KindOf(entt::entity object) const
{
	const auto& registry = _world->Entities();
	if (registry.AllOf<Creature>(object))
	{
		return GrabKind::None;
	}
	if (registry.AllOf<Villager>(object))
	{
		return GrabKind::Villager;
	}
	if (registry.AllOf<Animal>(object))
	{
		return GrabKind::Animal;
	}
	if (registry.AllOf<Tree>(object))
	{
		return GrabKind::Tree;
	}
	if (registry.AllOf<DeadTree>(object))
	{
		return GrabKind::DeadTree;
	}
	if (registry.AllOf<BigForest>(object))
	{
		return GrabKind::BigForest;
	}
	// A one-shot miracle's globe is held as any loose thing is: a quick press taps it for its miracle instead
	if (registry.AllOf<OneOffSpellSeed>(object))
	{
		return GrabKind::MobileObject;
	}
	if (registry.AnyOf<StreetLantern, TeleportStone>(object))
	{
		return GrabKind::None;
	}
	if (const auto* pot = registry.TryGet<const Pot>(object))
	{
		// Loose pots are picked up. Piles are scooped from, not picked up: the only pile the hand holds is a handful, of
		// its pile's own handful kind, while it holds something
		if (_world->IsLoosePot(pot->type))
		{
			return GrabKind::MobileObject;
		}
		const auto facts = _world->PotFactsOf(object);
		return facts.has_value() && facts->handful == pot->type && pot->amount > 0 ? GrabKind::MobileObject : GrabKind::None;
	}
	if (const auto* still = registry.TryGet<const MobileStatic>(object))
	{
		if (StaticOfItsOwn(still->type))
		{
			return GrabKind::None;
		}
		return _world->StaticKindOf(still->type) == MobileStaticInfo::Rock ? GrabKind::Rock : GrabKind::MobileStatic;
	}
	if (const auto* mobile = registry.TryGet<const MobileObject>(object))
	{
		return mobile->type == MobileObjectInfo::LumpOfPoo ? GrabKind::Poo : GrabKind::MobileObject;
	}
	// The football is held as any thing lying about
	if (registry.AllOf<Ball>(object))
	{
		return GrabKind::MobileObject;
	}
	return GrabKind::None;
}

hand_grab::Holdable HandGrabSystem::HoldableOf(entt::entity object) const
{
	const auto& registry = _world->Entities();
	const auto kind = KindOf(object);
	const auto* action = registry.TryGet<const LivingAction>(object);
	const bool hiding = action != nullptr && action->states[static_cast<size_t>(LivingAction::Index::Top)] ==
	                                             static_cast<uint8_t>(VillagerStates::GoAndHideInNearbyBuilding);
	return {
	    .kind = kind,
	    .available = Exists(object),
	    .atHome = registry.AllOf<AtHome>(object),
	    .inHand = registry.AllOf<InHand>(object),
	    .hiding = hiding,
	    .speciesAllows = kind == GrabKind::Animal && _world->SpeciesAllowsPickUp(object),
	    .radius = _world->SizeOf(object).radius,
	    .isRock = kind == GrabKind::Rock,
	};
}

bool HandGrabSystem::HandInInfluence() const
{
	// Asked at the point picked under the cursor: the position of the thing there, so a thing standing just inside the
	// border is within reach even with the land behind it outside, and the other way round. While the hand keeps some of
	// the player's influence past the border it counts as inside however far out it is. With nothing under the cursor at
	// all the hand counts as inside, as for what it offers.
	const auto point = _world->PickedPoint();
	return !point.has_value() || _world->HandInInfluence(_world->HandPlayer(), *point);
}

hand_grab::Gate HandGrabSystem::GateOf(entt::entity object) const
{
	return GateAt(object, _world->PickedPoint());
}

hand_grab::Gate HandGrabSystem::GateAt(entt::entity object, std::optional<glm::vec3> point) const
{
	const auto& registry = _world->Entities();
	return {.spaceInHand = true,
	        .alreadyInHand = registry.AllOf<InHand>(object),
	        .valid = hand_grab::ValidForPlaceInHand(HoldableOf(object)),
	        .cannotBePickedUp = registry.AllOf<CannotBePickedUp>(object),
	        .carried = registry.AllOf<CarriedByTornado>(object),
	        .inInfluence = !point.has_value() || _world->HandInInfluence(_world->HandPlayer(), *point)};
}

bool HandGrabSystem::MayTake(entt::entity object) const
{
	return hand_grab::PassesGate(GateOf(object));
}

bool HandGrabSystem::IsInInfluence() const
{
	// The interface of this computer asks about its own hand
	const hand_grab::HandGrabWorldInterface::ActingPlayer own(*_world, _world->OwnHandPlayer());
	return HandInInfluence();
}

std::string HandGrabSystem::WhyNotTake(entt::entity object) const
{
	// The interface of this computer asks about its own hand
	const hand_grab::HandGrabWorldInterface::ActingPlayer own(*_world, _world->OwnHandPlayer());
	if (!_world->Entities().Valid(object))
	{
		return "there is no such thing";
	}
	if (IsBusy())
	{
		return "the hand is busy with another thing";
	}
	return std::string(hand_grab::GateRefusal(GateOf(object)));
}

float HandGrabSystem::HangOf(entt::entity object) const
{
	return HoldOfObject(object).loweringMultiplier * _world->SizeOf(object).height;
}

hand_grab::HoldFacts HandGrabSystem::HoldOfObject(entt::entity object) const
{
	const auto& registry = _world->Entities();
	const auto size = _world->SizeOf(object);
	const auto* still = registry.TryGet<const MobileStatic>(object);
	const auto type = still != nullptr ? still->type : MobileStaticInfo::None;
	return hand_grab::HoldOf(KindOf(object), type, still != nullptr ? _world->StaticMeshOf(type) : MeshId::Dummy, size.height,
	                         size.radius);
}

void HandGrabSystem::Tap(entt::entity object)
{
	// The input and the frames of this computer act with its own hand
	const hand_grab::HandGrabWorldInterface::ActingPlayer own(*_world, _world->OwnHandPlayer());
	// Only within the player's influence (a script's scroll or sign is tapped anywhere), and not a thing a script holds
	// out of reach
	const auto& registry = _world->Entities();
	const bool needsInfluence = !registry.AllOf<ScriptHighlight>(object);
	if ((needsInfluence && !HandInInfluence()) || registry.AllOf<CannotBePickedUp>(object))
	{
		return;
	}
	_world->SendCommand(player_commands::HandTap {.object = object});
}

void HandGrabSystem::DoTap(PlayerNames player, entt::entity object)
{
	// The command acts with the hand of its player
	const hand_grab::HandGrabWorldInterface::ActingPlayer acting(*_world, player);
	if (player != _world->HandPlayer() || !Exists(object))
	{
		return;
	}
	_world->TapThing(object, _world->ReportedHandPlace(), player);
}

HandClicked* HandGrabSystem::Clicked()
{
	const auto hand = _world->Hand();
	if (!Exists(hand))
	{
		return nullptr;
	}
	auto& registry = _world->Entities();
	if (auto* clicked = registry.TryGet<HandClicked>(hand))
	{
		return clicked;
	}
	return &registry.Assign<HandClicked>(hand);
}

bool HandGrabSystem::MarksThing(entt::entity object) const
{
	return hand_click::MarksThing(_world->HoldsLooseLeash(), _world->Entities().AllOf<Reward>(object));
}

void HandGrabSystem::ClickThing(entt::entity object)
{
	// The input and the frames of this computer act with its own hand
	const hand_grab::HandGrabWorldInterface::ActingPlayer own(*_world, _world->OwnHandPlayer());
	if (!MarksThing(object))
	{
		return;
	}
	// The turn writes the click, so that the scripts read the same click at each frame rate and on every computer
	_world->SendCommand(player_commands::HandClick {.thing = object, .release = false});
}

void HandGrabSystem::ClickReleased()
{
	// The input and the frames of this computer act with its own hand
	const hand_grab::HandGrabWorldInterface::ActingPlayer own(*_world, _world->OwnHandPlayer());
	const auto under = _world->ObjectUnderCursor();
	const bool marks = under.has_value() && MarksThing(*under);
	const auto* grab = Grab();
	std::optional<map_coords::MapCoords> place;
	if (grab != nullptr && grab->handPoint.has_value())
	{
		place =
		    map_coords::MapCoords {.x = map_coords::ToFixed(grab->handPoint->x), .z = map_coords::ToFixed(grab->handPoint->z)};
	}
	// A thing under the hand that the click does not mark is not sent: the place is clicked in its stead
	_world->SendCommand(player_commands::HandClick {
	    .thing = marks ? *under : entt::null,
	    .release = true,
	    .hasPlace = place.has_value(),
	    .placeX = place.has_value() ? place->x : 0,
	    .placeZ = place.has_value() ? place->z : 0,
	});
}

void HandGrabSystem::DoClick(PlayerNames player, const player_commands::HandClick& click, uint32_t turn)
{
	// The command acts with the hand of its player
	const hand_grab::HandGrabWorldInterface::ActingPlayer acting(*_world, player);
	auto* clicked = Clicked();
	if (clicked == nullptr)
	{
		return;
	}
	// A thing that is gone by the turn is not clicked
	const bool thing = click.thing != entt::null && Exists(click.thing);
	if (!click.release)
	{
		if (thing)
		{
			hand_click::ClickThing(*clicked, click.thing, turn);
		}
		return;
	}
	const auto place =
	    click.hasPlace ? std::optional(map_coords::MapCoords {.x = click.placeX, .z = click.placeZ}) : std::nullopt;
	hand_click::ClickReleased(*clicked, thing ? std::optional(click.thing) : std::nullopt, thing, place, turn);
}

bool HandGrabSystem::Press(uint32_t nowMs, uint32_t turn)
{
	// The input and the frames of this computer act with its own hand
	const hand_grab::HandGrabWorldInterface::ActingPlayer own(*_world, _world->OwnHandPlayer());
	auto* grab = Grab();
	if (grab == nullptr)
	{
		return false;
	}
	switch (grab->state)
	{
	case HandGrab::State::Holding:
		grab->pressMs = nowMs;
		grab->pressTurn = turn;
		// The press first gives what the hand holds to the creature that it offers it to, or to a creature under the
		// hand that would take it. The hand keeps hold of it, not ready to throw, until the creature takes it.
		if (Give(*grab, _world->ObjectUnderCursor()))
		{
			return true;
		}
		// On something the held thing can be used on, the hand sends the use of it, which the turn does; otherwise ready to
		// throw: the spring takes hold of the hand the next frame, where the hand then is
		if (const auto target = _world->ObjectUnderCursor();
		    target.has_value() && HandInInfluence() && AppliesTo(*grab, *target))
		{
			_world->SendCommand(player_commands::HandApply {.target = *target});
			return true;
		}
		// Out of the player's influence the press does nothing: the hand keeps hold, and letting go of the button later,
		// even back inside, doesn't throw
		if (!HandInInfluence())
		{
			return true;
		}
		grab->state = HandGrab::State::ReadyToThrow;
		grab->springPending = true;
		return true;
	case HandGrab::State::Grabbing:
	case HandGrab::State::Taking:
	case HandGrab::State::ReadyToThrow:
	case HandGrab::State::LettingGo:
		return true;
	case HandGrab::State::Empty:
		break;
	}

	// With nothing under the cursor the hand reaches for what stands nearest the land behind it
	auto object = _world->ObjectUnderCursor();
	if (!object.has_value())
	{
		object = _world->ObjectNearHand();
	}
	// A press on a pile, a field or a fish farm sends the lock on to it. The turn that does the lock starts the scoop.
	if (object.has_value() && Scoopable(*object))
	{
		grab->locking = *object;
		_world->SendCommand(player_commands::HandLock {.object = *object});
		return true;
	}
	if (!object.has_value() || !MayTake(*object))
	{
		// A press the hand can't take clicks the thing, and is a tap on it (clicking and activating)
		if (object.has_value())
		{
			ClickThing(*object);
			Tap(*object);
		}
		return false;
	}
	grab->state = HandGrab::State::Grabbing;
	grab->object = *object;
	grab->pressMs = nowMs;
	grab->pressTurn = turn;
	grab->pullSeconds = 0.0f;
	grab->tug.reset();
	// Something in flight isn't pulled: it is caught once the button has been held a while, as is a thing the hand can't
	// pull at (a big forest). Anything else is pulled at, until it comes free.
	grab->pulling = !_world->IsFlying(*object) && hand_grab::Tuggable(KindOf(*object));
	// The hand grips all things that it can pull at from the press, also a thing in flight
	grab->grips = hand_grab::Tuggable(KindOf(*object));
	// The first frame of the pull never pulls
	grab->waits = true;
	return true;
}

std::optional<entt::entity> HandGrabSystem::Release(uint32_t nowMs, uint32_t turn)
{
	// The input and the frames of this computer act with its own hand
	const hand_grab::HandGrabWorldInterface::ActingPlayer own(*_world, _world->OwnHandPlayer());
	auto* grab = Grab();
	if (grab == nullptr)
	{
		return std::nullopt;
	}
	switch (grab->state)
	{
	case HandGrab::State::Grabbing:
	{
		// The pull freed the thing: the hand takes it, also when the button is let go in the same frame
		if (grab->tug.has_value() && !grab->pulling && Exists(grab->object))
		{
			SendTake(*grab, grab->object);
			return std::nullopt;
		}
		// Let go before the thing came free: it stays where the turns put it, and a short press was a tap. The lean of
		// the pull was only drawn, so the world does not change here.
		const auto object = grab->object;
		const bool tap = hand_grab::ElapsedMs(nowMs, grab->pressMs, turn, grab->pressTurn) <= hand_grab::k_GrabWaitMs;
		Empty(*grab);
		// It still becomes the thing last let go, which a flying thing's twist then finds
		grab->released = object;
		// A tap clicks whatever is under the hand as it is let go
		if (tap)
		{
			if (const auto under = _world->ObjectUnderCursor(); under.has_value())
			{
				ClickThing(*under);
			}
		}
		if (tap && Exists(object))
		{
			// With the leash held loose in the hand, the tap is an order to the creature when the leash takes it, needing no
			// influence; otherwise the thing is tapped
			if (!_world->LeashTap(object))
			{
				Tap(object);
			}
			return object;
		}
		return std::nullopt;
	}
	case HandGrab::State::ReadyToThrow:
	{
		// Let go with the point picked under the cursor off the map or out of the player's influence, the hand keeps hold
		// and its spring lets go of it
		if (const auto point = _world->PickedPoint(); !point.has_value() || !_world->InBounds(*point) || !HandInInfluence())
		{
			grab->state = HandGrab::State::Holding;
			grab->springOn = false;
			grab->springPending = false;
			return std::nullopt;
		}
		// The hand sends the throw with the velocity that it has now. It holds the thing until the turn that does the
		// command, and its spring keeps the hand where the drag took it.
		const auto velocity = hand_grab::ThrowVelocity(grab->recordedThrow, grab->springOn, grab->spring.Velocity());
		grab->lastThrow = HandGrab::Throw {
		    .velocity = velocity,
		    .springVelocity = hand_grab::ThrowVelocity(std::nullopt, grab->springOn, grab->spring.Velocity()),
		    .from = grab->spring.Position(),
		};
		grab->state = HandGrab::State::LettingGo;
		// The thing starts as this computer drew it last, and comes up out of the land. Then it flies for as many turns
		// as the reports of this hand come late. The command sends the start and the velocity after them to every
		// computer. Until the turn of the command, the frames draw the thing along that flight.
		auto led = ThrowStartOf(grab->object, velocity, hand_grab::throw_lead::LeadTurns(_world->ReportLatencyMs()));
		if (led.flight.has_value())
		{
			led.flight->sentTurn = _world->FrameTurn();
		}
		grab->predicted = std::move(led.flight);
		_world->SendCommand(player_commands::HandLetGo {.object = grab->object, .velocity = led.velocity, .start = led.start});
		return std::nullopt;
	}
	case HandGrab::State::Holding:
		// Letting go of the button sends the end of the lock, which ends the scoop at the turn
		if (grab->scoopSource != entt::null)
		{
			_world->SendCommand(player_commands::HandUnlock {.object = grab->scoopSource});
		}
		break;
	case HandGrab::State::Empty:
		// Let go before a turn did the lock, the end of the lock follows it
		if (grab->locking != entt::null)
		{
			_world->SendCommand(player_commands::HandUnlock {.object = grab->locking});
		}
		break;
	case HandGrab::State::Taking:
	case HandGrab::State::LettingGo:
		break;
	}
	return std::nullopt;
}

hand_grab::throw_lead::LedThrow HandGrabSystem::ThrowStartOf(entt::entity object, glm::vec3 velocity, int leadTurns)
{
	// It lies along the slope when the hand lets go slowly, but not a tree of any kind
	const bool anyTree = _world->Entities().AnyOf<Tree, DeadTree>(object);
	return _world->ThrowStartOf(object, !(anyTree || hand_grab::IsFastRelease(velocity)), velocity, leadTurns);
}

void HandGrabSystem::DoLetGo(PlayerNames player, const player_commands::HandLetGo& letGo)
{
	// The command acts with the hand of its player
	const hand_grab::HandGrabWorldInterface::ActingPlayer acting(*_world, player);
	// Every computer lets go of what the hand holds, whatever the hand of this computer does now
	auto* grab = Grab();
	if (grab == nullptr || player != _world->HandPlayer() || !Holds(*grab) || grab->object != letGo.object)
	{
		return;
	}
	// Letting go onto the land counts as one more turn for what the hand keeps of the player's influence past the border,
	// before the throw
	_world->HeldThingUsedOnLand(player);
	const bool pours = _world->Entities().AllOf<Pot>(letGo.object) && hand_grab::PotPours(letGo.velocity);
	LogHandDeed(player, pours ? "pours" : hand_grab::IsThrow(letGo.velocity, false) ? "throws" : "puts down", letGo.object);
	LetGo(*grab, letGo.velocity, letGo.start);
}

bool HandGrabSystem::Scoopable(entt::entity source) const
{
	// Only inside the player's influence, and only from what has something to give
	if (!HandInInfluence())
	{
		return false;
	}
	const auto scoop = _world->ScoopFactsOf(PotInfo::HandFood);
	if (const auto field = _world->FieldFactsOf(source))
	{
		return field->lockable && std::min(scoop.initial, field->food) >> (field->ripe ? 1u : 0u) != 0;
	}
	if (const auto farm = _world->FishFarmOf(source))
	{
		return fish_farm::FirstHandful(scoop.initial, *farm) != 0;
	}
	// Only a pile is scooped from, never a handful
	const auto facts = _world->PotFactsOf(source);
	if (!facts.has_value() || facts->potType == PotType::Pot ||
	    _world->Entities().Get<const Pot>(source).type == facts->handful)
	{
		return false;
	}
	return std::min(_world->ScoopFactsOf(facts->handful).initial, facts->amount) != 0;
}

void HandGrabSystem::DoLock(PlayerNames player, entt::entity object)
{
	// The command acts with the hand of its player
	const hand_grab::HandGrabWorldInterface::ActingPlayer acting(*_world, player);
	// Every computer starts the scoop, whatever the hand of this computer does now. Only a hand that holds nothing scoops.
	auto* grab = Grab();
	if (grab == nullptr || player != _world->HandPlayer())
	{
		return;
	}
	if (grab->locking == object)
	{
		grab->locking = entt::null;
	}
	if (Holds(*grab) || !Exists(object))
	{
		return;
	}
	if (StartScoop(*grab, object))
	{
		LogHandDeed(player, "scoops from", object);
	}
}

void HandGrabSystem::DoUnlock(PlayerNames player, entt::entity object)
{
	// The command acts with the hand of its player
	const hand_grab::HandGrabWorldInterface::ActingPlayer acting(*_world, player);
	auto* grab = Grab();
	if (grab == nullptr || player != _world->HandPlayer() || grab->scoopSource != object)
	{
		return;
	}
	// The scoop ends; the hand holds its handful. After a pile or a fish farm, but not a field, the hand shows the amount it
	// holds.
	if (const auto* handful = _world->Entities().TryGet<const Pot>(grab->object);
	    handful != nullptr && Exists(object) && !_world->FieldFactsOf(object).has_value())
	{
		_amountToolTip = handful->amount;
	}
	EndScoop(*grab);
}

bool HandGrabSystem::StartScoop(HandGrab& grab, entt::entity source)
{
	if (const auto field = _world->FieldFactsOf(source))
	{
		return StartFieldScoop(grab, source, *field);
	}
	if (const auto farm = _world->FishFarmOf(source))
	{
		return StartFishFarmScoop(grab, source, *farm);
	}
	const auto facts = _world->PotFactsOf(source);
	// Only a pile is scooped from, never a handful. The press asked for the player's influence.
	if (!facts.has_value() || facts->potType == PotType::Pot)
	{
		return false;
	}
	const auto& registry = _world->Entities();
	if (registry.Get<const Pot>(source).type == facts->handful)
	{
		return false;
	}
	const auto scoop = _world->ScoopFactsOf(facts->handful);
	const auto first = std::min(scoop.initial, facts->amount);
	if (first == 0)
	{
		return false;
	}
	const auto sourcePosition = _world->PoseOf(source).origin;
	_world->TakeFromPile(source, first);
	const auto hand = _world->ReportedHandPlace();
	const auto handful = _world->MakeHandful(facts->handful, hand, first, facts->poisoned);
	if (!Exists(handful))
	{
		return false;
	}
	Take(grab, handful, false);
	if (grab.state != HandGrab::State::Holding)
	{
		return false;
	}
	grab.scoopSource = source;
	_world->NoteTakenFromOwnerOf(source);
	grab.scoopTurns = 0;
	grab.scoopAnchor = hand;
	// What is scooped streams from the source into the hand
	grab.scoopStreamSeconds = 0.0f;
	// The cursor is pinned while it scoops
	_world->PinCursor(true);
	grab.scoopStream = _world->StartScoopStream(facts->resource, sourcePosition, facts->poisoned);
	return true;
}

bool HandGrabSystem::StartFieldScoop(HandGrab& grab, entt::entity field, const FieldFacts& facts)
{
	if (!facts.lockable)
	{
		return false;
	}
	// A field gives its first handful of food at once, half of it once ripe
	const auto scoop = _world->ScoopFactsOf(PotInfo::HandFood);
	auto first = std::min(scoop.initial, facts.food);
	if (facts.ripe)
	{
		first >>= 1u;
	}
	if (first == 0)
	{
		return false;
	}
	_world->TakeFromField(field, first);
	const auto hand = _world->ReportedHandPlace();
	const auto handful = _world->MakeHandful(PotInfo::HandFood, hand, first, false);
	if (!Exists(handful))
	{
		return false;
	}
	Take(grab, handful, false);
	if (grab.state != HandGrab::State::Holding)
	{
		return false;
	}
	grab.scoopSource = field;
	_world->NoteTakenAt(_world->PoseOf(field).origin);
	grab.scoopTurns = 0;
	grab.scoopAnchor = hand;
	grab.scoopStreamSeconds = 0.0f;
	// The cursor is pinned while it scoops
	_world->PinCursor(true);
	grab.scoopStream = _world->StartScoopStream(ResourceType::Food, _world->PoseOf(field).origin, false);
	return true;
}

bool HandGrabSystem::ScoopField(HandGrab& grab, const FieldFacts& facts)
{
	auto& registry = _world->Entities();
	auto* handful = registry.TryGet<Pot>(grab.object);
	if (handful == nullptr)
	{
		return false;
	}
	const auto scoop = _world->ScoopFactsOf(PotInfo::HandFood);
	// It sounds as food pours, before anything is taken
	grab.scoopSound = _world->PlayScoopSound(grab.scoopSound, ResourceType::Food, _world->PoseOf(_world->Hand()).origin,
	                                         hand_grab::ScoopRamp(grab.scoopTurns, scoop));
	// As much as the ramp gives, no more than the field has, within what one handful holds, and half once ripe
	auto taken = std::min(hand_grab::ScoopAmount(grab.scoopTurns, scoop), facts.food);
	if (scoop.maxPickedUp != 0)
	{
		const auto room = static_cast<int64_t>(scoop.maxPickedUp) - static_cast<int64_t>(handful->amount);
		taken = static_cast<uint32_t>(std::clamp<int64_t>(std::min<int64_t>(room, taken), 0, taken));
	}
	if (facts.ripe)
	{
		taken /= 2u;
	}
	if (taken == 0)
	{
		StopScoopEffects(grab);
		return false;
	}
	_world->TakeFromField(grab.scoopSource, taken);
	handful->amount += taken;
	_amountToolTip = handful->amount;
	_world->ResizePot(grab.object);
	return true;
}

bool HandGrabSystem::StartFishFarmScoop(HandGrab& grab, entt::entity farm, const fish_farm::Type& type)
{
	// The first handful comes by what a full farm holds, not out of the fish in it
	const auto scoop = _world->ScoopFactsOf(PotInfo::HandFood);
	const auto first = fish_farm::FirstHandful(scoop.initial, type);
	if (first == 0)
	{
		return false;
	}
	const auto hand = _world->ReportedHandPlace();
	const auto handful = _world->MakeHandful(PotInfo::HandFood, hand, first, false);
	if (!Exists(handful))
	{
		return false;
	}
	Take(grab, handful, false);
	if (grab.state != HandGrab::State::Holding)
	{
		return false;
	}
	grab.scoopSource = farm;
	_world->NoteTakenFromOwnerOf(farm);
	grab.scoopTurns = 0;
	grab.scoopAnchor = hand;
	grab.scoopStreamSeconds = 0.0f;
	// The cursor is pinned while it scoops
	_world->PinCursor(true);
	grab.scoopStream = _world->StartFishScoopStream(_world->PoseOf(farm).origin);
	return true;
}

bool HandGrabSystem::ScoopFishFarm(HandGrab& grab, const fish_farm::Type& type)
{
	auto& registry = _world->Entities();
	auto* handful = registry.TryGet<Pot>(grab.object);
	if (handful == nullptr)
	{
		return false;
	}
	const auto scoop = _world->ScoopFactsOf(PotInfo::HandFood);
	// It sounds as food pours, before anything is taken
	grab.scoopSound = _world->PlayScoopSound(grab.scoopSound, ResourceType::Food, _world->PoseOf(_world->Hand()).origin,
	                                         hand_grab::ScoopRamp(grab.scoopTurns, scoop));
	const auto wanted =
	    fish_farm::ScoopWanted(hand_grab::ScoopAmount(grab.scoopTurns, scoop), type, handful->amount, scoop.maxPickedUp);
	// Once the farm gives nothing the scoop ends; while it gives any, the hand gets all it wanted
	if (_world->TakeFromFishFarm(grab.scoopSource, wanted) == 0)
	{
		StopScoopEffects(grab);
		return false;
	}
	handful->amount += wanted;
	_amountToolTip = handful->amount;
	_world->ResizePot(grab.object);
	return true;
}

bool HandGrabSystem::Scoop(HandGrab& grab)
{
	if (const auto field = _world->FieldFactsOf(grab.scoopSource))
	{
		return ScoopField(grab, *field);
	}
	if (const auto farm = _world->FishFarmOf(grab.scoopSource))
	{
		return ScoopFishFarm(grab, *farm);
	}
	auto& registry = _world->Entities();
	const auto* handful = registry.TryGet<Pot>(grab.object);
	const auto source = _world->PotFactsOf(grab.scoopSource);
	// It scoops only into a handful of the pile's own kind
	if (handful == nullptr || !source.has_value() || handful->type != source->handful)
	{
		return false;
	}
	const auto scoop = _world->ScoopFactsOf(source->handful);
	const auto wanted = hand_grab::ScoopAmount(grab.scoopTurns, scoop);
	// It sounds before anything is taken
	grab.scoopSound = _world->PlayScoopSound(grab.scoopSound, source->resource, _world->PoseOf(_world->Hand()).origin,
	                                         hand_grab::ScoopRamp(grab.scoopTurns, scoop));
	const auto taken = hand_grab::ScoopTaken(wanted, source->amount, handful->amount, scoop);
	if (taken == 0)
	{
		StopScoopEffects(grab);
		return false;
	}
	const auto took = _world->TakeFromPile(grab.scoopSource, taken);
	registry.Get<Pot>(grab.object).amount += took;
	_amountToolTip = registry.Get<Pot>(grab.object).amount;
	_world->ResizePot(grab.object);
	// A pile scooped empty stops streaming and sounding; the scoop ends with the next turn
	if (!Exists(grab.scoopSource))
	{
		StopScoopEffects(grab);
	}
	return true;
}

void HandGrabSystem::StopScoopEffects(HandGrab& grab)
{
	if (grab.scoopStream.has_value())
	{
		_world->StopScoopStream(*grab.scoopStream);
		grab.scoopStream.reset();
	}
	if (grab.scoopSound != entt::null)
	{
		_world->StopScoopSound();
		grab.scoopSound = entt::null;
	}
}

void HandGrabSystem::EndScoop(HandGrab& grab)
{
	StopScoopEffects(grab);
	if (grab.scoopSource != entt::null)
	{
		_world->PinCursor(false);
	}
	grab.scoopSource = entt::null;
	grab.scoopTurns = 0;
}

bool HandGrabSystem::AppliesTo(const HandGrab& grab, entt::entity target) const
{
	const auto held = grab.object;
	if (!Exists(held) || target == held || !Exists(target))
	{
		return false;
	}
	// A pot goes to a store of what it holds, or into a pile or pot of the same. Anything else goes whole to an altar, a
	// gate stone plinth or a store.
	const auto pot = _world->PotFactsOf(held);
	if (!pot.has_value())
	{
		return _world->TakesWhole(target, held);
	}
	if (_world->StoresResource(target, pot->resource))
	{
		return true;
	}
	const auto other = _world->PotFactsOf(target);
	return other.has_value() && other->resource == pot->resource;
}

void HandGrabSystem::DoApply(PlayerNames player, entt::entity target)
{
	// The command acts with the hand of its player
	const hand_grab::HandGrabWorldInterface::ActingPlayer acting(*_world, player);
	// Every computer uses the held thing on the target, whatever the hand of this computer does now
	auto* grab = Grab();
	if (grab == nullptr || player != _world->HandPlayer() || !Holds(*grab))
	{
		return;
	}
	if (ApplyTo(*grab, target))
	{
		LogHandDeed(player, "uses what it holds on", target);
	}
}

bool HandGrabSystem::ApplyTo(HandGrab& grab, entt::entity target)
{
	const auto held = grab.object;
	if (!Exists(held) || target == held || !Exists(target))
	{
		return false;
	}
	// A villager, an animal, a tree or a dead tree given to a worship site's altar is sacrificed there. The altar can
	// refuse it: then the hand keeps it, and the press does nothing more.
	switch (_world->OfferAtAltar(target, held))
	{
	case altar_offering::Answer::Sacrificed:
		_world->HelpEvent(altar_sacrifice::k_AltarHelpEvent);
		Empty(grab);
		return true;
	case altar_offering::Answer::Refused:
		_world->HelpEvent(altar_sacrifice::k_AltarHelpEvent);
		return true;
	case altar_offering::Answer::NotAnAltar:
		break;
	}
	// A pot is given to a store of what it holds, or poured into a pile or pot of the same
	const auto pot = _world->PotFactsOf(held);
	if (!pot.has_value())
	{
		// A gate stone given to the gate stone plinth is laid in it and gone from the hand
		if (_world->LayGateStone(target, held))
		{
			_world->Entities().Remove<InHand>(held);
			_world->UseUp(held);
			_world->HelpEvent(altar_sacrifice::k_UsedOnThingHelpEvent);
			Empty(grab);
			return true;
		}
		// A tree, a dead tree, a fence, a mushroom or an animal goes whole into a store of what it is worth
		// TODO(stores): onto a teleport or a scaffold; openblack has none of those yet
		if (!_world->TakeIntoStore(target, held))
		{
			return false;
		}
		_world->HelpEvent(altar_sacrifice::k_UsedOnThingHelpEvent);
		Empty(grab);
		return true;
	}
	if (_world->StoresResource(target, pot->resource))
	{
		_world->AddToStore(target, pot->resource, pot->amount, pot->poisoned);
	}
	else if (const auto other = _world->PotFactsOf(target); other.has_value() && other->resource == pot->resource)
	{
		_world->PourAt(pot->resource, _world->ReportedHandPlace(), pot->amount, _world->HandPlayer(), pot->poisoned);
	}
	else
	{
		return false;
	}
	_world->Entities().Remove<InHand>(held);
	_world->UseUp(held);
	_world->HelpEvent(altar_sacrifice::k_UsedOnThingHelpEvent);
	Empty(grab);
	return true;
}

void HandGrabSystem::StartPull(HandGrab& grab, const Frame& frame)
{
	const auto object = grab.object;
	// A new pull starts from the place, the turn and the size of the thing in the world, not from the lean of an
	// earlier pull
	_world->ForgetPulledPose(object);
	const auto pose = _world->PoseOf(object);
	hand_grab::Tug tug {.axes = pose.axes, .base = pose.origin};
	// The pull works in the plane of the land under the thing, at the height the hand's drop below the camera reaches
	// when scaled from the hand's distance across the ground to the base's. The hand is where the cursor puts it.
	grab.pullPlaneNormal = _world->LandNormalAt(pose.origin);
	const auto across = [](glm::vec3 a, glm::vec3 b) { return glm::length(glm::vec2(a.x - b.x, a.z - b.z)); };
	const float toBase = across(frame.camera, pose.origin);
	const float toHand = across(frame.camera, frame.target);
	grab.pullPlanePoint = pose.origin;
	grab.pullPlanePoint.y =
	    toHand > 0.0f ? frame.camera.y - (toBase / toHand) * (frame.camera.y - frame.target.y) : pose.origin.y;
	const auto hold = HoldOfObject(object);
	// The hand holds it the way its kind is held while it pulls
	grab.hold = hold;
	grab.holdDistance = hand_grab::HoldDistance(hold.loweringMultiplier, _world->SizeOf(object).height, grab.handSize);
	grab.stretch.Reset(1.0f);
	grab.tug = tug;
	const auto& before = _world->Entities().Get<const Transform>(object);
	grab.pullRotation = before.rotation;
	grab.pullScale = before.scale;
}

glm::vec3 HandGrabSystem::Pull(HandGrab& grab, const Frame& frame)
{
	const auto object = grab.object;
	auto& tug = *grab.tug;
	const auto size = _world->SizeOf(object);
	const bool leans = !_world->IsOfRockMaterial(object);
	grab.pullSeconds += frame.seconds;
	if (grab.pullSeconds < hand_grab::k_PullBlendSeconds || grab.waits)
	{
		// The pull starts the frame after the hand has faded into its pulling pose
		grab.waits = grab.pullSeconds < hand_grab::k_PullBlendSeconds;
	}
	else if (const auto hand = LineMeetsPlane(frame.rayOrigin, frame.rayDirection, grab.pullPlanePoint, grab.pullPlaneNormal))
	{
		const bool tree = KindOf(object) == GrabKind::Tree;
		const auto result = hand_grab::PullAt(tug, {
		                                               .hand = *hand,
		                                               .holdDistance = grab.holdDistance,
		                                               .maxForce = hand_grab::MaxForce(0.0f),
		                                               .weight = _world->WeightOf(object),
		                                               .height = size.height,
		                                               .tree = tree,
		                                               .leans = leans,
		                                               .seconds = frame.seconds,
		                                           });
		grab.stretch.SetDestination(result.stretchTarget, hand_grab::k_TugStretchSeconds);
		grab.stretch.Update(frame.seconds);
		if (result.comesFree)
		{
			// A standing tree pulled out of the ground leaves its roots in a hole. The hole is at the base of the tree,
			// with the turn and the size of the tree before the pull, because the lean changes only the drawn tree.
			if (tree && !_world->IsFlying(object))
			{
				_world->LeaveRootsHole(object, tug.base, grab.pullRotation, grab.pullScale.x);
			}
			// Make the land's draw list again when the hand pulls a living thing free
			if (const auto kind = KindOf(object); kind == GrabKind::Villager || kind == GrabKind::Animal)
			{
				_world->RemakeDrawList(1);
			}
			grab.pulling = false;
		}
		// It is drawn leaning, and stretched up towards the hand
		auto axes = tug.axes;
		if (leans)
		{
			axes[1] *= grab.stretch.GetValue();
		}
		_world->DrawPulledPose(object, {.axes = axes, .origin = tug.base});
		_drawnPull = object;
	}
	// How far up it the hand grips it, as the hand measures it every frame
	grab.holdDistance = HoldOfObject(object).loweringMultiplier * size.height;
	return hand_grab::TugHandPoint(tug, grab.holdDistance, grab.stretch.GetValue());
}

void HandGrabSystem::PlayTakeSounds(entt::entity object)
{
	// A standing tree pulled from the ground creaks as the hand takes it, and a big forest gives a standing tree
	const auto kind = KindOf(object);
	if (kind == GrabKind::BigForest || (kind == GrabKind::Tree && !_world->IsFlying(object)))
	{
		return;
	}
	const auto position = _world->PoseOf(object).origin;
	_world->PlaySample(k_PickUpSample, position);
	// People cry out
	const auto& registry = _world->Entities();
	if (const auto* villager = registry.TryGet<const Villager>(object); villager != nullptr && _world->LifeOf(object) > 0.0f)
	{
		const uint32_t first = villager->lifeStage == Villager::LifeStage::Child ? k_ChildCries
		                       : villager->sex == Villager::Sex::FEMALE          ? k_WomanCries
		                                                                         : k_ManCries;
		_world->PlaySample(first + _world->LocalRandom(k_CriesEach), position);
	}
}

void HandGrabSystem::SendTake(HandGrab& grab, entt::entity object)
{
	// A thing that the hand can no longer take is let go of. It stays where the turns put it, as when the hand lets go
	// of it before it comes free.
	if (!MayTake(object))
	{
		Empty(grab);
		return;
	}
	// The hand plays the pick-up sound as it sends the take. It keeps its grip on the thing until the turn that does
	// the command.
	PlayTakeSounds(object);
	grab.state = HandGrab::State::Taking;
	grab.object = object;
	_world->SendCommand(player_commands::HandTake {.object = object});
}

void HandGrabSystem::DoTake(PlayerNames player, entt::entity object)
{
	// The command acts with the hand of its player
	const hand_grab::HandGrabWorldInterface::ActingPlayer acting(*_world, player);
	// Every computer takes the thing into the hand, whatever the hand of this computer does now. Only a hand that holds
	// nothing takes.
	auto* grab = Grab();
	if (grab == nullptr || player != _world->HandPlayer() || Holds(*grab))
	{
		return;
	}
	if (!Exists(object))
	{
		if (grab->object == object)
		{
			Empty(*grab);
		}
		return;
	}
	Take(*grab, object, true, false);
	if (Holds(*grab) && grab->object == object)
	{
		LogHandDeed(player, "takes", object);
	}
}

void HandGrabSystem::Take(HandGrab& grab, entt::entity object, bool fromTheWorld, bool sounds, bool askInfluence)
{
	auto& registry = _world->Entities();
	// The pull may have ended on a thing that can no longer be taken: the hand lets go of it. The turn asks the influence
	// at the point that the last report of the hand's player picked, the same on every computer.
	if (!hand_grab::PassesGate(GateAt(object, askInfluence ? _world->ReportedCollidePoint() : std::nullopt)))
	{
		Empty(grab);
		return;
	}
	// A big forest is never held itself: it gives the hand one of its trees, which the hand takes as it takes a tree from
	// the ground. Then the big forest's own fire, if it burns, starts moving as a held thing's does.
	if (KindOf(object) == GrabKind::BigForest)
	{
		if (const auto tree = _world->TreeFromBigForest(object); tree.has_value() && Exists(*tree))
		{
			Take(grab, *tree, true, sounds, askInfluence);
		}
		else
		{
			Empty(grab);
		}
		if (Exists(object))
		{
			_world->FireStartedMoving(object, true);
		}
		return;
	}
	const bool wasFlying = _world->IsFlying(object);
	const auto kind = KindOf(object);
	const auto player = _world->HandPlayer();

	// Out of the physics without landing, and out of the map's cells; it no longer flies past anyone
	if (fromTheWorld)
	{
		_world->LeavePhysicsAndMap(object);
		_world->RemoveReactions(object, Reaction::ReactToFlyingObject);
	}
	const auto position = _world->PoseOf(object).origin;

	// Its pick-up sound, when the hand did not send it as it sent the take
	if (fromTheWorld && sounds)
	{
		PlayTakeSounds(object);
	}

	// The people and animals about the hand see it pick something up
	const auto handPosition = _world->ReportedHandPlace();
	_world->CreateReaction(
	    {.initiator = _world->Hand(), .type = Reaction::ReactToHandPickUp, .player = player, .position = handPosition});
	// A burning thing keeps burning in the hand, out of its blaze; the people round it flee one that isn't a villager
	_world->FireStartedMoving(object, !registry.AllOf<Villager>(object));
	// A firefly hiding exactly where the thing stood is caught, for a miracle there
	_world->CatchFirefly(object);
	// The help counts each thing that the hand takes. A thing that the hand catches as it flies counts the same.
	_world->CountHelpEvent(help::profile::k_PickUp);
	// The hand starts the path of its gesture again
	_world->ForgetGesturePath();

	// The thing's own part of going into the hand
	if (registry.AllOf<Villager>(object))
	{
		// One of the hand's player's own people of an age to make love alarms those about it
		if (_world->IsSexuallyActive(object) && _world->PlayerOf(object) == player)
		{
			_world->CreateReaction(
			    {.initiator = object, .type = Reaction::ReactToVillagerInHand, .player = player, .position = position});
		}
		_world->VillagerIntoHand(object);
	}
	if (registry.AllOf<Animal>(object))
	{
		_world->AnimalIntoOwnFlock(object);
	}
	if (kind == GrabKind::Tree && !wasFlying)
	{
		_world->PlaySample(k_TreeCreaks + _world->LocalRandom(k_TreeCreaksEach), position);
		_world->TreeUprooted(player, object);
	}
	if (registry.AllOf<OneOffSpellSeed>(object))
	{
		_world->GlobeIntoHand(object, player);
	}
	if (registry.AllOf<Pot>(object))
	{
		// A pot no longer calls people to it while it is held
		_world->RemovePotReaction(object);
	}
	// A static, a rock or a dead tree taken stops being its town's artefact
	_world->ArtefactTaken(object, player);

	// The player of most influence where the thing was is where what the hand holds came from
	_world->NoteTakenAt(_world->PoseOf(object).origin);
	// It is held at its own size, and the frames draw it at the hand from now
	_world->ForgetPulledPose(object);
	auto pose = _world->PoseOf(object);
	pose.axes = _world->UnstretchedAxes(object, pose.axes);
	_world->SetPose(object, pose);
	// The thing keeps its place on the map while it is held, where a tree takes its light
	registry.AssignOrReplace<HeldLight>(object, HeldLight {.takenAt = pose.origin});
	const auto& held = registry.Get<const Transform>(object);
	registry.AssignOrReplace<InHand>(object, InHand {.hand = _world->Hand(), .rotation = held.rotation, .scale = held.scale});
	grab.state = HandGrab::State::Holding;
	grab.object = object;
	grab.tug.reset();
	grab.pulling = false;
	grab.hold = HoldOfObject(object);
	grab.lowering = grab.hold.loweringMultiplier * _world->SizeOf(object).height;
	grab.springOn = false;
	grab.springPending = false;
	grab.lastPickedUp = object;
	registry.SetDirty();
}

void HandGrabSystem::LetGo(HandGrab& grab, glm::vec3 velocity, const std::optional<player_commands::ThrowStart>& start)
{
	// A let-go with no start is one that the hand must do: the thing goes from where it is, and is not planted again
	const bool forced = !start.has_value();
	auto& registry = _world->Entities();
	const auto object = grab.object;
	Empty(grab);
	// It gets a twist a little after it leaves the hand, whatever becomes of it
	grab.released = object;
	grab.releaseSpinMs = static_cast<int32_t>(hand_grab::k_ReleaseSpinDelayMs);
	if (!Exists(object))
	{
		return;
	}
	// Out of the hand it keeps no light of the hand, on every computer
	registry.Remove<InHand>(object);
	registry.Remove<HeldLight>(object);
	// The help counts each thing that the hand lets go of, also a pour or a drop that the hand must do
	_world->CountHelpEvent(help::profile::k_Throw);
	// The hand starts the path of its gesture again. A thing that is gone when the hand lets go does not do this.
	_world->ForgetGesturePath();
	if (start.has_value())
	{
		// It starts from the start that the player's computer sent
		_world->StartThrowFrom(object, *start);
	}
	const auto player = _world->HandPlayer();
	if (!forced && registry.AllOf<Pot>(object))
	{
		// Let go over the land, a pot calls the people to it again before it is poured or thrown
		_world->SetUpPotReaction(object, player);
	}
	// A pot let go slowly is poured out where it is, onto what takes it or into a pile
	if (registry.AllOf<Pot>(object) && hand_grab::PotPours(velocity))
	{
		if (const auto pour = _world->PourPot(object, player))
		{
			grab.pourEffect = pour;
			grab.pourSeconds = 0.0f;
		}
		registry.SetDirty();
		return;
	}
	const auto result =
	    _world->LetGoFromHand(object, {
	                                      .velocity = velocity,
	                                      .angularMomentum = start.has_value() ? start->angularMomentum : glm::vec3(0.0f),
	                                      .player = player,
	                                      .dontReplant = forced,
	                                  });
	if (result.accepted)
	{
		grab.lastDropped = object;
	}
	// A player a script made proof against the wind throws things that fly without the air's drag
	if (_world->PlayerHasNoWindResistance(player))
	{
		_world->DropDrag(object);
	}
	registry.SetDirty();
}

void HandGrabSystem::Empty(HandGrab& grab)
{
	// The hand lets go of a thing that it gripped and did not take. The land's draw list is made again for two frames.
	if ((grab.state == HandGrab::State::Grabbing || grab.state == HandGrab::State::Taking) && grab.grips)
	{
		_world->RemakeDrawList(2);
	}
	// Whatever it offered, it offers no more
	ForgetOffer(grab);
	grab.offerTurn.Reset(0.0f);
	grab.lastHand.reset();
	grab.state = HandGrab::State::Empty;
	grab.object = entt::null;
	grab.waits = false;
	grab.pulling = false;
	grab.grips = false;
	grab.pullSeconds = 0.0f;
	grab.tug.reset();
	grab.rise = 0.0f;
	grab.springOn = false;
	grab.springPending = false;
	grab.scoopSource = entt::null;
	grab.scoopTurns = 0;
	grab.locking = entt::null;
	grab.predicted.reset();
}

glm::vec3 HandGrabSystem::UpdateFrame(const Frame& frame)
{
	// The input and the frames of this computer act with its own hand
	const hand_grab::HandGrabWorldInterface::ActingPlayer own(*_world, _world->OwnHandPlayer());
	auto* grab = Grab();
	if (grab == nullptr)
	{
		return frame.target;
	}
	grab->handSize = frame.handSize;
	grab->handPoint = frame.cursorGround;
	// In a game alone, a thing that the hand lets go of before it comes free stays drawn in the lean of the pull. In a
	// network game, the other computers did not see the lean, so the thing stands again as before the pull.
	const bool tugs = (grab->state == HandGrab::State::Grabbing || grab->state == HandGrab::State::Taking) &&
	                  grab->tug.has_value() && grab->object == _drawnPull;
	if (_drawnPull != entt::null && !tugs)
	{
		if (_world->IsNetworkGame())
		{
			_world->ForgetPulledPose(_drawnPull);
		}
		_drawnPull = entt::null;
	}
	// A pour lasts three quarters of a second of the game's time
	if (grab->pourEffect.has_value())
	{
		grab->pourSeconds += static_cast<float>(frame.gameMs) * 0.001f;
		if (grab->pourSeconds > hand_grab::k_PourSeconds)
		{
			_world->StopScoopStream(*grab->pourEffect);
			grab->pourEffect.reset();
		}
	}
	// A scoop's stream flows into the hand where it now is, for at most a minute
	if (grab->scoopStream.has_value())
	{
		grab->scoopStreamSeconds += static_cast<float>(frame.gameMs) * 0.001f;
		if (grab->scoopStreamSeconds > hand_grab::k_ScoopStreamSeconds)
		{
			_world->StopScoopStream(*grab->scoopStream);
			grab->scoopStream.reset();
		}
		else
		{
			_world->MoveScoopStream(*grab->scoopStream, _world->PoseOf(_world->Hand()).origin);
		}
	}

	// The twist of what it let go, once the hand has moved on a little
	if (grab->state == HandGrab::State::Grabbing || grab->state == HandGrab::State::Taking)
	{
		// While it pulls a thing the twist waits; while it waits to catch a flying one the twist is made ready again
		if (!grab->pulling)
		{
			grab->releaseSpinMs = static_cast<int32_t>(hand_grab::k_ReleaseSpinDelayMs);
		}
	}
	else if (grab->releaseSpinMs.has_value() && !Holds(*grab))
	{
		switch (hand_grab::CountDown(*grab->releaseSpinMs, frame.gameMs))
		{
		case hand_grab::Countdown::Running:
			break;
		case hand_grab::Countdown::RunOut:
			// The hand sends the twist with its motion. The turn turns the body by its mass and speed then.
			if (Exists(grab->released) && frame.cursorGround.has_value())
			{
				_world->SendCommand(
				    player_commands::HandTwist {.object = grab->released, .moved = *frame.cursorGround - grab->lastTarget});
			}
			grab->releaseSpinMs.reset();
			grab->released = entt::null;
			break;
		case hand_grab::Countdown::Cancelled:
			grab->releaseSpinMs.reset();
			grab->released = entt::null;
			break;
		}
	}

	switch (grab->state)
	{
	case HandGrab::State::Empty:
		grab->rise = 0.0f;
		return frame.target;
	case HandGrab::State::Grabbing:
	{
		if (!Exists(grab->object))
		{
			Empty(*grab);
			return frame.target;
		}
		// A thing in flight, or one the hand can't pull at, is taken once the button has been held a while; anything else
		// is taken as soon as the hand has pulled it free
		if (_world->IsFlying(grab->object) || !hand_grab::Tuggable(KindOf(grab->object)))
		{
			if (hand_grab::ElapsedMs(frame.nowMs, grab->pressMs, frame.turn, grab->pressTurn) < hand_grab::k_GrabWaitMs)
			{
				return frame.target;
			}
			SendTake(*grab, grab->object);
		}
		else if (grab->pulling)
		{
			if (!grab->tug.has_value())
			{
				StartPull(*grab, frame);
			}
			return Pull(*grab, frame);
		}
		else
		{
			SendTake(*grab, grab->object);
		}
		// Taken, the hand holds it from this frame, risen for it at once
		if (grab->state != HandGrab::State::Holding)
		{
			return frame.target;
		}
		break;
	}
	case HandGrab::State::Taking:
		if (!Exists(grab->object))
		{
			Empty(*grab);
			return frame.target;
		}
		// The hand keeps its grip where the pull left the thing, until the turn that takes the thing
		if (grab->tug.has_value())
		{
			return hand_grab::TugHandPoint(*grab->tug, grab->holdDistance, grab->stretch.GetValue());
		}
		return frame.target;
	case HandGrab::State::Holding:
	case HandGrab::State::ReadyToThrow:
	case HandGrab::State::LettingGo:
		break;
	}

	// Offered to a creature, the hand may be drawn to its hand as it reaches
	const auto offered = UpdateOffer(*grab, frame);
	// The hand rises by how far what it holds hung last frame, which it then measures again
	const auto* tree = _world->Entities().TryGet<const Tree>(grab->object);
	const auto size = _world->SizeOf(grab->object);
	const std::optional<float> rooted = tree != nullptr ? std::optional(size.height) : std::nullopt;
	grab->rise = hand_grab::HandRise(grab->hold.type, grab->lowering, frame.handSize, rooted);
	grab->lowering = grab->hold.loweringMultiplier * size.height;
	// While it holds something its twist is made ready again, for when it lets go
	grab->releaseSpinMs = static_cast<int32_t>(hand_grab::k_ReleaseSpinDelayMs);
	const auto lastTarget = std::exchange(grab->lastTarget, frame.target);
	auto target = offered + glm::vec3(0.0f, grab->rise, 0.0f);
	// The game time spent holding is counted whether the spring is on or not, scooping too
	grab->spring.Count(frame.gameMs);
	if (grab->scoopSource != entt::null && Exists(grab->scoopSource))
	{
		// While the hand scoops, it stays where it started. Its height is the land, plus the height of the source, plus
		// the rise of the hold.
		target = grab->scoopAnchor;
		target.y = _world->LandHeightAt(target) + _world->SizeOf(grab->scoopSource).height + grab->rise;
		grab->lastTarget = target;
		return target;
	}
	if (grab->springOn)
	{
		// The spring's own throw takes the place of a recorded one as soon as it moves the hand
		grab->recordedThrow.reset();
		// The hand went from its last place to this one in the frame, with its rise and the pull of the offer as now
		grab->spring.Step(lastTarget + (target - frame.target), target, frame.inputSteps);
		grab->lastHand = grab->spring.Position();
		return grab->spring.Position();
	}
	if (grab->springPending && grab->state == HandGrab::State::ReadyToThrow)
	{
		// The spring takes hold at the hand where it now is, at rest, and steps from the next frame
		grab->spring.Start(target);
		grab->springOn = true;
		grab->springPending = false;
	}
	grab->lastHand = target;
	return target;
}

glm::vec3 HandGrabSystem::UpdateOffer(HandGrab& grab, const Frame& frame)
{
	// Once given, the creature reaches for it until it has it or gives up
	if (grab.givenTo != entt::null && !_world->StillTaking(grab.givenTo))
	{
		ForgetOffer(grab);
	}
	const bool reaching = grab.givenTo != entt::null;
	// Over a creature that would take it, the offer lasts its full time again; otherwise it runs down on the frame's time
	const auto under = _world->ObjectUnderCursor();
	const auto verdict =
	    under.has_value() && *under != grab.object && Exists(*under) ? _world->GiveVerdict(*under, grab.object) : std::nullopt;
	if (verdict == creature_giving::Verdict::Give)
	{
		grab.offeredTo = *under;
		grab.offer.Renew();
	}
	else
	{
		if (verdict == creature_giving::Verdict::CantPickUp)
		{
			_world->SayCantPickUp(*under);
		}
		const auto frameMs = static_cast<int32_t>(std::lround(frame.seconds * 1000.0f));
		if (grab.offer.RunDown(frameMs, reaching))
		{
			grab.offeredTo = entt::null;
		}
	}
	// The held thing turns over towards the creature, and back as the offer ends
	if (const auto turn = creature_giving::OfferTurn(grab.hold.type, _world->RightHanded(), grab.offer, reaching))
	{
		grab.offerTurn.SetDestination(*turn, creature_giving::k_OfferTurnSeconds);
	}
	grab.offerTurn.UpdateInline(frame.seconds);
	if (!reaching)
	{
		return frame.target;
	}
	// While the creature reaches, the hand meets its hand at the moment it takes the thing; moved too far from the
	// creature's hand, the giving is called off
	const auto hand = grab.lastHand.value_or(frame.target);
	const auto takePoint = _world->TakePointOf(grab.givenTo, hand);
	if (!takePoint.has_value())
	{
		return frame.target;
	}
	if (creature_giving::CallsOff(hand, *takePoint, _world->CreatureSizeOf(grab.givenTo)))
	{
		// The turn calls off the gift on every computer. Until then the hand sends the call off one time only.
		if (grab.callingOff != grab.givenTo)
		{
			grab.callingOff = grab.givenTo;
			_world->SendCommand(player_commands::GiveCallOff {.creature = grab.givenTo});
		}
		return frame.target;
	}
	const auto taking = _world->TakingOf(grab.givenTo);
	if (!taking.has_value() || taking->object != grab.object)
	{
		return frame.target;
	}
	return creature_giving::DrawToCreature(*takePoint, frame.target, taking->msFromTake, grab.lowering).value_or(frame.target);
}

bool HandGrabSystem::Give(HandGrab& grab, std::optional<entt::entity> under)
{
	const auto held = grab.object;
	if (!Exists(held) || grab.givenTo != entt::null)
	{
		return false;
	}
	// The creature it offers it to comes first, then the thing under the hand
	auto creature = grab.offer.Active() && Exists(grab.offeredTo) ? std::optional(grab.offeredTo) : std::nullopt;
	if (!creature.has_value() && under.has_value() && *under != held && Exists(*under))
	{
		creature = under;
	}
	if (!creature.has_value() || _world->GiveVerdict(*creature, held) != creature_giving::Verdict::Give)
	{
		return false;
	}
	// The side of the hand of this computer goes with the give, as the turn of every computer reads it
	_world->SendCommand(player_commands::HandGive {.creature = *creature, .object = held, .leftHand = !_world->RightHanded()});
	return true;
}

void HandGrabSystem::DoGive(PlayerNames player, entt::entity creature, entt::entity object, bool leftHand)
{
	// The command acts with the hand of its player
	const hand_grab::HandGrabWorldInterface::ActingPlayer acting(*_world, player);
	auto* grab = Grab();
	if (grab == nullptr || player != _world->HandPlayer() || !Holds(*grab) || grab->object != object ||
	    grab->givenTo != entt::null || !Exists(object) || !Exists(creature) ||
	    _world->GiveVerdict(creature, object) != creature_giving::Verdict::Give)
	{
		return;
	}
	// The help counts the gift as the creature may take it, before the creature starts to take it
	_world->CountHelpEvent(help::profile::k_Give);
	if (!_world->GiveTo(creature, object, leftHand))
	{
		return;
	}
	grab->offeredTo = creature;
	grab->givenTo = creature;
	grab->callingOff = entt::null;
}

void HandGrabSystem::DoGiveCallOff(PlayerNames player, entt::entity creature)
{
	// The command acts with the hand of its player
	const hand_grab::HandGrabWorldInterface::ActingPlayer acting(*_world, player);
	if (Exists(creature))
	{
		if (const auto logger = spdlog::get("game"))
		{
			SPDLOG_LOGGER_INFO(logger, "The hand of player {} calls off the gift to creature {}", static_cast<int>(player),
			                   entt::to_integral(creature));
		}
		_world->CallOffGiving(creature);
	}
	// The hand of the player forgets the gift, whatever it gives now
	if (auto* grab = Grab(); grab != nullptr)
	{
		ForgetOffer(*grab);
	}
}

void HandGrabSystem::InterfaceTurn(uint32_t nowMs, uint32_t turn)
{
	// The turn forgets the old clicks of the hand of every player, the same at each frame rate and on every computer
	{
		static constexpr auto k_MillisecondsPerTurn = static_cast<uint32_t>(TimeSystemInterface::k_TurnDuration.count());
		_world->Entities().Each<HandClicked>(
		    [turn](HandClicked& clicked) { hand_click::Forget(clicked, turn, k_MillisecondsPerTurn); });
	}
	// The input and the frames of this computer act with its own hand
	const hand_grab::HandGrabWorldInterface::ActingPlayer own(*_world, _world->OwnHandPlayer());
	auto* grab = Grab();
	if (grab == nullptr)
	{
		return;
	}
	// A thing in flight, or one that the hand cannot pull at, is taken once the button was held a while. A thing that the
	// pull freed is taken at once.
	if (grab->state == HandGrab::State::Grabbing && Exists(grab->object))
	{
		if (_world->IsFlying(grab->object) || !hand_grab::Tuggable(KindOf(grab->object)))
		{
			if (!(hand_grab::ElapsedMs(nowMs, grab->pressMs, turn, grab->pressTurn) < hand_grab::k_GrabWaitMs))
			{
				SendTake(*grab, grab->object);
			}
		}
		else if (!grab->pulling)
		{
			SendTake(*grab, grab->object);
		}
	}
}

void HandGrabSystem::ForgetOffer(HandGrab& grab)
{
	grab.offeredTo = entt::null;
	grab.givenTo = entt::null;
	grab.callingOff = entt::null;
	grab.offer = {};
}

bool HandGrabSystem::HandOver(entt::entity object)
{
	// The hand that holds the thing gives it, whichever player's hand it is
	const auto* inHand = Exists(object) ? _world->Entities().TryGet<const InHand>(object) : nullptr;
	const hand_grab::HandGrabWorldInterface::ActingPlayer holder(*_world, inHand != nullptr ? _world->PlayerOfHand(inHand->hand)
	                                                                                        : std::nullopt);
	auto* grab = Grab();
	if (grab == nullptr || grab->object != object || !Holds(*grab))
	{
		return false;
	}
	// A scoop ends with it
	if (grab->scoopSource != entt::null)
	{
		EndScoop(*grab);
	}
	_world->Entities().Remove<InHand>(object);
	Empty(*grab);
	return true;
}

bool HandGrabSystem::TakeIntoHand(entt::entity object)
{
	auto* grab = Grab();
	if (grab == nullptr || grab->state != HandGrab::State::Empty || !Exists(object) || !MayTake(object))
	{
		return false;
	}
	Take(*grab, object, true);
	return grab->state == HandGrab::State::Holding;
}

bool HandGrabSystem::TakeIntoHandOf(PlayerNames player, entt::entity object)
{
	const hand_grab::HandGrabWorldInterface::ActingPlayer acting(*_world, player);
	auto* grab = Grab();
	if (grab == nullptr || grab->state != HandGrab::State::Empty || !Exists(object))
	{
		return false;
	}
	Take(*grab, object, true, true, false);
	return grab->state == HandGrab::State::Holding;
}

bool HandGrabSystem::GiveHeldTo(entt::entity creature)
{
	auto* grab = Grab();
	return grab != nullptr && grab->state == HandGrab::State::Holding && Give(*grab, creature);
}

std::optional<entt::entity> HandGrabSystem::GetOfferedCreature() const
{
	// The input and the frames of this computer act with its own hand
	const hand_grab::HandGrabWorldInterface::ActingPlayer own(*_world, _world->OwnHandPlayer());
	const auto* grab = Grab();
	if (grab == nullptr || !Holds(*grab))
	{
		return std::nullopt;
	}
	if (grab->givenTo != entt::null)
	{
		return grab->givenTo;
	}
	return grab->offer.Active() && grab->offeredTo != entt::null ? std::optional(grab->offeredTo) : std::nullopt;
}

bool HandGrabSystem::OffersToCreature() const
{
	// The input and the frames of this computer act with its own hand
	const hand_grab::HandGrabWorldInterface::ActingPlayer own(*_world, _world->OwnHandPlayer());
	const auto* grab = Grab();
	return grab != nullptr && Holds(*grab) && grab->offer.Active() && grab->offeredTo != entt::null;
}

float HandGrabSystem::GetOfferTurn() const
{
	// The input and the frames of this computer act with its own hand
	const hand_grab::HandGrabWorldInterface::ActingPlayer own(*_world, _world->OwnHandPlayer());
	const auto* grab = Grab();
	return grab != nullptr && grab->state != HandGrab::State::Empty ? grab->offerTurn.GetValue() : 0.0f;
}

void HandGrabSystem::ProcessTurn()
{
	// The hand of each god with a human: the first player's, then the others' in the order of their players
	ProcessHandTurn();
	for (const auto player : _world->OtherHandPlayers())
	{
		const hand_grab::HandGrabWorldInterface::ActingPlayer acting(*_world, player);
		ProcessHandTurn();
	}
}

void HandGrabSystem::ProcessHandTurn()
{
	auto* grab = Grab();
	if (grab == nullptr)
	{
		return;
	}
	if (!Exists(grab->lastPickedUp))
	{
		grab->lastPickedUp = entt::null;
	}
	if (!Exists(grab->lastDropped))
	{
		grab->lastDropped = entt::null;
	}
	if (!Holds(*grab))
	{
		return;
	}
	// What is no longer there leaves the hand
	if (!Exists(grab->object))
	{
		EndScoop(*grab);
		Empty(*grab);
		return;
	}
	// A scoop goes on while its source is there, the hand is in its player's influence and the source gives more
	if (grab->scoopSource != entt::null)
	{
		++grab->scoopTurns;
		const auto hand = _world->ReportedHandPlace();
		if (!Exists(grab->scoopSource) || !_world->InInfluence(_world->HandPlayer(), hand) || !Scoop(*grab))
		{
			EndScoop(*grab);
		}
	}
	// The hold is asked again, as what is held may have changed
	grab->hold = HoldOfObject(grab->object);
	// What is held over a fire catches from it, inside the holder's influence only
	if (_world->InInfluence(_world->HandPlayer(), _world->PoseOf(grab->object).origin))
	{
		_world->HeatHeld(grab->object);
	}
}

void HandGrabSystem::DoTwist(PlayerNames player, entt::entity object, glm::vec3 moved)
{
	// The command acts with the hand of its player
	const hand_grab::HandGrabWorldInterface::ActingPlayer acting(*_world, player);
	if (player != _world->HandPlayer() || !Exists(object))
	{
		return;
	}
	if (const auto body = _world->BodyOf(object); body.has_value())
	{
		_world->TwistBody(object, hand_grab::ReleaseSpinTorque(body->mass, body->speed, moved));
	}
}

void HandGrabSystem::PlaceHeldForTurn()
{
	// The frames draw a held thing at the hand. The turn has it where the last report put the hand, at the turn and size
	// that it had as the hand took it. The place has the precision of a map position, and the height of the reported
	// point above the ground.
	// A thing that the hand of this computer pulls at is not changed here. Only this computer knows of the pull, and
	// the frames only draw the lean. Thus a pulled villager continues to walk in the same way on every computer.
	// Each thing at the place of the hand that holds it
	auto& registry = _world->Entities();
	registry.Each<const InHand, Transform>([this](const InHand& inHand, Transform& transform) {
		transform.position = _world->HeldPlaceAt(_world->ReportedPlaceOf(inHand.hand));
		transform.rotation = inHand.rotation;
		transform.scale = inHand.scale;
	});
}

void HandGrabSystem::PlaceOthersHeldForFrame(int32_t frameMs)
{
	auto& registry = _world->Entities();
	registry.Each<InHand>([this, &registry, frameMs](InHand& inHand) {
		if (!Exists(inHand.hand) || !_world->IsOtherPlayersHand(inHand.hand))
		{
			return;
		}
		const auto reported = _world->ReportedPlaceOf(inHand.hand);
		auto* track = registry.TryGet<HeldDrawTrack>(inHand.hand);
		const auto point = track != nullptr ? hand_grab::other_held::Step(*track, reported, frameMs) : reported;
		// Only the draw reads this place. The turns keep the thing at the reported place of the hand.
		inHand.drawn = InHand::Drawn {.position = _world->AsMapPlace(point), .rotation = glm::mat3(1.0f)};
	});
	// A miracle seed in the hand of another god moves the same way. Its model shows only for a miracle that shows one
	// in the hand; this computer draws no effect of it, as the hand of another god is not drawn.
	registry.Each<const HandHeldSeed>([this, &registry, frameMs](entt::entity hand, const HandHeldSeed& held) {
		if (!registry.Valid(held.seed) || !_world->IsOtherPlayersHand(hand))
		{
			return;
		}
		auto* transform = registry.TryGet<Transform>(held.seed);
		if (transform == nullptr)
		{
			return;
		}
		const auto reported = _world->ReportedPlaceOf(hand);
		auto* track = registry.TryGet<HeldDrawTrack>(hand);
		const auto point = track != nullptr ? hand_grab::other_held::Step(*track, reported, frameMs) : reported;
		transform->position = _world->AsMapPlace(point);
		transform->rotation = glm::mat3(1.0f);
	});
}

void HandGrabSystem::DrawThrownForFrame()
{
	// The input and the frames of this computer act with its own hand
	const hand_grab::HandGrabWorldInterface::ActingPlayer own(*_world, _world->OwnHandPlayer());
	const auto* grab = Grab();
	if (grab == nullptr || grab->state != HandGrab::State::LettingGo || !grab->predicted.has_value() || !Exists(grab->object))
	{
		return;
	}
	// The hand does not move the thing. The thing goes along the flight from the turn of the send.
	_world->DrawThrownPose(
	    grab->object, hand_grab::throw_lead::DrawnPose(*grab->predicted, _world->FrameTurn(), _world->FrameTurnFraction()));
}

void HandGrabSystem::SetRecordedThrow(glm::vec3 velocity)
{
	// The input and the frames of this computer act with its own hand
	const hand_grab::HandGrabWorldInterface::ActingPlayer own(*_world, _world->OwnHandPlayer());
	if (auto* grab = Grab(); grab != nullptr)
	{
		grab->recordedThrow = velocity;
	}
}

void HandGrabSystem::ForceDrop()
{
	auto* grab = Grab();
	if (grab == nullptr || !Holds(*grab))
	{
		return;
	}
	// A scoop ends with it
	if (grab->scoopSource != entt::null)
	{
		EndScoop(*grab);
	}
	// Put down from where it is held, with no speed, and never planted again
	LetGo(*grab, glm::vec3(0.0f), std::nullopt);
}

void HandGrabSystem::ForceDropFromEveryHand(entt::entity object)
{
	const auto dropIfHeld = [this, object] {
		if (const auto* grab = Grab(); grab != nullptr && Holds(*grab) && grab->object == object)
		{
			ForceDrop();
		}
	};
	// The hand of each god with a human, in the order of the turn: the first player's, then the others'
	{
		const hand_grab::HandGrabWorldInterface::ActingPlayer first(*_world, PlayerNames::PLAYER_ONE);
		dropIfHeld();
	}
	for (const auto player : _world->OtherHandPlayers())
	{
		const hand_grab::HandGrabWorldInterface::ActingPlayer acting(*_world, player);
		dropIfHeld();
	}
}

void HandGrabSystem::DoShakeOff(PlayerNames player)
{
	// The command acts with the hand of the player that sent it
	const hand_grab::HandGrabWorldInterface::ActingPlayer acting(*_world, player);
	ForceDrop();
}

bool HandGrabSystem::LetGoAt(glm::vec3 velocity)
{
	// The input and the frames of this computer act with its own hand
	const hand_grab::HandGrabWorldInterface::ActingPlayer own(*_world, _world->OwnHandPlayer());
	auto* grab = Grab();
	if (grab == nullptr || !Holds(*grab))
	{
		return false;
	}
	if (grab->scoopSource != entt::null)
	{
		EndScoop(*grab);
	}
	// This let go does not wait for a turn, thus it has no lead
	const auto led = ThrowStartOf(grab->object, velocity, 0);
	LetGo(*grab, led.velocity, led.start);
	return true;
}

void HandGrabSystem::Reset()
{
	_drawnPull = entt::null;
	if (auto* grab = Grab())
	{
		if (grab->scoopSource != entt::null)
		{
			_world->PinCursor(false);
		}
		*grab = HandGrab {};
	}
}

std::optional<entt::entity> HandGrabSystem::GetHeldOf(PlayerNames player) const
{
	const hand_grab::HandGrabWorldInterface::ActingPlayer acting(*_world, player);
	return HeldNow();
}

bool HandGrabSystem::IsInInfluenceOf(PlayerNames player) const
{
	const hand_grab::HandGrabWorldInterface::ActingPlayer acting(*_world, player);
	return HandInInfluence();
}

std::optional<entt::entity> HandGrabSystem::GetHeld() const
{
	// The input and the frames of this computer act with its own hand
	const hand_grab::HandGrabWorldInterface::ActingPlayer own(*_world, _world->OwnHandPlayer());
	return HeldNow();
}

std::optional<entt::entity> HandGrabSystem::HeldNow() const
{
	const auto* grab = Grab();
	if (grab == nullptr || !Holds(*grab))
	{
		return std::nullopt;
	}
	return grab->object;
}

std::optional<entt::entity> HandGrabSystem::GetLockedOn() const
{
	// The input and the frames of this computer act with its own hand
	const hand_grab::HandGrabWorldInterface::ActingPlayer own(*_world, _world->OwnHandPlayer());
	return LockedOnNow();
}

std::optional<entt::entity> HandGrabSystem::GetLockedOnOf(PlayerNames player) const
{
	const hand_grab::HandGrabWorldInterface::ActingPlayer acting(*_world, player);
	return LockedOnNow();
}

std::optional<entt::entity> HandGrabSystem::LockedOnNow() const
{
	const auto* grab = Grab();
	if (grab == nullptr || grab->scoopSource == entt::null || !Holds(*grab))
	{
		return std::nullopt;
	}
	return grab->scoopSource;
}

bool HandGrabSystem::IsBusy() const
{
	// The input and the frames of this computer act with its own hand
	const hand_grab::HandGrabWorldInterface::ActingPlayer own(*_world, _world->OwnHandPlayer());
	const auto* grab = Grab();
	return grab != nullptr && grab->state != HandGrab::State::Empty;
}

bool HandGrabSystem::IsBusyOf(PlayerNames player) const
{
	const hand_grab::HandGrabWorldInterface::ActingPlayer acting(*_world, player);
	const auto* grab = Grab();
	return grab != nullptr && grab->state != HandGrab::State::Empty;
}

bool HandGrabSystem::IsPulling() const
{
	// The input and the frames of this computer act with its own hand
	const hand_grab::HandGrabWorldInterface::ActingPlayer own(*_world, _world->OwnHandPlayer());
	const auto* grab = Grab();
	// Once it has come free the pull is over, though the hand takes it a frame later
	return grab != nullptr && grab->state == HandGrab::State::Grabbing && grab->pulling;
}

bool HandGrabSystem::IsAboutToThrow() const
{
	// The input and the frames of this computer act with its own hand
	const hand_grab::HandGrabWorldInterface::ActingPlayer own(*_world, _world->OwnHandPlayer());
	const auto* grab = Grab();
	return grab != nullptr && grab->state == HandGrab::State::ReadyToThrow;
}

bool HandGrabSystem::CreatureTakesHeld(entt::entity creature) const
{
	// The input and the frames of this computer act with its own hand
	const hand_grab::HandGrabWorldInterface::ActingPlayer own(*_world, _world->OwnHandPlayer());
	const auto held = HeldNow();
	if (!held.has_value() || !Exists(*held) || !Exists(creature))
	{
		return false;
	}
	const auto verdict = _world->GiveVerdict(creature, *held);
	return verdict.has_value() && *verdict == creature_giving::Verdict::Give;
}

std::optional<HandGrabSystemInterface::HeldPose> HandGrabSystem::GetHeldPose() const
{
	// The input and the frames of this computer act with its own hand
	const hand_grab::HandGrabWorldInterface::ActingPlayer own(*_world, _world->OwnHandPlayer());
	const auto held = GetHeld();
	if (!held.has_value() || !Exists(*held))
	{
		return std::nullopt;
	}
	const auto& grab = *Grab();
	// It hangs its height times its lowering below the hand
	return HeldPose {
	    .object = *held,
	    .hold = grab.hold.type,
	    .hang = grab.hold.loweringMultiplier * _world->SizeOf(*held).height,
	    .reach = grab.hold.holdRadius,
	};
}

std::optional<HandGrabSystemInterface::PullPose> HandGrabSystem::GetPullPose() const
{
	// The input and the frames of this computer act with its own hand
	const hand_grab::HandGrabWorldInterface::ActingPlayer own(*_world, _world->OwnHandPlayer());
	const auto* grab = Grab();
	// Until the hand holds it, including the frame it comes free, the hand keeps to its pull
	if (grab == nullptr || (grab->state != HandGrab::State::Grabbing && grab->state != HandGrab::State::Taking) ||
	    !grab->tug.has_value() || !Exists(grab->object))
	{
		return std::nullopt;
	}
	return PullPose {.hold = grab->hold.type, .reach = grab->hold.holdRadius};
}

float HandGrabSystem::GetCursorRaise() const
{
	// The input and the frames of this computer act with its own hand
	const hand_grab::HandGrabWorldInterface::ActingPlayer own(*_world, _world->OwnHandPlayer());
	const auto* grab = Grab();
	return grab != nullptr ? hand_grab::CursorRaise(grab->rise) : 0.0f;
}
