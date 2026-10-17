/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the rules of the wakes, rings and splashes on the experiment's water

#include "UnderwaterWake.h"

#include <cmath>

#include <algorithm>
#include <numbers>

#include <glm/common.hpp>
#include <glm/geometric.hpp>

namespace openblack::graphics::underwater::wake
{

namespace
{
float SmoothStep(float low, float high, float x)
{
	const float t = std::clamp((x - low) / (high - low), 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}
} // namespace

Write WriteFor(const Mover& mover)
{
	if (mover.submerged <= 0.0f || mover.radius <= 0.0f)
	{
		return {};
	}
	// The motion that counts: across the ground, and a part of the motion up and down as a thing bobs or wades
	const float motion = glm::length(mover.velocity) + 0.6f * std::abs(mover.verticalSpeed);
	const float moving = SmoothStep(k_StillSpeed, k_FullWakeSpeed, motion);
	// A thing pushes the water down by about three fifths of what it has in the water, and a wide thing by no more than its
	// radius
	const float inWater = std::min(mover.submerged, mover.radius);
	const float depth = std::min(inWater * 0.6f * moving, k_MostDepth);
	// A wide thing makes more foam, up to a radius of 3 units
	const float foam = moving * std::min(mover.radius / 3.0f, 1.0f);
	return {.depth = depth, .foam = foam};
}

float ImpactEnergy(const Impact& impact)
{
	constexpr float k_AcrossShare = 0.5f;
	const float down = std::max(impact.downSpeed, 0.0f) / k_UnitsPerMetre;
	const float across = k_AcrossShare * std::max(impact.acrossSpeed, 0.0f) / k_UnitsPerMetre;
	const float kilograms = std::max(impact.mass, 0.0f) * k_KilogramsPerMass;
	return 0.5f * kilograms * (down * down + across * across);
}

float CavityRadius(const Impact& impact)
{
	// The hole of an impact in water: about 0.6 of the fourth root of the energy over the weight of a cubic metre of water
	constexpr float k_WaterWeight = 1000.0f * 9.81f;
	constexpr float k_HoleShare = 0.6f;
	return k_HoleShare * std::pow(ImpactEnergy(impact) / k_WaterWeight, 0.25f) * k_UnitsPerMetre;
}

std::optional<Splash> SplashFor(const Impact& impact)
{
	const float energy = ImpactEnergy(impact);
	if (impact.downSpeed < k_LeastSplashSpeed || impact.radius <= 0.0f || energy < k_LeastSplashEnergy)
	{
		return std::nullopt;
	}
	constexpr float k_G = 9.81f;
	const float hole = CavityRadius(impact);
	// The crown rises to about two times the radius of the hole. It starts at the rim of the hole or of the thing.
	const float crown = std::min(2.2f * hole, k_HighestCrown);
	const float crownRadius = std::max(1.1f * impact.radius, 1.4f * hole);
	// The number of drops grows with the fourth root of the energy. A thing that skims throws more spray.
	const float skim = impact.acrossSpeed / (impact.acrossSpeed + impact.downSpeed + 1e-3f);
	const float dropCount = 3.0f * std::pow(energy, 0.25f) * (1.0f + 0.5f * skim);
	const auto drops = static_cast<uint32_t>(std::clamp(std::round(dropCount), 2.0f, static_cast<float>(k_MostDrops)));
	// The drops leave the rim fast enough to go a little higher than the crown
	const float dropSpeed = 1.1f * std::sqrt(2.0f * k_G * crown);
	// The drops go up at about two thirds of their speed: they show until they fall back, with half a second more
	const float seconds = 2.0f * dropSpeed * 0.7f / k_G + 0.5f;
	// A hole of half a metre or more leaves all the foam
	const float share = std::clamp(hole / (0.5f * k_UnitsPerMetre), 0.0f, 1.0f);
	return Splash {.crownHeight = crown,
	               .crownRadius = crownRadius,
	               .drops = drops,
	               .dropSpeed = dropSpeed,
	               .ringDepth = RingDepthFor(impact),
	               .seconds = seconds,
	               .foam = std::max(share, 0.15f)};
}

float RingDepthFor(const Impact& impact)
{
	if (impact.downSpeed <= 0.0f || impact.radius <= 0.0f)
	{
		return 0.0f;
	}
	// A slow thing also pushes the water aside by its own size
	const float pushed = 0.05f * impact.radius * std::min(impact.downSpeed, k_LeastSplashSpeed);
	return std::min(std::max(0.5f * CavityRadius(impact), pushed), k_DeepestRing);
}

Kernel WaveKernel()
{
	// The kernel of the vertical slope of the surface for waves of all lengths, with the short waves faded out so the
	// field stays smooth: a sum over wave numbers of q^2 exp(-q^2) J0(q r), divided by the same sum at r = 0
	constexpr float k_Step = 0.01f;
	double norm = 0.0;
	for (float q = 0.0f; q < 10.0f; q += k_Step)
	{
		norm += static_cast<double>(q * q * std::exp(-q * q));
	}
	Kernel kernel {};
	for (int z = 0; z <= k_KernelReach; ++z)
	{
		for (int x = 0; x <= k_KernelReach; ++x)
		{
			const float r = std::sqrt(static_cast<float>(x * x + z * z));
			double sum = 0.0;
			for (float q = 0.0f; q < 10.0f; q += k_Step)
			{
				sum += static_cast<double>(q * q * std::exp(-q * q)) * std::cyl_bessel_j(0.0, static_cast<double>(q * r));
			}
			kernel.at(static_cast<size_t>(z * (k_KernelReach + 1) + x)) = static_cast<float>(sum / norm);
		}
	}
	// The kernel ends at its reach, so it pulls some short waves the wrong way. The middle weight takes the least pull
	// of all waves in all directions, so that no wave of the field grows by itself.
	constexpr int k_Samples = 32;
	float least = 0.0f;
	for (int j = 0; j <= k_Samples; ++j)
	{
		for (int i = 0; i <= k_Samples; ++i)
		{
			const float kx = std::numbers::pi_v<float> * static_cast<float>(i) / k_Samples;
			const float kz = std::numbers::pi_v<float> * static_cast<float>(j) / k_Samples;
			float response = 0.0f;
			for (int z = -k_KernelReach; z <= k_KernelReach; ++z)
			{
				for (int x = -k_KernelReach; x <= k_KernelReach; ++x)
				{
					response += kernel.at(static_cast<size_t>(std::abs(z) * (k_KernelReach + 1) + std::abs(x))) *
					            std::cos(kx * static_cast<float>(x) + kz * static_cast<float>(z));
				}
			}
			least = std::min(least, response);
		}
	}
	constexpr float k_Margin = 0.01f;
	kernel.at(0) += k_Margin - least;
	return kernel;
}

float KernelResponse(const Kernel& kernel, float radiansPerTexel)
{
	float response = 0.0f;
	for (int z = -k_KernelReach; z <= k_KernelReach; ++z)
	{
		for (int x = -k_KernelReach; x <= k_KernelReach; ++x)
		{
			response += kernel.at(static_cast<size_t>(std::abs(z) * (k_KernelReach + 1) + std::abs(x))) *
			            std::cos(radiansPerTexel * static_cast<float>(x));
		}
	}
	return response;
}

float GravityTerm(const Kernel& kernel, float stepSeconds, float spacing)
{
	// A deep water wave of wave number k turns at sqrt(g k) radians a second. Each step, the kernel's response times
	// this term must be that turn squared.
	const float radiansPerTexel = 2.0f * std::numbers::pi_v<float> / k_ReferenceLength;
	const float worldNumber = radiansPerTexel / spacing;
	const float response = KernelResponse(kernel, radiansPerTexel);
	return response > 0.0f ? k_Gravity * worldNumber * stepSeconds * stepSeconds / response : 0.0f;
}

glm::ivec2 FieldCell(glm::vec2 point, float spacing)
{
	return glm::ivec2(glm::floor(point / spacing));
}

glm::vec2 FieldOrigin(glm::ivec2 centre, uint16_t side, float spacing)
{
	return glm::vec2(centre - glm::ivec2(side / 2)) * spacing;
}

glm::vec2 FieldCentre(glm::vec3 eye, glm::vec3 focus, float reach)
{
	const glm::vec2 from {eye.x, eye.z};
	const glm::vec2 ahead = glm::vec2(focus.x, focus.z) - from;
	const float length = glm::length(ahead);
	return length > reach ? from + ahead * (reach / length) : from + ahead;
}

float SettleSeconds(float height, float foam)
{
	// The height falls as exp(-damping t / 2), the foam as exp(-t / life)
	const float waves = height > k_FlatHeight ? 2.0f / k_Damping * std::log(height / k_FlatHeight) : 0.0f;
	const float suds = foam > k_FlatFoam ? k_FoamLife * std::log(foam / k_FlatFoam) : 0.0f;
	return std::max(waves, suds);
}

StepPlan FieldClock::Plan(float frameSeconds, glm::ivec2 centre, float height, float foam)
{
	StepPlan plan;
	const bool writes = height > k_FlatHeight || foam > k_FlatFoam;
	if (writes)
	{
		// The waves of an older write are smaller now by the quiet time since it
		_settle = std::max(_settle - _quiet, SettleSeconds(height, foam));
		_quiet = 0.0f;
	}
	else
	{
		_quiet += frameSeconds;
	}
	const bool wasFlat = _flat;
	_flat = !writes && _quiet >= _settle;
	if (_flat)
	{
		// Flat: nothing to step, and the field follows its centre with no cost
		_carry = 0.0f;
		_settle = 0.0f;
		_centre = centre;
		return plan;
	}
	plan.reset = wasFlat;
	_carry += frameSeconds;
	const float stepTime = 1.0f / k_StepsPerSecond;
	auto steps = static_cast<uint32_t>(_carry / stepTime);
	if (plan.reset && steps == 0)
	{
		// The first write shows at once
		steps = 1;
		_carry = stepTime;
	}
	if (steps > k_MostStepsPerFrame)
	{
		// After a long frame the field does not try to catch up
		steps = k_MostStepsPerFrame;
		_carry = static_cast<float>(steps) * stepTime;
	}
	_carry -= static_cast<float>(steps) * stepTime;
	plan.steps = steps;
	// The field moves with its centre only when it steps: with no step this frame, it stays where the last step put it
	if (steps > 0)
	{
		plan.shift = plan.reset ? glm::ivec2(0) : centre - _centre;
		_centre = centre;
	}
	return plan;
}

std::optional<Hull> HullFor(const glm::mat4& model, glm::vec3 boxLow, glm::vec3 boxHigh, float waterHeight, bool boat)
{
	// The box's corners in the world
	float lowest = 1e9f;
	float highest = -1e9f;
	for (int i = 0; i < 8; ++i)
	{
		const glm::vec3 corner {(i & 1) != 0 ? boxHigh.x : boxLow.x, (i & 2) != 0 ? boxHigh.y : boxLow.y,
		                        (i & 4) != 0 ? boxHigh.z : boxLow.z};
		const float y = (model * glm::vec4(corner, 1.0f)).y;
		lowest = std::min(lowest, y);
		highest = std::max(highest, y);
	}
	if (lowest >= waterHeight || highest <= waterHeight)
	{
		return std::nullopt;
	}
	const glm::vec3 middle = model * glm::vec4((boxLow + boxHigh) * 0.5f, 1.0f);
	// The model's x and z axes across the ground, with the box's half sizes along them
	const glm::vec3 axisX = model[0];
	const glm::vec3 axisZ = model[2];
	const glm::vec2 flatX {axisX.x, axisX.z};
	const glm::vec2 flatZ {axisZ.x, axisZ.z};
	const float halfX = 0.5f * (boxHigh.x - boxLow.x) * glm::length(flatX);
	const float halfZ = 0.5f * (boxHigh.z - boxLow.z) * glm::length(flatZ);
	if (glm::length(flatX) <= 0.0f || glm::length(flatZ) <= 0.0f)
	{
		return std::nullopt;
	}
	const bool alongX = halfX >= halfZ;
	return Hull {.centre = {middle.x, middle.z},
	             .forward = glm::normalize(alongX ? flatX : flatZ),
	             .half = alongX ? glm::vec2(halfX, halfZ) : glm::vec2(halfZ, halfX),
	             .draft = waterHeight - lowest,
	             .speed = 0.0f,
	             .boat = boat};
}

std::optional<WaterlineBox> WaterlineOf(std::span<const glm::vec3> points, float waterline, float band)
{
	std::optional<WaterlineBox> box;
	for (const auto& point : points)
	{
		if (std::abs(point.y - waterline) > band)
		{
			continue;
		}
		const glm::vec2 across {point.x, point.z};
		if (!box.has_value())
		{
			box = WaterlineBox {.low = across, .high = across};
		}
		box->low = glm::min(box->low, across);
		box->high = glm::max(box->high, across);
	}
	return box;
}

float HullDistance(const Hull& hull, glm::vec2 point)
{
	const glm::vec2 away = point - hull.centre;
	const glm::vec2 across {-hull.forward.y, hull.forward.x};
	const float u = glm::dot(away, hull.forward) / std::max(hull.half.x, 1e-3f);
	const float v = std::abs(glm::dot(away, across));
	// The width at a place along the hull: round at the stern, pointed at the bow
	const float clamped = std::clamp(u, -1.0f, 1.0f);
	const float width =
	    hull.half.y * (clamped < 0.0f ? std::sqrt(std::max(1.0f - clamped * clamped, 0.0f)) : 1.0f - clamped * clamped);
	const float side = v - width;
	const float ends = (std::abs(u) - 1.0f) * hull.half.x;
	return std::max(side, ends);
}

} // namespace openblack::graphics::underwater::wake
