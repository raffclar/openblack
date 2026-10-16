/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ForceFeedbackSystem.h"

#include <algorithm>
#include <utility>

namespace openblack::ecs::systems
{

namespace
{

/// The number of starts and stops that the system keeps for the inspector
constexpr size_t k_KeptEvents = 64;

} // namespace

void ForceFeedbackSystem::Connect(std::unique_ptr<force_feedback::Backend> backend, const ifr::IFRFile& file)
{
	Disconnect();
	if (backend == nullptr)
	{
		return;
	}
	// The effects that the file does not have stay empty, and the game's calls for them do nothing
	for (size_t i = 0; i < force_feedback::k_Effects.size(); ++i)
	{
		if (const auto* object = file.Find(force_feedback::k_Effects.at(i).name); object != nullptr)
		{
			_slots.at(i) = Slot {.effect = force_feedback::EffectFrom(file, *object), .startedAt = {}, .iterations = {}};
		}
	}
	_backend = std::move(backend);
}

void ForceFeedbackSystem::Disconnect()
{
	if (_backend != nullptr)
	{
		_backend->Vibrate(0.0f);
	}
	_backend.reset();
	_slots.fill(std::nullopt);
	_handTexture = {};
	_strength = 0.0f;
}

bool ForceFeedbackSystem::DeviceExists() const
{
	return _backend != nullptr;
}

std::string_view ForceFeedbackSystem::DeviceName() const
{
	return _backend != nullptr ? _backend->Name() : std::string_view {};
}

int ForceFeedbackSystem::Level() const
{
	return _level;
}

void ForceFeedbackSystem::SetLevel(int level)
{
	StopAll();
	_level = level;
}

ForceFeedbackSystem::Slot* ForceFeedbackSystem::SlotFor(ImmersionEffectType type)
{
	return const_cast<Slot*>(std::as_const(*this).SlotFor(type));
}

const ForceFeedbackSystem::Slot* ForceFeedbackSystem::SlotFor(ImmersionEffectType type) const
{
	if (_backend == nullptr || !force_feedback::LevelAllows(type, _level))
	{
		return nullptr;
	}
	const auto& slot = _slots.at(static_cast<size_t>(type));
	return slot.has_value() ? &*slot : nullptr;
}

void ForceFeedbackSystem::Record(ImmersionEffectType type, bool start, std::optional<uint32_t> iterations)
{
	_events.push_back({.seconds = _clock, .type = type, .start = start, .iterations = iterations});
	while (_events.size() > k_KeptEvents)
	{
		_events.pop_front();
	}
	const auto name = force_feedback::EntryFor(type)->name;
	if (start)
	{
		_backend->Started(type, name);
	}
	else
	{
		_backend->Stopped(type, name);
	}
}

void ForceFeedbackSystem::Begin(ImmersionEffectType type, std::optional<uint32_t> iterations)
{
	auto* slot = SlotFor(type);
	if (slot == nullptr)
	{
		return;
	}
	// A start of an effect that plays starts it again from its beginning
	slot->startedAt = _clock;
	slot->iterations = iterations;
	Record(type, true, iterations);
}

void ForceFeedbackSystem::Start(ImmersionEffectType type)
{
	Begin(type, std::nullopt);
}

void ForceFeedbackSystem::StartTimes(ImmersionEffectType type, uint32_t iterations)
{
	Begin(type, iterations);
}

void ForceFeedbackSystem::Stop(ImmersionEffectType type)
{
	auto* slot = SlotFor(type);
	if (slot == nullptr)
	{
		return;
	}
	slot->startedAt.reset();
	Record(type, false, std::nullopt);
}

void ForceFeedbackSystem::StopAll()
{
	if (_backend == nullptr)
	{
		return;
	}
	// All the effects stop, whatever the detail level
	for (size_t i = 0; i < _slots.size(); ++i)
	{
		auto& slot = _slots.at(i);
		if (slot.has_value() && slot->startedAt.has_value())
		{
			slot->startedAt.reset();
			Record(force_feedback::k_Effects.at(i).type, false, std::nullopt);
		}
	}
}

bool ForceFeedbackSystem::IsPlaying(ImmersionEffectType type) const
{
	const auto* slot = SlotFor(type);
	return slot != nullptr && slot->startedAt.has_value() &&
	       force_feedback::Plays(slot->effect, _clock - *slot->startedAt, slot->iterations);
}

void ForceFeedbackSystem::QuickenBeat(ImmersionEffectType type, float fraction)
{
	if (_backend == nullptr || force_feedback::EntryFor(type) == nullptr)
	{
		return;
	}
	if (auto& slot = _slots.at(static_cast<size_t>(type)); slot.has_value())
	{
		// The beat changes from the period that the file gives, not from the last quicker beat
		force_feedback::QuickenBeat(slot->effect, fraction);
	}
}

void ForceFeedbackSystem::SetPeriodAndMagnitude(ImmersionEffectType type, uint32_t periodMs, uint32_t magnitude)
{
	if (_backend == nullptr || force_feedback::EntryFor(type) == nullptr)
	{
		return;
	}
	if (auto& slot = _slots.at(static_cast<size_t>(type)); slot.has_value())
	{
		force_feedback::SetPeriodAndMagnitude(slot->effect, periodMs, magnitude);
	}
}

void ForceFeedbackSystem::SetHandTexture(std::optional<ImmersionEffectType> type)
{
	if (_backend == nullptr)
	{
		return;
	}
	const auto step =
	    force_feedback::NextHandTexture(_handTexture, type, [this](ImmersionEffectType effect) { return IsPlaying(effect); });
	if (step.stop.has_value())
	{
		Stop(*step.stop);
	}
	if (step.start.has_value())
	{
		Start(*step.start);
	}
}

void ForceFeedbackSystem::TouchCreature(bool touchesBody)
{
	const auto step = force_feedback::FurStep(touchesBody, IsPlaying(ImmersionEffectType::Fur));
	if (step == true)
	{
		Start(ImmersionEffectType::Fur);
	}
	else if (step == false)
	{
		Stop(ImmersionEffectType::Fur);
	}
}

void ForceFeedbackSystem::Update(float seconds, glm::vec2 cursorShare)
{
	if (_backend == nullptr)
	{
		return;
	}
	_clock += seconds;
	const auto cursor = cursorShare * force_feedback::k_ReferenceScreen;
	const auto from = _lastCursor.value_or(cursor);
	_lastCursor = cursor;

	float strength = 0.0f;
	for (auto& slot : _slots)
	{
		if (!slot.has_value() || !slot->startedAt.has_value())
		{
			continue;
		}
		const auto elapsed = _clock - *slot->startedAt;
		if (!force_feedback::Plays(slot->effect, elapsed, slot->iterations))
		{
			slot->startedAt.reset();
			continue;
		}
		for (const auto& part : slot->effect.parts)
		{
			if (const auto* periodic = std::get_if<ifr::Periodic>(&part))
			{
				strength += force_feedback::PeriodicStrength(*periodic, elapsed, slot->iterations);
			}
			else if (const auto* texture = std::get_if<ifr::Texture>(&part))
			{
				strength += force_feedback::TextureStrength(*texture, from, cursor);
			}
		}
	}
	_strength = std::min(strength, 1.0f);
	_backend->Vibrate(_strength);
}

std::vector<ForceFeedbackEvent> ForceFeedbackSystem::Events() const
{
	return {_events.begin(), _events.end()};
}

float ForceFeedbackSystem::Strength() const
{
	return _strength;
}

} // namespace openblack::ecs::systems
