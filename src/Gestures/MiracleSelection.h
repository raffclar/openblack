/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <functional>
#include <optional>
#include <span>
#include <vector>

#include "Enums.h"

/// The miracle selection. With an empty hand, the player draws a spiral (for the player's own miracles) or an inverse
/// spiral (for the miracles of the creature). The selection then offers each miracle that a worship site icon of the
/// player can give for that gesture. Each miracle has a chain of gestures: the first gesture opens the selection, and
/// the next gesture (the gesture of the miracle) calls the miracle into the hand from its icon. A scribble closes the
/// selection, and the selection closes by itself after a time.
namespace openblack::gesture
{

/// A miracle has a chain of this many gestures at most. The first gesture opens the selection.
inline constexpr size_t k_SelectionStages = 3;
/// The number of kinds of seed that the selection knows
inline constexpr size_t k_SelectionSeeds = 30;
/// The number of gesture kinds
inline constexpr size_t k_GestureKinds = 24;

/// A miracle that an icon of the player can give, with its chain of gestures
struct SelectionOffer
{
	SpellSeedType seed {SpellSeedType::None};
	std::array<GestureType, k_SelectionStages> chain {GestureType::None, GestureType::None, GestureType::None};
};

/// A gesture that the open selection waits for, and the seed that its hint shows
struct SelectionWait
{
	GestureType gesture {GestureType::None};
	SpellSeedType seed {SpellSeedType::None};

	bool operator==(const SelectionWait&) const = default;
};

class MiracleSelection
{
public:
	/// What a gesture of the open selection did
	struct Outcome
	{
		enum class Kind : uint8_t
		{
			/// No miracle that the player can still have has this gesture now
			Unmatched,
			/// A miracle has this gesture and more gestures follow it in its chain
			Advanced,
			/// The gesture is the last of the chain of a miracle: the selection closes and the miracle comes into the hand
			Chosen,
		};
		Kind kind {Kind::Unmatched};
		/// Chosen: the miracle
		SpellSeedType seed {SpellSeedType::None};
	};

	/// Opens the selection with a gesture. The selection offers each miracle of the offers whose chain starts with the
	/// gesture. It stays closed when it has nothing to offer. Returns whether it opened.
	bool Open(GestureType opening, std::span<const SelectionOffer> offers);
	void Close();
	[[nodiscard]] bool IsOpen() const { return _open; }
	/// The gesture that opened the selection
	[[nodiscard]] GestureType OpenedWith() const { return _openedWith; }
	/// Time passes. The selection closes when it stays open for longer than the time limit.
	void Update(float seconds, float timeoutSeconds);
	/// The gestures that the selection waits for, in the order of the gesture numbers. The hint of each gesture shows the
	/// last of its miracles in the order of the seeds.
	[[nodiscard]] std::vector<SelectionWait> Waiting() const;
	/// The hand drew a gesture that the selection waits for. Only a miracle that is still valid for the hand matches.
	/// The miracles that do not match stop. When no miracle stays, the selection closes.
	Outcome Recognised(GestureType gesture, const std::function<bool(SpellSeedType)>& stillValid);

private:
	bool _open {false};
	/// The stage of the chains that the next gesture must match
	size_t _stage {1};
	float _seconds {0.0f};
	GestureType _openedWith {GestureType::None};
	/// The chain of each miracle that the selection still offers, by seed
	std::array<std::optional<std::array<GestureType, k_SelectionStages>>, k_SelectionSeeds> _candidates {};
	/// The gestures that the selection waits for, by gesture number
	std::array<bool, k_GestureKinds> _waiting {};
};

} // namespace openblack::gesture
