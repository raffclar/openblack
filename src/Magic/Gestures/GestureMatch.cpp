/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GestureMatch.h"

#include <cmath>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

using namespace openblack::magic::gestures;

namespace
{
constexpr float k_Pi = glm::pi<float>();
constexpr float k_TwoPi = glm::two_pi<float>();

/// |x| > pi -> x -/+ 2pi (a different wrap from the buffer's)
float Wrap(float x)
{
	if (std::abs(x) > k_Pi)
	{
		return x < 0.0f ? x + k_TwoPi : x - k_TwoPi;
	}
	return x;
}

/// The box of keypoints s..e (inclusive)
BoundingBox KeyBox(const GestureData& data, int s, int e)
{
	BoundingBox box {data.samples[s].x, data.samples[s].z, data.samples[s].x, data.samples[s].z};
	for (int k = s + 1; k <= e; ++k)
	{
		const auto& sample = data.samples[k];
		if (sample.x < box.minX)
		{
			box.minX = sample.x;
		}
		else if (sample.x > box.maxX)
		{
			box.maxX = sample.x;
		}
		if (sample.z < box.minZ)
		{
			box.minZ = sample.z;
		}
		else if (sample.z > box.maxZ)
		{
			box.maxZ = sample.z;
		}
	}
	return box;
}

/// A normalised list (a template) keeps maxX / max(maxZ, 0.1); pixels use Aspect
void ComputeAspect(GestureData& data, float screenRatio)
{
	const auto box = KeyBox(data, 0, static_cast<int>(data.count) - 1);
	if (box.maxX <= 1.0f && box.maxZ <= 1.0f)
	{
		data.aspect = box.maxX / (box.maxZ <= 0.1f ? 0.1f : box.maxZ);
		return;
	}
	data.aspect = Aspect(box, screenRatio);
}

/// The input's direction at idx against the template's first (mirrored: 8 - d, 0 stays 0)
bool DirectionMatches(const GestureData& tpl, const GestureData& input, int index, bool mirror)
{
	if (!tpl.checkDirection)
	{
		return true;
	}
	const uint32_t d = input.samples[index].direction & 0xFFu;
	const uint32_t t0 = tpl.samples[0].direction;
	if (mirror)
	{
		return d == (t0 != 0 ? (8u - t0) & 0xFFu : 0u);
	}
	return d == (t0 & 0xFFu);
}

/// The aspect class of the matched keypoints against the template's (the "> 4" branch accepts any
/// template above 0.15, as the original does)
bool AspectMatches(const GestureData& tpl, const GestureData& input, const Result& result, float screenRatio)
{
	if (!tpl.checkAspect)
	{
		return true;
	}
	const float a = Aspect(KeyBox(input, result.start, result.end), screenRatio);
	if (a < k_AspectThin)
	{
		return tpl.aspect < k_AspectThin;
	}
	if (a > k_AspectWide)
	{
		return tpl.aspect > k_AspectThin;
	}
	return k_AspectThin <= tpl.aspect && tpl.aspect <= k_AspectWide;
}

/// The template's turns aligned to the input's from any start keypoint i0 >= 1, absorbing small
/// extra corners on either side while the error keeps shrinking; the template must be used up
bool MatchForward(const GestureData& tpl, const GestureData& input, Result& result, float screenRatio)
{
	const int inputLast = static_cast<int>(input.count) - 1;
	const int tplLast = static_cast<int>(tpl.count) - 1;
	float a = 0.0f; // the last input and template turns taken (never reset)
	float b = 0.0f;
	for (int i0 = 1; i0 < inputLast; ++i0)
	{
		if (!DirectionMatches(tpl, input, i0 - 1, false))
		{
			continue;
		}
		int i = i0;
		int j = 1;
		float error = 0.0f;
		bool failed = false;
		if (tplLast > 1)
		{
			do
			{
				float inputTurn = 0.0f;
				if (i < inputLast)
				{
					inputTurn = input.samples[i++].turn;
					a = inputTurn;
				}
				float tplTurn = 0.0f;
				if (j < tplLast)
				{
					tplTurn = tpl.samples[j++].turn;
					b = tplTurn;
				}
				error = Wrap(tplTurn - inputTurn + error);
				if (i < inputLast)
				{
					const float c = input.samples[i].turn;
					if (std::abs(c) < k_SmallTurn || std::abs(a) < k_SmallTurn)
					{
						const float e = Wrap(error - c);
						if (std::abs(error) > std::abs(e))
						{
							error = e;
							++i;
						}
					}
				}
				if (j < tplLast)
				{
					const float d = tpl.samples[j].turn;
					if (std::abs(d) < k_SmallTurn || std::abs(b) < k_SmallTurn)
					{
						const float e = Wrap(d + error);
						if (std::abs(error) > std::abs(e))
						{
							error = e;
							++j;
						}
					}
				}
				if (std::abs(error) > k_MaxError)
				{
					failed = true;
					break;
				}
			} while (j < tplLast);
		}
		if (failed)
		{
			continue;
		}
		result.start = static_cast<uint8_t>(i0 - 1);
		result.end = static_cast<uint8_t>(i);
		if (AspectMatches(tpl, input, result, screenRatio))
		{
			return true;
		}
	}
	return false;
}

/// The same with the input's turns negated, the error summed without wrapping, the direction
/// mirrored; the match is (i0, i - 1)
bool MatchMirror(const GestureData& tpl, const GestureData& input, Result& result, float screenRatio)
{
	const int inputLast = static_cast<int>(input.count) - 1;
	const int tplLast = static_cast<int>(tpl.count) - 1;
	float a = 0.0f;
	float b = 0.0f;
	for (int i0 = 1; i0 < inputLast; ++i0)
	{
		if (!DirectionMatches(tpl, input, i0 - 1, true))
		{
			continue;
		}
		int i = i0;
		int j = 1;
		float error = 0.0f;
		bool failed = false;
		if (tplLast > 1)
		{
			do
			{
				float inputTurn = 0.0f;
				if (i < inputLast)
				{
					inputTurn = -input.samples[i++].turn;
					a = inputTurn;
				}
				float tplTurn = 0.0f;
				if (j < tplLast)
				{
					tplTurn = tpl.samples[j++].turn;
					b = tplTurn;
				}
				error += tplTurn - inputTurn;
				if (i < inputLast)
				{
					const float c = -input.samples[i].turn;
					if (std::abs(c) < k_SmallTurn || std::abs(a) < k_SmallTurn)
					{
						const float e = error - c;
						if (std::abs(error) > std::abs(e))
						{
							error = e;
							++i;
						}
					}
				}
				if (j < tplLast)
				{
					const float d = tpl.samples[j].turn;
					if (std::abs(d) < k_SmallTurn || std::abs(b) < k_SmallTurn)
					{
						const float e = d + error;
						if (std::abs(error) > std::abs(e))
						{
							error = e;
							++j;
						}
					}
				}
				if (std::abs(error) > k_MaxError)
				{
					failed = true;
					break;
				}
			} while (j < tplLast);
		}
		if (failed)
		{
			continue;
		}
		result.start = static_cast<uint8_t>(i0);
		result.end = static_cast<uint8_t>(i - 1);
		if (AspectMatches(tpl, input, result, screenRatio))
		{
			return true;
		}
	}
	return false;
}
} // namespace

float openblack::magic::gestures::Aspect(const BoundingBox& box, float screenRatio)
{
	float h = (box.maxZ - box.minZ + 1.0f) * screenRatio;
	if (h < 1.0f)
	{
		h = 1.0f;
	}
	return (box.maxX - box.minX + 1.0f) / h;
}

GestureData openblack::magic::gestures::BuildFromSystem(const GestureSystem& system, float screenRatio)
{
	GestureData data;
	data.SetToZero();
	const int count = system.Count();
	for (int i = 0; i < count; ++i)
	{
		const auto& s = system.At(i);
		if (i >= count - 1 || (s.flags & 0xBu) != 0)
		{
			data.Append({s.sx, s.sy, s.sz, s.turn, s.direction});
		}
	}
	ComputeAspect(data, screenRatio);
	return data;
}

bool openblack::magic::gestures::Match(const GestureData& tpl, uint8_t index, const GestureData& input, Result& result,
                                       float screenRatio)
{
	if (MatchForward(tpl, input, result, screenRatio))
	{
		result.gesture = tpl.gesture;
		result.reversed = false;
	}
	else if (tpl.allowReverse && MatchMirror(tpl, input, result, screenRatio))
	{
		result.gesture = tpl.gesture;
		result.reversed = true;
	}
	else
	{
		result.gesture = k_None;
		result.reversed = false;
		result.end = 0;
		result.start = 0;
		return false;
	}
	result.templateIndex = index;
	return true;
}

bool openblack::magic::gestures::MatchGesture(const std::vector<GestureData>& list, Gesture gesture, const GestureData& input,
                                              Result& result, float screenRatio)
{
	for (size_t t = 0; t < list.size(); ++t)
	{
		if (list[t].gesture == gesture && Match(list[t], static_cast<uint8_t>(t), input, result, screenRatio))
		{
			return true;
		}
	}
	return false;
}

Packet openblack::magic::gestures::PacketFromResult(const std::vector<GestureData>& list, const GestureSystem& system,
                                                    const Result& result, const Projection& projection)
{
	Packet packet {
	    .gesture = result.gesture,
	    .reversed = result.reversed,
	};
	int first = 0;
	int last = 0;
	system.KeypointIndices(result.start, result.end, first, last);
	// the first template of that gesture gives the position mode
	uint8_t mode = 0;
	for (const auto& tpl : list)
	{
		if (tpl.gesture == result.gesture)
		{
			mode = tpl.positionMode;
			break;
		}
	}
	if (mode != 2 || !projection.screenToLand || !projection.rayDirection)
	{
		packet.position = system.At(first).world;
		packet.size = 1.0f;
		return packet;
	}
	const auto box = system.Box(first, last);
	// the centre of the box
	const glm::vec2 centre(box.minX + (box.maxX - box.minX + 1.0f) * 0.5f, box.minZ + (box.maxZ - box.minZ + 1.0f) * 0.5f);
	const float width = std::max(box.maxX - box.minX + 1.0f, box.maxZ - box.minZ + 1.0f);
	const auto pixel = glm::vec2(glm::ivec2(centre));
	// the land lookup is not checked: off the land the point stays as it was (inferred: the original keeps the
	// output's previous contents; the camera's position here gives distance 0, so size 0)
	const glm::vec3 p = projection.screenToLand(pixel).value_or(projection.cameraPosition);
	const float distance = glm::distance(p, projection.cameraPosition);
	// the point at that distance on the ray through (centre.x + width / 2, centre.y)
	const auto edgePixel = glm::vec2(glm::ivec2(glm::vec2(centre.x + width * 0.5f, centre.y)));
	const glm::vec3 q = projection.cameraPosition + projection.rayDirection(edgePixel) * distance;
	const glm::vec3 dq = q - projection.cameraPosition;
	const glm::vec3 dp = p - projection.cameraPosition;
	const float along = dq.x * projection.yawAxis.x + dq.z * projection.yawAxis.y;
	const float alongP = dp.x * projection.yawAxis.x + dp.z * projection.yawAxis.y;
	packet.size = std::abs(along - alongP) * 1.05f;
	packet.position = p;
	return packet;
}
