/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GestureBuffer.h"

#include <cmath>

#include <glm/gtc/constants.hpp>

using namespace openblack::magic::gestures;

namespace
{
constexpr float k_Pi = glm::pi<float>();
constexpr float k_TwoPi = glm::two_pi<float>();
/// 1e-4: below it a position counts as (0, 0, 0)
constexpr float k_Zero = 1e-4f;

bool NonZero(const Sample& s)
{
	return std::abs(s.sx) > k_Zero || std::abs(s.sy) > k_Zero || std::abs(s.sz) > k_Zero;
}
} // namespace

int openblack::magic::gestures::RoundHalfDown(float v)
{
	int n = static_cast<int>(v);
	if (v - static_cast<float>(n) > 0.5f)
	{
		++n;
	}
	return n;
}

float openblack::magic::gestures::WrapDifference(float a, float b)
{
	float d = b - a;
	if (d > k_Pi)
	{
		return -(k_TwoPi - d);
	}
	if (d <= -k_Pi)
	{
		d += k_TwoPi;
	}
	return d;
}

float openblack::magic::gestures::Atan2Positive(float x, float z)
{
	float a = std::atan2(z, x);
	if (a < 0.0f)
	{
		a += k_TwoPi;
	}
	return a;
}

uint32_t openblack::magic::gestures::Octant(float heading)
{
	float a = heading + glm::half_pi<float>();
	while (a > k_TwoPi)
	{
		a -= k_TwoPi;
	}
	// (n % 8) with C's sign; n >= 0 here
	const int n = RoundHalfDown(a * 8.0f * (1.0f / k_TwoPi)) % 8;
	return static_cast<uint32_t>(n < 0 ? n + 8 : n);
}

uint8_t GestureSystem::Physical(int i) const
{
	if (i > _count)
	{
		return 0;
	}
	return static_cast<uint8_t>((_head - _count + i + k_Size) % k_Size);
}

void GestureSystem::Clear()
{
	_count = 0;
	_head = 0;
	_stationary = 0;
	_samples.fill(Sample {});
}

void GestureSystem::AddSample(const glm::vec3& world, glm::ivec2 mouse)
{
	for (;;)
	{
		auto& sample = _samples[_head];
		sample.world = world;
		sample.sx = static_cast<float>(mouse.x);
		sample.sz = static_cast<float>(mouse.y);
		if (_count < k_Size)
		{
			++_count;
		}
		if (_count > 1)
		{
			// the head is not moved yet, so this is the sample from two messages ago (kept as the original has it)
			const auto& previous = _samples[Physical(_count - 2)];
			if (previous.sx - static_cast<float>(mouse.x) == 0.0f && previous.sz - static_cast<float>(mouse.y) == 0.0f)
			{
				if (++_stationary >= k_StationaryLimit)
				{
					Clear();
					continue; // the sample goes into the emptied buffer
				}
			}
			else
			{
				_stationary = 0;
			}
		}
		break;
	}
	_head = static_cast<uint8_t>((_head + 1) % k_Size);
	ProcessNewSample(_count - 1);
}

void GestureSystem::AddSampleAtLastWorld(glm::ivec2 mouse)
{
	if (_count == 0)
	{
		return;
	}
	const auto world = At(_count - 1).world;
	AddSample(world, mouse);
}

int GestureSystem::PrevNonZero(int i) const
{
	for (int j = i - 1; j > 0; --j)
	{
		if (At(j).flags != 0)
		{
			return j;
		}
	}
	return 0;
}

int GestureSystem::PrevCorner(int i) const
{
	for (int j = i - 1; j > 0; --j)
	{
		if (At(j).flags == Sample::k_Corner)
		{
			return j;
		}
	}
	return 0;
}

int GestureSystem::PrevAnchor(int i) const
{
	for (int j = i - 1; j != 0; --j)
	{
		const auto& s = At(j);
		if (s.flags != 0)
		{
			return j;
		}
		if (NonZero(s) && Far(s, At(i)))
		{
			return j;
		}
	}
	return 0;
}

bool GestureSystem::Far(const Sample& a, const Sample& b)
{
	return std::abs(b.sx - a.sx) >= 4.0f || std::abs(b.sz - a.sz) >= 4.0f;
}

float GestureSystem::Turn(const Sample& a, const Sample& b, const Sample& c)
{
	const float h1 = Atan2Positive(b.sx - a.sx, b.sz - a.sz);
	const float h2 = Atan2Positive(c.sx - b.sx, c.sz - b.sz);
	return WrapDifference(h1, h2);
}

int GestureSystem::FindCorner(int k, int i) const
{
	if (i <= 1)
	{
		return 0;
	}
	const auto& last = At(i);
	const auto& from = At(k);
	int best = 0;
	float bestTurn = 0.0f;
	for (int j = PrevAnchor(i); j > k; --j)
	{
		const auto& s = At(j);
		if (!Far(from, s))
		{
			break;
		}
		const float a = std::abs(Turn(from, s, last));
		if (a >= k_CornerTurn && a > bestTurn)
		{
			bestTurn = a;
			best = j;
		}
	}
	if (best != 0)
	{
		return best;
	}
	if (k == 0)
	{
		return 0;
	}
	const int m = PrevNonZero(k);
	return std::abs(Turn(At(m), from, last)) >= k_CornerTurn ? k : 0;
}

bool GestureSystem::LongEnough(int a, int b, float dx, float dz) const
{
	const float m = std::max(std::abs(dx), std::abs(dz));
	if (m >= 12.0f)
	{
		return true;
	}
	if (m <= 4.0f)
	{
		return false;
	}
	const int s = std::min(a != 0 ? PrevCorner(a) : 0, std::max(b - 8, 0));
	const auto box = Box(s, b);
	return std::max(box.maxZ - box.minZ + 1.0f, box.maxX - box.minX + 1.0f) < 50.0f;
}

int GestureSystem::MergeOrReject(int c, int i)
{
	auto copyPosition = [](Sample& to, const Sample& from) {
		to.sx = from.sx;
		to.sy = from.sy;
		to.sz = from.sz;
	};
	auto& corner = At(c);
	const int p = PrevCorner(c);
	if (p != 0)
	{
		auto& previous = At(p);
		if (LongEnough(p, c, corner.sx - previous.sx, corner.sz - previous.sz))
		{
			return 0;
		}
		const auto& before = At(PrevCorner(p));
		const auto& last = At(i);
		const float a1 = std::abs(Turn(before, previous, last));
		const float a2 = std::abs(Turn(before, corner, last));
		if (a2 < a1)
		{
			copyPosition(corner, previous);
		}
		else
		{
			copyPosition(previous, corner);
		}
		return 1;
	}
	const auto& start = At(0);
	if (LongEnough(0, c, corner.sx - start.sx, corner.sz - start.sz))
	{
		return 0;
	}
	copyPosition(corner, start);
	return 1;
}

void GestureSystem::UpdateHeading(int i)
{
	const int p = PrevCorner(i);
	auto& corner = At(p);
	const auto& last = At(i);
	corner.heading = Atan2Positive(last.sx - corner.sx, last.sz - corner.sz);
	corner.direction = Octant(corner.heading);
	// the corner's turn from the one before it (the start keeps its own)
	if (p != 0)
	{
		corner.turn = WrapDifference(At(PrevCorner(p)).heading, corner.heading);
	}
}

void GestureSystem::ProcessNewSample(int i)
{
	if (i != 0 && At(i - 1).flags == Sample::k_End)
	{
		At(i - 1).flags = 0;
	}
	At(0).flags = Sample::k_Start;
	At(i).flags = Sample::k_End;
	if (i == 0)
	{
		return;
	}
	const int k = PrevNonZero(i);
	if (const int c = FindCorner(k, i); c != 0)
	{
		if (MergeOrReject(c, i) == 0)
		{
			At(c).flags = Sample::k_Corner;
			UpdateHeading(c);
		}
		At(i).flags = Sample::k_Anchor;
	}
	UpdateHeading(i);
}

BoundingBox GestureSystem::Box(int s, int b) const
{
	int p = Physical(s);
	BoundingBox box {_samples[p].sx, _samples[p].sz, _samples[p].sx, _samples[p].sz};
	int n = b - s;
	if (n >= _count)
	{
		n = _count;
	}
	if (n < 0)
	{
		n += k_Size;
	}
	p = (p + 1) % k_Size;
	for (int k = n - 1; k > 0; --k)
	{
		const auto& sample = _samples[p];
		if (NonZero(sample))
		{
			if (sample.sx < box.minX)
			{
				box.minX = sample.sx;
			}
			else if (sample.sx > box.maxX)
			{
				box.maxX = sample.sx;
			}
			if (sample.sz < box.minZ)
			{
				box.minZ = sample.sz;
			}
			else if (sample.sz > box.maxZ)
			{
				box.maxZ = sample.sz;
			}
		}
		p = (p + 1) % k_Size;
	}
	return box;
}

void GestureSystem::KeypointIndices(int start, int end, int& first, int& last) const
{
	auto isKey = [this](int i) { return i >= _count - 1 || (At(i).flags & 0xBu) != 0; };
	int keys = 0;
	int i = 0;
	if (start == 0)
	{
		first = 0;
	}
	else
	{
		for (; i < _count; ++i)
		{
			if (isKey(i) && ++keys > start)
			{
				first = i++;
				break;
			}
		}
	}
	for (; i < _count; ++i)
	{
		if (isKey(i) && ++keys > end)
		{
			last = i;
			return;
		}
	}
}
