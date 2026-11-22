/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BlueNoise.h"

#include <cmath>

#include <algorithm>
#include <functional>
#include <limits>
#include <random>

namespace openblack::graphics::blue_noise
{

namespace
{

/// The spread of the energy that each point gives to the texels near it
constexpr double k_Sigma = 1.5;
/// The points that the first pattern has, as a part of all the texels
constexpr uint32_t k_FirstPart = 10;
/// The seed of the first pattern, so that each call gives the same square
constexpr uint32_t k_Seed = 1;

/// A square of points. Each texel keeps the sum of the energy that the points near it give it.
class Points
{
public:
	explicit Points(uint32_t side)
	    : _side(side)
	    , _radius(static_cast<int>(std::min<uint32_t>(side / 2, static_cast<uint32_t>(std::ceil(k_Sigma * 4.0)))))
	    , _on(static_cast<size_t>(side) * side, 0)
	    , _energy(static_cast<size_t>(side) * side, 0.0)
	{
		const auto across = static_cast<size_t>((_radius * 2) + 1);
		_kernel.resize(across * across);
		for (int dy = -_radius; dy <= _radius; ++dy)
		{
			for (int dx = -_radius; dx <= _radius; ++dx)
			{
				const auto distance = static_cast<double>((dx * dx) + (dy * dy));
				_kernel[(static_cast<size_t>(dy + _radius) * across) + static_cast<size_t>(dx + _radius)] =
				    std::exp(-distance / (2.0 * k_Sigma * k_Sigma));
			}
		}
	}

	[[nodiscard]] bool On(size_t texel) const { return _on[texel] != 0; }

	void Set(size_t texel, bool on)
	{
		_on[texel] = on ? 1 : 0;
		const double sign = on ? 1.0 : -1.0;
		const auto side = static_cast<int>(_side);
		const auto x = static_cast<int>(texel % _side);
		const auto y = static_cast<int>(texel / _side);
		const auto across = static_cast<size_t>((_radius * 2) + 1);
		for (int dy = -_radius; dy <= _radius; ++dy)
		{
			const auto row = static_cast<size_t>((y + dy + side) % side) * _side;
			for (int dx = -_radius; dx <= _radius; ++dx)
			{
				const auto column = static_cast<size_t>((x + dx + side) % side);
				_energy[row + column] +=
				    sign * _kernel[(static_cast<size_t>(dy + _radius) * across) + static_cast<size_t>(dx + _radius)];
			}
		}
	}

	/// The point with the most energy: the centre of the tightest cluster
	[[nodiscard]] size_t TightestCluster() const { return Find(true, std::greater<>()); }

	/// The empty texel with the least energy: the centre of the largest void
	[[nodiscard]] size_t LargestVoid() const { return Find(false, std::less<>()); }

private:
	template <typename Better>
	[[nodiscard]] size_t Find(bool on, Better better) const
	{
		size_t found = 0;
		bool any = false;
		for (size_t texel = 0; texel < _on.size(); ++texel)
		{
			if (On(texel) == on && (!any || better(_energy[texel], _energy[found])))
			{
				found = texel;
				any = true;
			}
		}
		return found;
	}

	uint32_t _side;
	int _radius;
	std::vector<double> _kernel;
	std::vector<uint8_t> _on;
	std::vector<double> _energy;
};

} // namespace

std::vector<uint16_t> Ranks(uint32_t side)
{
	const auto count = static_cast<size_t>(side) * side;
	std::vector<uint16_t> ranks(count, 0);
	if (count == 0 || count > static_cast<size_t>(std::numeric_limits<uint16_t>::max()) + 1)
	{
		return {};
	}
	// A first pattern of random points
	Points first(side);
	std::mt19937 random(k_Seed);
	size_t points = std::max<size_t>(1, count / k_FirstPart);
	for (size_t placed = 0; placed < points;)
	{
		const auto texel = static_cast<size_t>(random() % count);
		if (!first.On(texel))
		{
			first.Set(texel, true);
			++placed;
		}
	}
	// Move the point of the tightest cluster into the largest void, until it stays where it is
	for (size_t move = 0; move < count; ++move)
	{
		const auto cluster = first.TightestCluster();
		first.Set(cluster, false);
		const auto empty = first.LargestVoid();
		first.Set(empty, true);
		if (empty == cluster)
		{
			break;
		}
	}
	// The points of the first pattern get the low ranks: the point of the tightest cluster gets the highest of these
	Points fewer = first;
	for (auto left = points; left > 0; --left)
	{
		const auto cluster = fewer.TightestCluster();
		fewer.Set(cluster, false);
		ranks[cluster] = static_cast<uint16_t>(left - 1);
	}
	// Each of the other texels gets the next rank when it is the largest void
	Points more = first;
	for (; points < count; ++points)
	{
		const auto empty = more.LargestVoid();
		more.Set(empty, true);
		ranks[empty] = static_cast<uint16_t>(points);
	}
	return ranks;
}

std::vector<uint8_t> Bytes(std::span<const uint16_t> ranks)
{
	std::vector<uint8_t> bytes(ranks.size(), 0);
	if (ranks.empty())
	{
		return bytes;
	}
	std::ranges::transform(ranks, bytes.begin(), [count = ranks.size()](uint16_t rank) {
		return static_cast<uint8_t>((static_cast<size_t>(rank) * 256) / count);
	});
	return bytes;
}

} // namespace openblack::graphics::blue_noise
