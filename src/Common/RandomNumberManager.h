/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <functional>
#include <mutex>
#include <optional>
#include <random>

namespace openblack
{

/// The game's own generators that aren't the original's: one for the general draws and one for each system drawing
/// often, so that one system drawing more doesn't move another's draws. Every stream starts from the run's one seed.
enum class RandomStream : uint8_t
{
	General,
	CreatureAnimation,
	CreatureFight,
	CreatureLocomotion,
	CreatureMind,
	CreaturePhysiology,
	CreatureSpawner,
	Count,
};

namespace run_seed
{
/// Spreads a number's bits over the whole word, so that close seeds give unrelated streams
[[nodiscard]] constexpr uint32_t Mix(uint32_t value) noexcept
{
	value ^= value >> 16u;
	value *= 0x7FEB352Du;
	value ^= value >> 15u;
	value *= 0x846CA68Bu;
	value ^= value >> 16u;
	return value;
}

/// The seed a stream starts from for a run's seed: the same for the same two, different for each stream
[[nodiscard]] constexpr uint32_t StreamSeed(uint32_t runSeed, RandomStream stream) noexcept
{
	return Mix(runSeed ^ Mix(static_cast<uint32_t>(stream) + 1u));
}
} // namespace run_seed

class RandomNumberManagerInterface
{
public:
	template <typename T>
	    requires(std::is_arithmetic_v<T>)
	T NextValue(T min, T max)
	{
		using dist_t =
		    std::conditional_t<std::is_integral_v<T>, std::uniform_int_distribution<T>, std::uniform_real_distribution<T>>;
		dist_t dist(min, max);
		std::optional<std::reference_wrapper<std::mutex>> lock(LockAccess());
		if (lock)
		{
			std::lock_guard<std::mutex> const contextLock(*lock);
			return dist(Generator());
		}
		return dist(Generator());
	}

	template <typename T>
	    requires((std::is_same_v<T, std::array<typename T::value_type, std::tuple_size<T>::value>> &&
	              std::tuple_size<T>::value > 1) ||
	             std::is_same_v<T, std::vector<typename T::value_type>>)
	auto Choose(const T& container) -> const decltype(container[0])&
	{
		const auto size = container.size();
		if constexpr (std::is_same_v<T, std::vector<typename T::value_type>>)
		{
			assert(size > 0);
		}
#if __has_cpp_attribute(assume)
		[[assume(size > 0)]];
#endif

		const auto index = NextValue<std::size_t>(0, size - 1);
		return container[index];
	}

	virtual ~RandomNumberManagerInterface() = default;

	/// The seed every stream started from: random for each run unless given (--seed, or the inspector's game.seed)
	[[nodiscard]] virtual uint32_t GetRunSeed() const = 0;
	/// Starts every stream again from a seed, so that the same seed gives the same draws
	virtual void SetRunSeed(uint32_t seed) = 0;
	/// A stream's generator
	[[nodiscard]] virtual std::mt19937& Stream(RandomStream stream) = 0;

protected:
	virtual std::mt19937& Generator() = 0;
	virtual std::optional<std::reference_wrapper<std::mutex>> LockAccess() = 0;
};

/// A system's stream, used where a generator is: draws come from the run's seeded stream when the game's random numbers
/// are there, or from a generator of its own on the standard seed otherwise (in tests that make the system alone)
class RandomStreamSource
{
public:
	using result_type = std::mt19937::result_type;

	explicit RandomStreamSource(RandomStream stream) noexcept
	    : _stream(stream)
	{
	}

	[[nodiscard]] static constexpr result_type min() { return std::mt19937::min(); }
	[[nodiscard]] static constexpr result_type max() { return std::mt19937::max(); }
	result_type operator()();

private:
	RandomStream _stream;
	std::mt19937 _own;
};

} // namespace openblack
