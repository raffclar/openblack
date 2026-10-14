/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FightAutopilot.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <format>
#include <variant>

#include <entt/entity/entity.hpp>
#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureLocomotion.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/FightTranscriptSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::inspector;
using namespace openblack::fight_autopilot;
namespace fight = openblack::creature_fight;
using ecs::components::Creature;
using ecs::components::CreatureFighting;
using ecs::components::CreatureLocomotion;
using ecs::components::Transform;

namespace
{
/// The Action button is the right button of the mouse; a double click is made with the left
constexpr uint8_t k_ActionButton = 3;
constexpr uint8_t k_LeftButton = 1;
/// The hand presses once it has pointed at the right thing for this many frames, and the aim moved less than this many
/// pixels in the last frame
constexpr uint32_t k_AimFrames = 2;
constexpr int32_t k_SteadyPixels = 4;
/// The hand gives up an aim after this many frames, and tries a double click into the arena again after this long
constexpr uint32_t k_AimGiveUpFrames = 90;
constexpr float k_ViewRetryMs = 3000.0f;
/// The lesson is checked this long after the bout starts
constexpr double k_LessonCheckDelayMs = 1000.0;
/// The keeps of the log
constexpr size_t k_MaxLog = 4000;
/// The heights on the body that the hand aims at, as shares of the height, for each band. The middle of the band comes
/// first; the others are for a body with a gap there, such as between the legs.
constexpr std::array<float, 4> k_HighShares {0.83f, 0.74f, 0.70f, 0.92f};
constexpr std::array<float, 4> k_MiddleShares {0.50f, 0.42f, 0.58f, 0.36f};
constexpr std::array<float, 4> k_LowShares {0.17f, 0.25f, 0.30f, 0.08f};
static_assert(k_HighShares.back() > fight::k_HighAbove && k_HighShares.at(2) > fight::k_HighAbove);
static_assert(k_LowShares.at(2) < fight::k_LowBelow);
/// A step is pressed on the ground this far from the creature, as shares of the arena's radius, and this far to the
/// side, as shares of that distance, so that the direction stays the same
constexpr std::array<float, 3> k_StepDistances {0.35f, 0.55f, 0.75f};
constexpr std::array<float, 3> k_StepSides {0.0f, 0.5f, -0.5f};
/// A double click is made at the middle of the arena, or this far from it as a share of the radius
constexpr float k_ViewOffset = 0.3f;

ecs::Registry& Registry()
{
	return Locator::entitiesRegistry::value();
}

std::optional<entt::entity> PlayersCreature()
{
	if (!Locator::playerSystem::has_value())
	{
		return std::nullopt;
	}
	auto& players = Locator::playerSystem::value();
	return players.GetPrimaryCreature(players.GetLocalPlayer());
}

float HeightOf(const Creature& creature)
{
	return creature_feedback::k_HeightAtSizeOne * ecs::components::ShownSize(creature);
}

std::optional<entt::entity> Picked()
{
	if (!Locator::pickingSystem::has_value())
	{
		return std::nullopt;
	}
	return Locator::pickingSystem::value().GetPick().object;
}

bool PickedCreature()
{
	const auto picked = Picked();
	return picked.has_value() && Registry().Valid(*picked) && Registry().AllOf<Creature>(*picked);
}

const fight_transcript::Field* FieldOf(const fight_transcript::Entry& entry, std::string_view name)
{
	for (const auto& field : entry.fields)
	{
		if (field.name == name)
		{
			return &field;
		}
	}
	return nullptr;
}

std::string TextOf(const fight_transcript::Entry& entry, std::string_view name)
{
	const auto* field = FieldOf(entry, name);
	if (field == nullptr)
	{
		return {};
	}
	return std::visit(
	    [](const auto& value) -> std::string {
		    using T = std::decay_t<decltype(value)>;
		    if constexpr (std::is_same_v<T, std::string>)
		    {
			    return value;
		    }
		    else
		    {
			    return std::format("{}", value);
		    }
	    },
	    field->value);
}

bool IsOwn(const fight_transcript::Entry& entry, std::string_view name, entt::entity own)
{
	const auto* field = FieldOf(entry, name);
	const auto* number = field != nullptr ? std::get_if<int64_t>(&field->value) : nullptr;
	return number != nullptr && *number == static_cast<int64_t>(entt::to_integral(own));
}
} // namespace

FightAutopilot::FightAutopilot(InputTargetInterface& input)
    : _input(input)
{
}

std::string_view FightAutopilot::Name(Phase phase)
{
	static constexpr std::array<std::string_view, 5> k_Names {"waiting", "showing_fight", "aiming", "holding", "cooling"};
	return k_Names.at(static_cast<size_t>(phase));
}

std::string FightAutopilot::Start(Settings settings)
{
	if (settings.policy == Policy::Sequence && settings.sequence.empty())
	{
		return "the sequence policy needs a sequence of moves";
	}
	if (_active)
	{
		Stop("restarted");
	}
	_settings = std::move(settings);
	_stage = _settings.stage.value_or(LessonStage::Hit);
	_random.seed(_settings.seed);
	_phase = Phase::Waiting;
	_elapsedMs = 0.0;
	_phaseMs = 0.0f;
	_waitMs = 0.0f;
	_phaseFrames = 0;
	_nextSequenceMove = 0;
	_clickStep = 0;
	_viewRetryMs = 0.0f;
	_fight.reset();
	_readTo = 0;
	_checkedLesson = false;
	_lessonCheckDueMs.reset();
	_counts = {};
	_active = true;
	Log("start",
	    std::format("policy {}, seed {}, stage {}, {} sequence moves, enter view {}", fight_autopilot::Name(_settings.policy),
	                _settings.seed, fight_autopilot::Name(_stage), _settings.sequence.size(), _settings.enterView));
	return {};
}

void FightAutopilot::Stop(std::string_view why)
{
	if (!_active)
	{
		return;
	}
	// A button still down is let go, and the mouse goes back to the player
	if (_phase == Phase::Holding)
	{
		static_cast<void>(Give({.kind = InputEvent::Kind::ButtonUp, .button = k_ActionButton}));
	}
	static_cast<void>(Give({.kind = InputEvent::Kind::Release}));
	_active = false;
	_phase = Phase::Waiting;
	Log("stop", std::string(why));
}

Status FightAutopilot::GetStatus() const
{
	auto status = _counts;
	status.active = _active;
	status.policy = _settings.policy;
	status.stage = _stage;
	status.phase = std::string(Name(_phase));
	status.fight = _fight;
	return status;
}

void FightAutopilot::Log(std::string_view event, std::string detail, std::optional<glm::ivec2> pointer)
{
	if (_log.size() >= k_MaxLog)
	{
		_log.erase(_log.begin(), _log.begin() + static_cast<std::ptrdiff_t>(k_MaxLog / 4));
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Fight autopilot: {} {}", event, detail);
	_log.push_back({
	    .frame = _frame,
	    .turn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0u,
	    .elapsedMs = _elapsedMs,
	    .event = std::string(event),
	    .detail = std::move(detail),
	    .pointer = pointer.has_value() ? std::optional(std::pair(pointer->x, pointer->y)) : std::nullopt,
	});
}

bool FightAutopilot::Give(const InputEvent& event)
{
	const auto error = _input.Apply(event);
	if (!error.empty())
	{
		Log("input_failed", std::format("{}: {}", KindName(event.kind), error));
		return false;
	}
	return true;
}

void FightAutopilot::Frame(uint64_t frame)
{
	_frame = frame;
	if (!_active || !Locator::creatureFightSystem::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	// The autopilot runs at the game's speed: nothing happens while the game is paused
	if (!Locator::time::has_value() || Locator::time::value().IsPaused())
	{
		return;
	}
	const auto gameMs = static_cast<float>(Locator::time::value().GetFrameGameTime().count());
	_elapsedMs += gameMs;
	_phaseMs += gameMs;
	++_phaseFrames;

	auto& fights = Locator::creatureFightSystem::value();
	auto& registry = Registry();
	const auto own = PlayersCreature();
	if (!own.has_value() || !registry.Valid(*own))
	{
		return;
	}
	ReadResults(*own);
	const auto* fighting = registry.TryGet<const CreatureFighting>(*own);
	if (fighting == nullptr)
	{
		// The fight is over: the lesson is checked once from its transcript, and the hand waits for the next fight
		if (_phase == Phase::Holding)
		{
			static_cast<void>(Give({.kind = InputEvent::Kind::ButtonUp, .button = k_ActionButton}));
		}
		if (_fight.has_value() && !_checkedLesson)
		{
			CheckLesson(*own);
		}
		_phase = Phase::Waiting;
		return;
	}
	if (Locator::fightTranscript::has_value())
	{
		const auto fight = Locator::fightTranscript::value().GetRecorder().FightOf(entt::to_integral(*own));
		if (fight != _fight)
		{
			_fight = fight;
			_readTo = 0;
			_checkedLesson = false;
		}
	}
	const auto opponent = fighting->opponent;

	// The lesson moves on as each of its goals shows
	if (_settings.policy == Policy::Lesson)
	{
		const Observation seen {
		    .own = fights.CurrentFightAction(*own),
		    .opponent = fights.CurrentFightAction(opponent),
		    .queuedBlows = fights.QueuedBlows(*own),
		    .queued = static_cast<uint32_t>(fighting->fighter.queue.Size()),
		};
		// The stage that the script waits for comes first; without the script's reads, what the autopilot sees moves it on
		std::optional<LessonStage> scripted;
		if (_fight.has_value() && Locator::fightTranscript::has_value())
		{
			if (const auto* fight = Locator::fightTranscript::value().GetRecorder().Find(*_fight); fight != nullptr)
			{
				scripted = ScriptStage(*fight, entt::to_integral(*own));
			}
		}
		const auto next = scripted.has_value() ? std::max(*scripted, _stage) : Advance(_stage, seen);
		if (next != _stage)
		{
			Log("stage", std::format("{} -> {}", fight_autopilot::Name(_stage), fight_autopilot::Name(next)));
			_stage = next;
			// The taught stages are all done when the bout starts. The check waits a little, so that the transcript has the
			// records of the last stage.
			if (_stage == LessonStage::Bout)
			{
				_lessonCheckDueMs = _elapsedMs + k_LessonCheckDelayMs;
			}
		}
		if (_lessonCheckDueMs.has_value() && _elapsedMs >= *_lessonCheckDueMs && _fight.has_value() && !_checkedLesson)
		{
			_lessonCheckDueMs.reset();
			CheckLesson(*own);
		}
	}

	const auto enter = [this](Phase phase) {
		_phase = phase;
		_phaseMs = 0.0f;
		_phaseFrames = 0;
	};
	switch (_phase)
	{
	case Phase::Waiting:
	{
		if (fighting->stage != CreatureFighting::Stage::Duel)
		{
			return;
		}
		if (!fights.IsCameraOnFight())
		{
			_viewRetryMs -= gameMs;
			if (_settings.enterView && _viewRetryMs <= 0.0f)
			{
				_clickStep = 0;
				_candidate = 0;
				enter(Phase::ShowingFight);
			}
			return;
		}
		if (!fights.DirectsFight())
		{
			return;
		}
		if (_settings.policy == Policy::Sequence)
		{
			const auto due = DueMove(_settings.sequence, _nextSequenceMove, static_cast<float>(_elapsedMs));
			if (!due.has_value())
			{
				return;
			}
			_intent = _settings.sequence.at(*due).intent;
			_nextSequenceMove = *due + 1;
			_waitMs = 0.0f;
		}
		else
		{
			const Observation seen {.own = fights.CurrentFightAction(*own),
			                        .opponent = fights.CurrentFightAction(opponent),
			                        .queuedBlows = fights.QueuedBlows(*own),
			                        .queued = static_cast<uint32_t>(fighting->fighter.queue.Size())};
			const auto choice = Choose(_settings.policy, _stage, seen, _random);
			if (choice.intent.kind == Intent::Kind::None)
			{
				return;
			}
			_intent = choice.intent;
			_waitMs = choice.waitMs;
		}
		_candidate = 0;
		_lastAim.reset();
		Log("intent", Describe(_intent));
		enter(Phase::Aiming);
		break;
	}
	case Phase::ShowingFight:
		ShowFight(*own);
		break;
	case Phase::Aiming:
	{
		if (!fights.DirectsFight())
		{
			Log("aim", "the hand no longer directs the fight");
			enter(Phase::Waiting);
			break;
		}
		const auto aim = AimPoint(*own, opponent);
		if (!aim.has_value())
		{
			Log("aim", "the target is off the screen");
			enter(Phase::Cooling);
			break;
		}
		const bool steady = _lastAim.has_value() && std::abs(aim->x - _lastAim->x) <= k_SteadyPixels &&
		                    std::abs(aim->y - _lastAim->y) <= k_SteadyPixels;
		static_cast<void>(Give({.kind = InputEvent::Kind::PointerTo, .position = *aim}));
		_lastAim = aim;
		if (_phaseFrames >= k_AimFrames && steady && PickMatches(*own, opponent))
		{
			if (Give({.kind = InputEvent::Kind::ButtonDown, .position = *aim, .button = k_ActionButton}))
			{
				++_counts.presses;
				Log("press", Describe(_intent), aim);
				enter(Phase::Holding);
			}
			break;
		}
		// The hand tries the next place when it does not point at the target: another height on the body, or another
		// place on the ground when a creature is in the way
		if (_phaseFrames % (k_AimFrames + 1) == 0)
		{
			++_candidate;
		}
		if (_phaseFrames > k_AimGiveUpFrames)
		{
			const auto picked = Picked();
			Log("aim",
			    std::format("gave up: the hand did not point at the target; it points at {}",
			                picked.has_value() ? std::format("entity {}", entt::to_integral(*picked)) : std::string("nothing")),
			    aim);
			enter(Phase::Cooling);
		}
		break;
	}
	case Phase::Holding:
		if (_phaseMs >= _intent.holdMs)
		{
			static_cast<void>(Give({.kind = InputEvent::Kind::ButtonUp, .button = k_ActionButton}));
			Log("release", std::format("after {:.0f} ms", _phaseMs));
			enter(Phase::Cooling);
		}
		break;
	case Phase::Cooling:
		if (_phaseMs >= _waitMs)
		{
			enter(Phase::Waiting);
		}
		break;
	}
}

void FightAutopilot::ShowFight(entt::entity own)
{
	auto& registry = Registry();
	const auto& fighting = registry.Get<const CreatureFighting>(own);
	const auto& arena = fighting.arena;
	const std::array<glm::vec2, 5> offsets {glm::vec2(0.0f, 0.0f), glm::vec2(k_ViewOffset, 0.0f),
	                                        glm::vec2(-k_ViewOffset, 0.0f), glm::vec2(0.0f, k_ViewOffset),
	                                        glm::vec2(0.0f, -k_ViewOffset)};
	const auto place = arena.centre + (offsets.at(_candidate % offsets.size()) * arena.radius);
	const auto screen = _input.WorldToScreen({place.x, _input.GroundHeight(place), place.y});
	const auto failed = [this](std::string_view why) {
		Log("view", std::string(why));
		_viewRetryMs = k_ViewRetryMs;
		_phase = Phase::Waiting;
		_phaseMs = 0.0f;
		_phaseFrames = 0;
	};
	if (!screen.has_value())
	{
		failed("the arena is off the screen");
		return;
	}
	switch (_clickStep)
	{
	case 0:
		static_cast<void>(Give({.kind = InputEvent::Kind::PointerTo, .position = *screen}));
		_clickStep = 1;
		break;
	case 1:
		// A double click on a creature locks the camera on it, so the click needs land under the hand
		static_cast<void>(Give({.kind = InputEvent::Kind::PointerTo, .position = *screen}));
		if (PickedCreature())
		{
			++_candidate;
			if (_candidate >= offsets.size())
			{
				failed("creatures cover every place in the arena");
			}
			_clickStep = 0;
			break;
		}
		static_cast<void>(Give({.kind = InputEvent::Kind::ButtonDown, .position = *screen, .button = k_LeftButton}));
		Log("view", "double click: first press", screen);
		_clickStep = 2;
		break;
	case 2:
		static_cast<void>(Give({.kind = InputEvent::Kind::ButtonUp, .position = *screen, .button = k_LeftButton}));
		_clickStep = 3;
		break;
	case 3:
		static_cast<void>(
		    Give({.kind = InputEvent::Kind::ButtonDown, .position = *screen, .button = k_LeftButton, .clicks = 2}));
		Log("view", "double click: second press", screen);
		_clickStep = 4;
		break;
	case 4:
		static_cast<void>(Give({.kind = InputEvent::Kind::ButtonUp, .position = *screen, .button = k_LeftButton, .clicks = 2}));
		_clickStep = 5;
		break;
	default:
		if (Locator::creatureFightSystem::value().IsCameraOnFight())
		{
			Log("view", "the camera shows the fight");
			_phase = Phase::Waiting;
			_phaseMs = 0.0f;
			_phaseFrames = 0;
		}
		else if (_phaseMs > k_ViewRetryMs)
		{
			failed("the double click did not show the fight");
		}
		break;
	}
}

std::optional<glm::ivec2> FightAutopilot::AimPoint(entt::entity own, entt::entity opponent) const
{
	auto& registry = Registry();
	switch (_intent.kind)
	{
	case Intent::Kind::Hit:
	case Intent::Kind::Block:
	{
		const auto target = _intent.kind == Intent::Kind::Hit ? opponent : own;
		if (!registry.Valid(target))
		{
			return std::nullopt;
		}
		const auto& feet = registry.Get<const Transform>(target).position;
		const auto& shares = _intent.kind == Intent::Kind::Block ? k_MiddleShares
		                     : _intent.band == fight::Band::High ? k_HighShares
		                     : _intent.band == fight::Band::Low  ? k_LowShares
		                                                         : k_MiddleShares;
		const auto share = shares.at(_candidate % shares.size());
		return _input.WorldToScreen(feet + glm::vec3(0.0f, HeightOf(registry.Get<const Creature>(target)) * share, 0.0f));
	}
	case Intent::Kind::Step:
	{
		const auto& fighting = registry.Get<const CreatureFighting>(own);
		const auto& at = registry.Get<const Transform>(own).position;
		const auto ahead = creature_locomotion::DirectionOf(registry.Get<const CreatureLocomotion>(own).heading);
		const glm::vec2 right {-ahead.y, ahead.x};
		glm::vec2 way = ahead;
		glm::vec2 side = right;
		switch (_intent.step)
		{
		case fight::Step::Forward:
			break;
		case fight::Step::Back:
			way = -ahead;
			break;
		case fight::Step::Right:
			way = right;
			side = ahead;
			break;
		case fight::Step::Left:
			way = -right;
			side = ahead;
			break;
		}
		// The places to try, near ones first, each in the arena
		const auto count = k_StepDistances.size() * k_StepSides.size();
		for (size_t tried = 0; tried < count; ++tried)
		{
			const auto index = (_candidate + tried) % count;
			const auto distance = k_StepDistances.at(index / k_StepSides.size()) * fighting.arena.radius;
			const auto offset = k_StepSides.at(index % k_StepSides.size()) * distance;
			const auto place = glm::vec2(at.x, at.z) + (way * distance) + (side * offset);
			if (!fight::InArena(fighting.arena, place))
			{
				continue;
			}
			return _input.WorldToScreen({place.x, _input.GroundHeight(place), place.y});
		}
		return std::nullopt;
	}
	case Intent::Kind::None:
		break;
	}
	return std::nullopt;
}

bool FightAutopilot::PickMatches(entt::entity own, entt::entity opponent) const
{
	switch (_intent.kind)
	{
	case Intent::Kind::Hit:
		return Picked() == opponent;
	case Intent::Kind::Block:
		return Picked() == own;
	case Intent::Kind::Step:
		return !PickedCreature();
	case Intent::Kind::None:
		break;
	}
	return false;
}

void FightAutopilot::ReadResults(entt::entity own)
{
	if (!_fight.has_value() || !Locator::fightTranscript::has_value())
	{
		return;
	}
	const auto* fight = Locator::fightTranscript::value().GetRecorder().Find(*_fight);
	if (fight == nullptr)
	{
		return;
	}
	for (; _readTo < fight->entries.size(); ++_readTo)
	{
		const auto& entry = fight->entries.at(_readTo);
		if (entry.kind == "press" && IsOwn(entry, "creature", own))
		{
			const auto result = TextOf(entry, "result");
			if (result == "accepted")
			{
				++_counts.accepted;
			}
			Log("result", std::format("press {} (record {}): {} {}", result, entry.index, TextOf(entry, "target"),
			                          TextOf(entry, "part")));
		}
		else if (entry.kind == "blow" && IsOwn(entry, "attacker", own))
		{
			const auto outcome = TextOf(entry, "outcome");
			_counts.landed += outcome == "hit" ? 1 : 0;
			_counts.blocked += outcome == "blocked" ? 1 : 0;
			_counts.missed += outcome == "miss" ? 1 : 0;
			Log("result", std::format("blow {} (record {}): part {}, damage {}", outcome, entry.index, TextOf(entry, "part"),
			                          TextOf(entry, "damage")));
		}
		else if (entry.kind == "fight_over")
		{
			Log("result", std::format("fight over (record {}): {}", entry.index, TextOf(entry, "reason")));
		}
	}
}

void FightAutopilot::CheckLesson(entt::entity own)
{
	_checkedLesson = true;
	ReadResults(own);
	if (_settings.policy != Policy::Lesson || !Locator::fightTranscript::has_value())
	{
		return;
	}
	const auto* fight = Locator::fightTranscript::value().GetRecorder().Find(*_fight);
	if (fight == nullptr)
	{
		return;
	}
	const auto check = fight_autopilot::CheckLesson(*fight, entt::to_integral(own));
	_counts.lesson = check;
	const auto at = [](const std::optional<uint64_t>& index) {
		return index.has_value() ? std::format("{}", *index) : std::string("none");
	};
	Log("lesson_check",
	    std::format("{}: hit at record {}, hits at {}, block at {}, step at {}", check.InOrder() ? "passed" : "failed",
	                at(check.hit), at(check.hits), at(check.block), at(check.step)));
}
