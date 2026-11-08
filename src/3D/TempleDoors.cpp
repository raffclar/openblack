/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TempleDoors.h"

#include <cmath>

#include <algorithm>

#include <glm/gtc/constants.hpp>
#include <glm/vec4.hpp>

#include "Common/X87.h"
#include "Sound/SoundId.h"

using namespace openblack;

TempleDoors::TempleDoors(PlaySound playSound)
    : _playSound(std::move(playSound))
{
	_joints.fill(glm::mat4(1.0f));
}

std::optional<uint32_t> TempleDoors::LeafOf(TempleRoom room)
{
	// The main room's mesh numbers the leaves round from the doorway out, skipping the wall of the scrolls
	switch (room)
	{
	case TempleRoom::Options:
		return 3;
	case TempleRoom::CreatureCave:
		return 5;
	case TempleRoom::Challenge:
		return 7;
	case TempleRoom::SaveGame:
		return 9;
	case TempleRoom::Credits:
		return 11;
	case TempleRoom::Multi:
		return 13;
	case TempleRoom::Main:
	case TempleRoom::Unknown:
		break;
	}
	return std::nullopt;
}

void TempleDoors::Open(std::optional<uint32_t> leaf, float from, float rate)
{
	if (_swing > 1.0f || leaf != _leaf)
	{
		_swing = from;
		if (leaf.has_value())
		{
			_leaf = leaf;
		}
	}
	_rate = rate;
}

void TempleDoors::Close(float rate)
{
	if (!_leaf.has_value() || _swing > 1.0f)
	{
		return;
	}
	_playSound(static_cast<entt::id_type>(audio::SoundId::G_CitadelDoorClose_02));
	_swing = 2.0f - _swing;
	_rate = rate;
	if (_swing <= 1.0f)
	{
		_swing += 0.01f;
	}
}

void TempleDoors::FastClose()
{
	if (!_leaf.has_value())
	{
		return;
	}
	_playSound(static_cast<entt::id_type>(audio::SoundId::G_CitadelDoorClose_02));
	_swing = 2.0f;
	_rate = 2.0f;
}

void TempleDoors::Update(float deltaSeconds)
{
	if (!_leaf.has_value())
	{
		_swing = 0.0f;
	}
	else if (_swing < 1.0f)
	{
		const bool waiting = _swing <= 0.0f;
		_swing += _rate * deltaSeconds;
		if (waiting && _swing > 0.0f)
		{
			_playSound(static_cast<entt::id_type>(audio::SoundId::G_CitadelDoorOpen_01));
		}
		_swing = std::min(_swing, 1.0f);
	}
	else if (_swing > 1.0f)
	{
		_swing += _rate * deltaSeconds;
		if (_swing > 2.0f)
		{
			_swing = 0.0f;
			_leaf.reset();
		}
	}
	LayOutJoints();
}

void TempleDoors::LayOutJoints()
{
	// The leaves ease open and back shut. Each step rounds to a float, as the game's maths does.
	float turn = 0.0f;
	if (_swing > 0.0f && _swing < 2.0f)
	{
		const float angle = _swing * glm::pi<float>();
		turn = x87::Mul(x87::Sub(1.0, x87::Cos(angle)), k_HalfSwing);
	}
	_joints.fill(glm::mat4(1.0f));
	if (!_leaf.has_value())
	{
		return;
	}
	// The two leaves turn in opposite directions, so that the two open outwards. The first leaf turns by the negative
	// angle.
	const auto leafTurn = [](float angle) {
		const float c = x87::RoundToFloat(x87::Cos(angle));
		const float s = x87::RoundToFloat(x87::Sin(angle));
		glm::mat4 joint(1.0f);
		joint[0] = glm::vec4(c, 0.0f, s, 0.0f);
		joint[2] = glm::vec4(-s, 0.0f, c, 0.0f);
		return joint;
	};
	if (*_leaf < k_JointCount)
	{
		_joints.at(*_leaf) = leafTurn(-turn);
	}
	if (*_leaf + 1 < k_JointCount)
	{
		_joints.at(*_leaf + 1) = leafTurn(turn);
	}
}
