/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "HandGrain.h"

#include <cstdio>
#include <cstdlib>

#include <algorithm>

#include <spdlog/spdlog.h>

#include "Debug/DebugEnv.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandMagicStateInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "GameClock.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{
struct HandGrainState
{
	bool active {false};
	bool loop {false};
	float t {0.0f};
	float totalTime {1.0f};
	float heightToRaise {0.0f};
	float angleToRaise {0.0f};
	bool clampHand {false};
	glm::vec3 start {0.0f};
	float tilt {0.0f};
	float height {0.0f};
	float lastTilt {0.0f};
	float lastHeight {0.0f};
	bool holdingSeed {false};
};

/// The grain sprinkle's hand raise (Locator::handMagicState)
HandGrainState& Grain()
{
	if (!Locator::handMagicState::has_value())
	{
		std::fputs("hand_grain: no hand magic state in the locator (Locator::handMagicState)\n", stderr);
		std::abort();
	}
	return Locator::handMagicState::value().Get<HandGrainState>();
}

const hand_grain::Spline& KeyPoints()
{
	// 1e30 for both end slopes: a natural spline (with end slopes of 0 the raise would peak at 1.96 instead of 1.61)
	static const auto spline = hand_grain::BuildSpline(1e30f, 1e30f);
	return spline;
}

/// The fraction of the turn of the game clock
float TurnFraction()
{
	return game_clock::TurnFraction();
}

bool Trace()
{
	static const bool trace = debug_env::SpellTrace() || debug_env::HandTrace();
	return trace;
}
} // namespace

hand_grain::Spline hand_grain::BuildSpline(float yp1, float ypn)
{
	Spline spline;
	const auto& x = spline.x;
	const auto& y = spline.y;
	auto& y2 = spline.y2;
	constexpr size_t k_N = 4;
	std::array<float, k_N> u {};
	if (yp1 > 0.99e30f)
	{
		y2[0] = 0.0f;
		u[0] = 0.0f;
	}
	else
	{
		y2[0] = -0.5f;
		u[0] = (3.0f / (x[1] - x[0])) * ((y[1] - y[0]) / (x[1] - x[0]) - yp1);
	}
	for (size_t i = 1; i + 1 < k_N; ++i)
	{
		const float sig = (x[i] - x[i - 1]) / (x[i + 1] - x[i - 1]);
		const float p = sig * y2[i - 1] + 2.0f;
		y2[i] = (sig - 1.0f) / p;
		u[i] = (y[i + 1] - y[i]) / (x[i + 1] - x[i]) - (y[i] - y[i - 1]) / (x[i] - x[i - 1]);
		u[i] = (6.0f * u[i] / (x[i + 1] - x[i - 1]) - sig * u[i - 1]) / p;
	}
	float qn = 0.0f;
	float un = 0.0f;
	if (!(ypn > 0.99e30f))
	{
		qn = 0.5f;
		un = (3.0f / (x[k_N - 1] - x[k_N - 2])) * (ypn - (y[k_N - 1] - y[k_N - 2]) / (x[k_N - 1] - x[k_N - 2]));
	}
	y2[k_N - 1] = (un - qn * u[k_N - 2]) / (qn * y2[k_N - 2] + 1.0f);
	for (size_t k = k_N - 1; k-- > 0;)
	{
		y2[k] = y2[k] * y2[k + 1] + u[k];
	}
	return spline;
}

float hand_grain::Evaluate(const Spline& spline, float t)
{
	int lo = 0;
	int hi = static_cast<int>(spline.x.size()) - 1;
	while (hi - lo > 1)
	{
		const int k = (hi + lo) / 2;
		if (spline.x[static_cast<size_t>(k)] > t)
		{
			hi = k;
		}
		else
		{
			lo = k;
		}
	}
	const auto l = static_cast<size_t>(lo);
	const auto h = static_cast<size_t>(hi);
	const float step = spline.x[h] - spline.x[l];
	if (step == 0.0f)
	{
		return t;
	}
	const float a = (spline.x[h] - t) / step;
	const float b = (t - spline.x[l]) / step;
	return a * spline.y[l] + b * spline.y[h] +
	       ((a * a * a - a) * spline.y2[l] + (b * b * b - b) * spline.y2[h]) * (step * step) * (1.0f / 6.0f);
}

void hand_grain::Start(bool clampHand, float totalTime, float heightToRaise, float angleToRaise, bool loop)
{
	Grain().active = true;
	Grain().loop = loop;
	Grain().t = 0.0f;
	Grain().totalTime = totalTime;
	Grain().heightToRaise = heightToRaise;
	Grain().angleToRaise = angleToRaise;
	Grain().clampHand = clampHand;
	// the hand's point. (approximate) openblack's left hand transform stands in for it (the grip point while it holds
	// something)
	if (Locator::handSystem::has_value() && Locator::entitiesRegistry::has_value())
	{
		const auto hand = Locator::handSystem::value().GetPlayerHands()[0];
		auto& registry = Locator::entitiesRegistry::value();
		if (registry.Valid(hand))
		{
			Grain().start = registry.Get<const ecs::components::Transform>(hand).position;
		}
	}
}

void hand_grain::Stop()
{
	Grain().clampHand = false;
	Grain().height = 0.0f;
	Grain().tilt = 0.0f;
	Grain().active = false;
}

void hand_grain::GameTurnUpdate(float dt)
{
	Grain().lastHeight = Grain().height;
	Grain().lastTilt = Grain().tilt;
	// (pending) a creature it follows: creatures are not ported yet
	if (!Grain().active)
	{
		return;
	}
	Grain().t += dt / Grain().totalTime;
	if (Grain().t > 1.0f)
	{
		if (Grain().loop)
		{
			Grain().t = 0.0f;
		}
		else
		{
			Stop();
		}
	}
	if (!Grain().active)
	{
		return;
	}
	const float v = Evaluate(KeyPoints(), Grain().t);
	Grain().height = v * Grain().heightToRaise;
	Grain().tilt = v * Grain().angleToRaise;
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "Grain trace: t {:.2f} of {:.1f} s, raise {:.2f} m, tilt {:.3f} rad, clamped {}", Grain().t,
		                   Grain().totalTime, Grain().height, Grain().tilt, Grain().clampHand);
	}
}

float hand_grain::Height()
{
	return Grain().lastHeight + (Grain().height - Grain().lastHeight) * TurnFraction();
}

float hand_grain::Tilt()
{
	return Grain().lastTilt + (Grain().tilt - Grain().lastTilt) * TurnFraction();
}

std::optional<glm::vec3> hand_grain::ClampedPosition()
{
	return Grain().clampHand ? std::optional(Grain().start) : std::nullopt;
}

bool hand_grain::Active()
{
	return Grain().active;
}

glm::vec3 hand_grain::Debug()
{
	return {Grain().t, Grain().height, Grain().tilt};
}

void hand_grain::SetHoldingSeed(bool holding)
{
	if (holding == Grain().holdingSeed)
	{
		return;
	}
	Grain().holdingSeed = holding;
	if (holding)
	{
		// entering (after the holding state's enter): the grain state is cleared
		Grain() = HandGrainState {};
		Grain().holdingSeed = true;
	}
	else
	{
		// leaving: off, no creature followed
		Grain().active = false;
	}
}

void hand_grain::Reset()
{
	Grain() = HandGrainState {};
}
