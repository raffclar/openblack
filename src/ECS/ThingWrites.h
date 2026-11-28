/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <atomic>
#include <memory>
#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>

namespace openblack::ecs
{

/// The marks of the things, by the numbers of their entities, that can have changed since the last take. Many threads
/// can mark at the same time, and a take can occur while other threads mark. A mark stays until a take gives it. The
/// marks have no memory and cost almost nothing until the first take.
class ThingWrites
{
public:
	/// The most entity numbers that the registry gives
	static constexpr size_t k_Numbers = size_t {entt::entt_traits<entt::entity>::entity_mask} + 1;

	ThingWrites();
	~ThingWrites();
	ThingWrites(const ThingWrites&) = delete;
	ThingWrites& operator=(const ThingWrites&) = delete;

	/// Marks the number of the entity. It does nothing before the first take.
	void Mark(entt::entity entity) noexcept
	{
		auto* words = _words.load(std::memory_order_acquire);
		if (words == nullptr || entity == entt::null)
		{
			return;
		}
		const auto number = static_cast<size_t>(entt::to_entity(entity));
		const auto bit = uint64_t {1} << (number % 64);
		auto& word = words->at(number / 64);
		// Set the bit of the summary after the bit of the number. Then a take that finds the bit of the summary also finds
		// the number.
		if ((word.load(std::memory_order_relaxed) & bit) == 0)
		{
			word.fetch_or(bit, std::memory_order_relaxed);
			_summary.at(number / 64 / 64).fetch_or(uint64_t {1} << (number / 64 % 64), std::memory_order_release);
		}
	}
	/// Tells that any thing can have changed, as after a direct write to the stores
	void MarkAll() noexcept { _all.store(true, std::memory_order_release); }
	/// The marked numbers in increasing order, and clears them. None when all the things can have changed, also at the
	/// first take, because the marks start then.
	[[nodiscard]] std::optional<std::vector<uint32_t>> Take();

private:
	static constexpr size_t k_Words = k_Numbers / 64;
	static constexpr size_t k_SummaryWords = k_Words / 64;

	using Words = std::array<std::atomic<uint64_t>, k_Words>;
	/// One bit for each number. Made at the first take.
	std::atomic<Words*> _words {nullptr};
	std::unique_ptr<Words> _ownWords;
	/// One bit for each word of _words that can hold a mark
	std::array<std::atomic<uint64_t>, k_SummaryWords> _summary {};
	std::atomic<bool> _all {false};
};

} // namespace openblack::ecs
