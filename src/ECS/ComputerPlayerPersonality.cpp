/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ComputerPlayerPersonality.h"

#include <algorithm>

namespace openblack::computer_player::personality
{

namespace
{
constexpr uint32_t k_StateStep = 0x9E3779B9u;
constexpr uint32_t k_SeedMix = 0x9E3779B1u;
constexpr uint32_t k_GodMix = 0x85EBCA77u;
constexpr float k_ShareStep = 1.0f / 16777216.0f;
} // namespace

Stream::Stream(uint32_t seed, PlayerNames god) noexcept
    : _state(seed * k_SeedMix + (static_cast<uint32_t>(god) + 1u) * k_GodMix)
{
}

uint32_t Stream::Next() noexcept
{
	_state += k_StateStep;
	auto mixed = _state;
	mixed = (mixed ^ (mixed >> 16u)) * 0x21F0AAADu;
	mixed = (mixed ^ (mixed >> 15u)) * 0x735A2D97u;
	return mixed ^ (mixed >> 15u);
}

float Stream::NextShare() noexcept
{
	return static_cast<float>(Next() >> 8u) * k_ShareStep;
}

float WeightOf(const Aspect& aspect, float share) noexcept
{
	const float scale = k_LeastShare + ((k_MostShare - k_LeastShare) * share);
	return std::min(aspect.builtIn * scale, k_HighestWeight);
}

std::vector<Weight> WeightsFor(uint32_t seed, PlayerNames god)
{
	Stream stream(seed, god);
	std::vector<Weight> weights;
	weights.reserve(k_Aspects.size());
	for (const auto& aspect : k_Aspects)
	{
		weights.push_back({.name = aspect.name, .value = WeightOf(aspect, stream.NextShare())});
	}
	return weights;
}

} // namespace openblack::computer_player::personality
