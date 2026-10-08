/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BeamMaths.h"

#include <cmath>

#include <array>

#include "KeyPointSpline.h"

using namespace openblack::particles;

float maths::BeamBulge(float t)
{
	const float f = (t + t) - 1.0f;
	return 1.0f - (f * f);
}

std::vector<glm::vec3> maths::BeamKeyPoints(glm::vec3 start, glm::vec3 end, int count, const BeamWiggle& wiggle, float age,
                                            int beam, const std::function<float(float)>& noise,
                                            const std::function<float(glm::vec2)>& landHeight)
{
	std::vector<glm::vec3> keys;
	if (count < 2)
	{
		return keys;
	}
	keys.reserve(static_cast<size_t>(count));
	const float step = 1.0f / (static_cast<float>(count) - 1.0f);
	const auto offset = static_cast<float>(beam);
	for (int i = 0; i < count; ++i)
	{
		const float t = static_cast<float>(i) * step;
		auto point = start + ((end - start) * t);
		if (i != 0 && i != count - 1)
		{
			const float bulge = BeamBulge(t);
			const float along = t * wiggle.frequency;
			point.x += noise((age * wiggle.speed) + along + offset) * wiggle.amount * bulge;
			point.z += noise((age * wiggle.speed * k_BeamDepthDriftShare) + along + offset) * wiggle.amount * bulge;
			point.y += (noise((age * wiggle.speed * k_BeamHeightDriftShare) + along + offset) + 1.0f) * wiggle.amount * bulge *
			           k_BeamHeightShare;
			const float ground = landHeight({point.x, point.z});
			if (point.y - ground < wiggle.minHeight)
			{
				point.y = ground + wiggle.minHeight;
			}
		}
		keys.push_back(point);
	}
	return keys;
}

std::vector<maths::BeamJoint> maths::BeamJoints(std::span<const glm::vec3> keys, size_t count, float minScale, float maxScale)
{
	std::vector<BeamJoint> joints;
	if (keys.size() < 2 || count == 0)
	{
		return joints;
	}
	// One curve for each axis, through the key points at their shares of the way
	const float keyStep = 1.0f / (static_cast<float>(keys.size()) - 1.0f);
	std::array<std::vector<float>, 3> pairs;
	for (size_t i = 0; i < keys.size(); ++i)
	{
		const float t = static_cast<float>(i) * keyStep;
		for (size_t axis = 0; axis < pairs.size(); ++axis)
		{
			pairs.at(axis).push_back(t);
			pairs.at(axis).push_back(keys[i][static_cast<glm::length_t>(axis)]);
		}
	}
	const std::array<KeyPointSpline, 3> curves {KeyPointSpline(pairs[0]), KeyPointSpline(pairs[1]), KeyPointSpline(pairs[2])};
	// A lone joint sits at the start
	const float step = count > 1 ? 1.0f / (static_cast<float>(count) - 1.0f) : 0.0f;
	const float range = maxScale - minScale;
	joints.reserve(count);
	// Where two key points share a place on the curve it gives nothing, and the joint stays where the last one was
	glm::vec3 position = keys.back();
	for (size_t j = 0; j < count; ++j)
	{
		const float u = static_cast<float>(j) * step;
		for (size_t axis = 0; axis < curves.size(); ++axis)
		{
			const auto component = static_cast<glm::length_t>(axis);
			position[component] = curves.at(axis).Evaluate(u, position[component]);
		}
		joints.push_back({.position = position, .scale = (BeamBulge(u) * range) + minScale});
	}
	return joints;
}

uint8_t maths::PlasmaAlpha(float age, float life, uint8_t alpha, int32_t maxAlpha)
{
	float t = age / life;
	t = t <= 0.0f ? 0.0f : (t < 1.0f ? t : 1.0f);
	const int32_t most = static_cast<int32_t>(alpha) * maxAlpha;
	// Scaled by a 255th as a float, cut down to a whole number and its lowest byte kept
	constexpr float k_Byte = 1.0f / 255.0f;
	return static_cast<uint8_t>(static_cast<int32_t>(static_cast<float>(most) * BeamBulge(t) * k_Byte) & 0xFF);
}

maths::HermiteCurve maths::PlasmaCurve(glm::vec3 start, glm::vec3 end, glm::vec3 startTangent, glm::vec3 endTangent)
{
	return {
	    .c3 = (((start * 2.0f) + (end * -2.0f)) + startTangent) + endTangent,
	    .c2 = (((start * -3.0f) + (end * 3.0f)) + (startTangent * -2.0f)) + (endTangent * -1.0f),
	    .c1 = (((start * 0.0f) + (end * 0.0f)) + startTangent) + (endTangent * 0.0f),
	    .c0 = (((start * 1.0f) + (end * 0.0f)) + (startTangent * 0.0f)) + (endTangent * 0.0f),
	};
}

glm::vec3 maths::At(const HermiteCurve& curve, float t)
{
	const float t2 = t * t;
	const float t3 = t2 * t;
	return (((curve.c3 * t3) + (curve.c2 * t2)) + (curve.c1 * t)) + curve.c0;
}

std::pair<glm::vec3, glm::vec3> maths::PlasmaTangents(glm::vec3 start, glm::vec3 end, glm::vec3 startTangent,
                                                      glm::vec3 endTangent, glm::vec3 startNudge, glm::vec3 endNudge,
                                                      float randomShare, float scale)
{
	const glm::vec3 d = start - end;
	const auto length =
	    static_cast<float>(std::sqrt(static_cast<double>((d.z * d.z) + (d.y * d.y)) + static_cast<double>(d.x * d.x)));
	const auto first = (startNudge * randomShare) + startTangent;
	const auto second = (endNudge * randomShare) + endTangent;
	const float s = length * scale;
	return {first * s, second * s};
}

std::vector<glm::vec3> maths::PlasmaKeyPoints(const HermiteCurve& curve, int count, float frequency, float amount, float drift,
                                              int beam, const std::function<float(float)>& noise)
{
	std::vector<glm::vec3> keys;
	if (count < 2)
	{
		return keys;
	}
	keys.reserve(static_cast<size_t>(count));
	const float step = 1.0f / (static_cast<float>(count) - 1.0f);
	const auto offset = static_cast<float>(beam);
	for (int i = 0; i < count; ++i)
	{
		const float t = static_cast<float>(i) * step;
		auto point = At(curve, t);
		if (i != 0 && i != count - 1)
		{
			const float bulge = BeamBulge(t);
			const float along = t * frequency;
			// Across, then the other way across, then upwards, each drifting at its own share
			point.x = (noise((along + offset) + drift) * amount * bulge) + point.x;
			point.z = (noise((along + (drift * k_BeamDepthDriftShare)) + offset) * amount * bulge) + point.z;
			point.y =
			    ((noise((along + (drift * k_BeamHeightDriftShare)) + offset) + 1.0f) * amount * bulge * k_BeamHeightShare) +
			    point.y;
		}
		keys.push_back(point);
	}
	return keys;
}
