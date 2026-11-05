/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <array>
#include <iterator>
#include <limits>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <spdlog/spdlog.h>

#include "Commands/PlayerCommands.h"
#include "Common/GUtilsDistance.h"
#include "Creature/FightMarks.h"
#include "CreatureFightSystem.h"
#include "CreatureFightSystemDetail.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreatureHome.h"
#include "ECS/CreaturePlacement.h"
#include "ECS/CreatureScars.h"
#include "ECS/Map.h"
#include "ECS/PosedModel.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/FightTranscriptNote.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "ECS/ThingCircles.h"
#include "ECS/ThingMapPlace.h"
#include "ECS/ThingModel.h"
#include "ECS/WorldObjects.h"
#include "Input/GameActionMapInterface.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Sound/GameSoundEffects.h"
#include "Sound/SoundRequestInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
namespace fight = openblack::creature_fight;
namespace marks = openblack::creature_fight::marks;
namespace posed_model = openblack::ecs::posed_model;
using namespace openblack::ecs::systems::fight_detail;

namespace
{
/// A press on the ground counts as a step within this many of the arena's radii of its middle
constexpr float k_GroundClickRadii = 1.5f;

/// Where a line first meets a creature's skin as it is drawn, tested against every triangle of its nearest level of
/// detail as posed this frame
std::optional<posed_model::SkinHit> SkinHitOn(const ecs::Registry& registry, entt::entity creature, const glm::vec3& origin,
                                              const glm::vec3& direction)
{
	const auto drawn = DrawnMeshOf(registry, creature);
	if (!drawn.has_value())
	{
		return std::nullopt;
	}
	return posed_model::NearestSkinHit(registry, creature, *drawn->first, drawn->second, origin, direction);
}

/// A sparkle sitting where a line met a creature's skin, kept on that triangle so that it moves with the skin
marks::Mark MarkOnSkin(const posed_model::SkinHit& hit, marks::Anchor anchor)
{
	return {.anchor = anchor, .subMesh = hit.subMesh, .firstIndex = hit.hit.firstIndex, .s = hit.hit.s, .t = hit.hit.t};
}

/// Whether the hand of the player holds nothing: no thing and no miracle. A hand that only starts to take a thing holds
/// nothing yet.
bool HandHoldsNothing(const ecs::Registry& registry)
{
	if (Locator::magicSystem::has_value() && Locator::magicSystem::value().GetHeldSeedOf(OwnPlayer()).has_value())
	{
		return false;
	}
	if (!Locator::handSystem::has_value())
	{
		return true;
	}
	const auto hand = Locator::handSystem::value().GetOwnHands()[static_cast<size_t>(HandSystemInterface::Side::Left)];
	const auto* grab = registry.Valid(hand) ? registry.TryGet<const HandGrab>(hand) : nullptr;
	return grab == nullptr || grab->state == HandGrab::State::Empty || grab->state == HandGrab::State::Grabbing ||
	       grab->state == HandGrab::State::Taking;
}

/// The creature the hand is over, as the interface picked it when the last frame was drawn
std::optional<entt::entity> CreatureUnderHand(const ecs::Registry& registry)
{
	if (!Locator::pickingSystem::has_value())
	{
		return std::nullopt;
	}
	const auto& picked = Locator::pickingSystem::value().GetPick().object;
	if (!picked.has_value() || !registry.Valid(*picked) || !registry.AllOf<Creature>(*picked))
	{
		return std::nullopt;
	}
	return picked;
}

/// The sound of the hand directing a fight: heard the same wherever the click was
constexpr auto k_DirectSound = entt::hashed_string("InGame.sad/1");

void PlayDirectSound()
{
	if (Locator::sounds::has_value())
	{
		// Each click is a sound of its own to the voice rules
		audio::StartGameSoundEffect(k_DirectSound.value(), {.voiceOwner = audio::VoiceOwner::OwnSound});
	}
}

/// The miracles of the fight that the creature knows. A creature with no mind, or a game with no tables, knows none.
std::vector<MagicType> KnownFightMagic(const ecs::Registry& registry, entt::entity creature)
{
	std::vector<MagicType> known;
	const auto* mind = registry.TryGet<const CreatureMindState>(creature);
	if (mind == nullptr || !mind->learnt.has_value() || !Locator::infoConstants::has_value())
	{
		return known;
	}
	for (const auto& miracle :
	     magic::KnownFightMiracles(Locator::infoConstants::value(), mind->learnt->knowledge.miraclesKnown))
	{
		known.push_back(miracle.magic);
	}
	return known;
}
} // namespace

std::optional<entt::entity> CreatureFightSystem::PlayersFighter() const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto creature = PrimaryCreatureOf(registry, OwnPlayer());
	if (!creature.has_value() || !InDuel(registry, *creature))
	{
		return std::nullopt;
	}
	const auto opponent = registry.Get<const CreatureFighting>(*creature).opponent;
	return registry.Valid(opponent) ? creature : std::nullopt;
}

bool CreatureFightSystem::DirectsFight() const
{
	return fight::DirectsFight(PlayersFighter().has_value(), _view.has_value(),
	                           HandHoldsNothing(Locator::entitiesRegistry::value()));
}

bool CreatureFightSystem::Press(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, fight::Button button,
                                uint32_t milliseconds, uint32_t turn)
{
	auto& registry = Locator::entitiesRegistry::value();
	// The transcript keeps each press while the player's creature is in a fight, with what the fight did with it
	const auto note = [&](std::string_view result, fight_transcript::Fields more) {
		if (!fight_transcript::Recording())
		{
			return;
		}
		const auto own = PrimaryCreatureOf(registry, OwnPlayer());
		if (!own.has_value() || !registry.AllOf<CreatureFighting>(*own))
		{
			return;
		}
		const auto pointer =
		    Locator::gameActionSystem::has_value() ? Locator::gameActionSystem::value().GetPointerPosition() : glm::ivec2(0);
		fight_transcript::Fields fields {{"creature", fight_transcript::Id(*own)},
		                                 {"button", button == fight::Button::Action ? "action" : "move"},
		                                 {"result", result},
		                                 {"pointer_x", pointer.x},
		                                 {"pointer_y", pointer.y},
		                                 {"ray_x", rayOrigin.x},
		                                 {"ray_y", rayOrigin.y},
		                                 {"ray_z", rayOrigin.z},
		                                 {"direction_x", rayDirection.x},
		                                 {"direction_y", rayDirection.y},
		                                 {"direction_z", rayDirection.z}};
		std::ranges::move(more, std::back_inserter(fields));
		Locator::fightTranscript::value().Note(*own, "press", std::move(fields), std::nullopt);
	};
	const auto self = PlayersFighter();
	if (!self.has_value() || !DirectsFight())
	{
		note("refused_gate", {{"duelling", self.has_value()},
		                      {"camera_shows_fight", _view.has_value()},
		                      {"hand_empty", HandHoldsNothing(registry)}});
		return false;
	}
	auto& fighting = registry.Get<CreatureFighting>(*self);
	// Nothing more is taken while twelve sparkles show
	if (fighting.marks.Full())
	{
		note("refused_full", {{"queue", static_cast<uint32_t>(fighting.fighter.queue.Size())}});
		return false;
	}
	const auto opponent = fighting.opponent;
	const auto& selfAt = registry.Get<const Transform>(*self).position;
	const auto& opponentAt = registry.Get<const Transform>(opponent).position;
	const auto opponentHeight = HeightOf(ShownSize(registry.Get<const Creature>(opponent)));
	// Over the player's creature or its opponent, the press is theirs: it counts only where the line meets their skin
	const auto under = CreatureUnderHand(registry);
	if (under.has_value() && *under != *self && *under != opponent)
	{
		note("refused_other_creature", {{"under", fight_transcript::Id(*under)}});
		return false;
	}

	const bool replace = fight::ReplacesQueue(button);
	marks::Mark mark;
	fight_transcript::Fields target;
	std::optional<player_commands::Action> command;
	if (under.has_value())
	{
		const auto hit = SkinHitOn(registry, *under, rayOrigin, rayDirection);
		if (!hit.has_value())
		{
			note("missed_skin", {{"target", *under == opponent ? "opponent" : "self"}});
			return true;
		}
		fight::Move move;
		if (*under == opponent)
		{
			// High, in the middle or low by where on the opponent's body the press lands
			const auto share = (hit->hit.point.y - opponentAt.y) / opponentHeight;
			move = fight::AttackMove(fight::BandOf(share));
			mark = MarkOnSkin(*hit, marks::Anchor::Opponent);
			target = {{"target", "opponent"}, {"height_share", share}, {"part", fight::Name(fight::BandOf(share))}};
		}
		else
		{
			move = fight::BlockMove();
			mark = MarkOnSkin(*hit, marks::Anchor::Self);
			target = {{"target", "self"}};
		}
		target.emplace_back("hit_x", hit->hit.point.x);
		target.emplace_back("hit_y", hit->hit.point.y);
		target.emplace_back("hit_z", hit->hit.point.z);
		target.emplace_back("move", fight::Name(move.kind));
		command = player_commands::FightMove {
		    .target = *under, .kind = static_cast<uint8_t>(move.kind), .value = move.value, .replace = replace};
	}
	else
	{
		// The arena's ground: a step towards where it was pressed
		if (rayDirection.y >= 0.0f)
		{
			note("refused_sky", {});
			return false;
		}
		const auto along = (selfAt.y - rayOrigin.y) / rayDirection.y;
		const auto point = Flat(rayOrigin + (rayDirection * along));
		if (!fight::GroundPressCounts(fighting.arena, point))
		{
			note("refused_outside_arena", {{"ground_x", point.x}, {"ground_z", point.y}});
			return false;
		}
		const auto heading = registry.Get<const CreatureLocomotion>(*self).heading;
		const glm::vec3 ground {point.x, GroundAt(point), point.y};
		mark = {.anchor = marks::Anchor::Ground, .local = marks::ToLocal(marks::GroundFrame(selfAt, heading), ground)};
		target = {{"target", "ground"}, {"ground_x", point.x}, {"ground_z", point.y}};
		command = player_commands::FightGround {.creature = *self, .point = ground, .replace = replace};
	}
	// The sparkle goes up first; a press on a body charges it while held, one on the ground does not. The move itself
	// goes into the queue at the next turn.
	fighting.marks.Add(mark);
	_markHeld = mark.anchor != marks::Anchor::Ground;
	PlayDirectSound();
	note("accepted", std::move(target));
	Send(*command);
	_pressed = Pressed {.creature = *self, .milliseconds = milliseconds, .turn = turn};
	return true;
}

void CreatureFightSystem::Release(uint32_t milliseconds, uint32_t turn)
{
	if (!_pressed.has_value())
	{
		return;
	}
	const auto pressed = *_pressed;
	_pressed.reset();
	_markHeld = false;
	const auto realMs = milliseconds >= pressed.milliseconds ? milliseconds - pressed.milliseconds : 0u;
	const auto turns = turn >= pressed.turn ? turn - pressed.turn : 0u;
	const auto heldMs = fight::HeldMs(static_cast<float>(realMs), turns);
	fight_transcript::Note(pressed.creature, "release", [&] {
		return fight_transcript::Fields {{"creature", fight_transcript::Id(pressed.creature)},
		                                 {"real_ms", realMs},
		                                 {"turns", turns},
		                                 {"held_ms", heldMs},
		                                 {"charge_ms", fight::ReleasedCharge(heldMs)},
		                                 {"blow_speed", fight::BlowSpeed(fight::ReleasedCharge(heldMs))}};
	});
	// The held time is whole milliseconds; the charge goes to the blow at the next turn
	Send(player_commands::FightCharge {.heldMs = static_cast<uint32_t>(heldMs)});
}

void CreatureFightSystem::Send(const player_commands::Action& action)
{
	if (Locator::playerCommandSystem::has_value())
	{
		Locator::playerCommandSystem::value().Submit(action);
		return;
	}
	// With no queue of commands (the tests), the world does the command at once
	std::visit(
	    [this](const auto& command) {
		    using Kind = std::decay_t<decltype(command)>;
		    if constexpr (std::is_same_v<Kind, player_commands::FightGround>)
		    {
			    DoGroundPress(OwnPlayer(), command.creature, command.point, command.replace);
		    }
		    else if constexpr (std::is_same_v<Kind, player_commands::FightMove>)
		    {
			    DoMove(OwnPlayer(), command.target,
			           {.kind = static_cast<fight::Move::Kind>(command.kind), .value = command.value}, command.replace);
		    }
		    else if constexpr (std::is_same_v<Kind, player_commands::FightCharge>)
		    {
			    DoCharge(OwnPlayer(), static_cast<float>(command.heldMs));
		    }
	    },
	    action);
}

void CreatureFightSystem::DoGroundPress(PlayerNames player, entt::entity creature, const glm::vec3& point, bool replace)
{
	auto& registry = Locator::entitiesRegistry::value();
	// Only the sending player's creature in its fight takes a step
	if (!registry.Valid(creature) || PrimaryCreatureOf(registry, player) != creature ||
	    !registry.AllOf<CreatureFighting>(creature))
	{
		return;
	}
	const auto& selfAt = registry.Get<const Transform>(creature).position;
	const auto heading = registry.Get<const CreatureLocomotion>(creature).heading;
	const auto step = fight::StepTowards(WorldToLocal(Flat(point) - Flat(selfAt), heading));
	_queueSource = "hand";
	QueueMove(creature, fight::StepMove(step), replace);
	_queueSource = "script";
}

void CreatureFightSystem::DoMove(PlayerNames player, entt::entity target, const fight::Move& move, bool replace)
{
	auto& registry = Locator::entitiesRegistry::value();
	// The thing pressed on must still be a creature; the move goes to the sending player's creature
	if (!registry.Valid(target) || !registry.AllOf<Creature>(target))
	{
		return;
	}
	const auto self = PrimaryCreatureOf(registry, player);
	if (!self.has_value())
	{
		return;
	}
	// The hand and the gestures of a real player give only these moves. A miracle must be a fight miracle that the
	// creature knows.
	if (!fight::HandGives(move, KnownFightMagic(registry, *self)))
	{
		if (const auto logger = spdlog::get("game"); logger != nullptr)
		{
			SPDLOG_LOGGER_WARN(
			    logger, "The turn refuses the fight move of kind {} and value {} of player {}: the hand does not give it",
			    static_cast<int>(move.kind), move.value, static_cast<int>(player));
		}
		return;
	}
	const bool gesture = move.kind == fight::Move::Kind::Special || move.kind == fight::Move::Kind::Spell;
	_queueSource = gesture ? "gesture" : "hand";
	QueueMove(*self, move, replace);
	_queueSource = "script";
}

void CreatureFightSystem::DoCharge(PlayerNames player, float heldMs)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (const auto self = PrimaryCreatureOf(registry, player); self.has_value())
	{
		ReleaseCharge(*self, heldMs);
	}
}

void CreatureFightSystem::DoFightCheat(PlayerNames player)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto self = PrimaryCreatureOf(registry, player);
	if (!self.has_value())
	{
		return;
	}
	if (Locator::leashSystem::has_value() && Locator::leashSystem::value().IsLeashed(*self))
	{
		Locator::leashSystem::value().TakeOff(*self);
	}
	// A creature in a duel gives up its action: its fight is called off, with no winner
	if (IsDuelling(*self))
	{
		CallOff(*self);
		return;
	}
	// Any other creature fights the nearest creature by the distance on the map. The first creature found at the least
	// distance wins.
	const auto at = registry.Get<const Transform>(*self).position;
	std::optional<entt::entity> nearest;
	float least = std::numeric_limits<float>::max();
	registry.Each<const Creature, const Transform>(
	    [&nearest, &least, at, own = *self](entt::entity entity, const Creature& /*creature*/, const Transform& transform) {
		    if (entity == own)
		    {
			    return;
		    }
		    const auto distance = gutils::GetDistanceInMetres(at, transform.position);
		    if (distance < least)
		    {
			    least = distance;
			    nearest = entity;
		    }
	    });
	if (nearest.has_value())
	{
		const auto result = StartFight(*self, *nearest);
		fight_transcript::Note(*self, "fight_cheat", [&] {
			return fight_transcript::Fields {{"creature", fight_transcript::Id(*self)},
			                                 {"opponent", fight_transcript::Id(*nearest)},
			                                 {"player", static_cast<int32_t>(player)},
			                                 {"started", result == StartResult::Started}};
		});
	}
}

std::optional<fight::Tip> CreatureFightSystem::HandTip(std::optional<entt::entity> under) const
{
	const auto self = PlayersFighter();
	if (!self.has_value() || !DirectsFight())
	{
		return std::nullopt;
	}
	const auto opponent = Locator::entitiesRegistry::value().Get<const CreatureFighting>(*self).opponent;
	return fight::TipOver(under == self, under == opponent);
}

std::optional<posed_model::SkinHit> CreatureFightSystem::SeesOpponent(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto opponent = registry.Get<const CreatureFighting>(creature).opponent;
	// The line from the fighter's groin through the opponent's must meet the opponent's skin
	const auto from = creature_scars::GroinOf(creature);
	const auto to = creature_scars::GroinOf(opponent);
	if (!from.has_value() || !to.has_value())
	{
		return std::nullopt;
	}
	return SkinHitOn(registry, opponent, *from, *to - *from);
}

bool CreatureFightSystem::GestureMove(const fight::Move& move)
{
	const auto self = PlayersFighter();
	auto& registry = Locator::entitiesRegistry::value();
	if (!self.has_value() || registry.Get<const CreatureFighting>(*self).marks.Full())
	{
		return false;
	}
	const auto seen = SeesOpponent(*self);
	if (!seen.has_value())
	{
		return false;
	}
	// Its sparkle sits where the line to the opponent meets its skin; a gesture charges nothing. The move goes into the
	// queue at the next turn.
	auto& fighting = registry.Get<CreatureFighting>(*self);
	fighting.marks.Add(MarkOnSkin(*seen, marks::Anchor::Opponent));
	_markHeld = false;
	Send(player_commands::FightMove {
	    .target = fighting.opponent, .kind = static_cast<uint8_t>(move.kind), .value = move.value, .replace = false});
	return true;
}

bool CreatureFightSystem::GestureSpecialMove()
{
	return GestureMove({.kind = fight::Move::Kind::Special});
}

bool CreatureFightSystem::GestureSpell(MagicType type)
{
	return GestureMove({.kind = fight::Move::Kind::Spell, .value = static_cast<uint32_t>(type)});
}
