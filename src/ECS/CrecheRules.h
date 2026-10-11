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

#include <functional>
#include <optional>
#include <span>

#include <glm/vec2.hpp>

#include "Enums.h"

/// The rules of a town's crèche and of the children who spend their days there. A town takes the first of its crèches
/// to work as its own. By day its children walk to its door and then stroll along the paths marked out in its model,
/// five points out and the same five back, a sound playing at the crèche at each new leg; at night they wait for their
/// home to be lived in again and go home. Free of the game's state so they can be tested on their own.
namespace openblack::creche
{

/// A drawing a whole number below a limit, none for a limit of none
using IntRandom = std::function<uint32_t(uint32_t)>;
/// A drawing below a limit
using FloatRandom = std::function<float(float)>;

/// The points of each path in the crèche's model: a child walks out along them and back
inline constexpr uint32_t k_PointsPerPath = 5;
/// The legs of a stroll along a path: out to its last point and back to its first
inline constexpr int32_t k_LegsPerStroll = 2 * static_cast<int32_t>(k_PointsPerPath);
/// The sky at which children stop strolling for the night, on openblack's scale (0 at night, 1 at dusk, 2 by day).
/// The game keeps its sky the other way up and strolls while its own value is no more than 1.2.
inline constexpr float k_DaySkyLeast = 0.8f;
/// How far a child strays from each point of its path, at most half of this either way
inline constexpr float k_Stray = 1.0f;

/// Where a child is on its stroll: the path it walks and how many legs along it it has gone
struct Stroll
{
	int32_t path {0};
	int32_t leg {0};
};

/// The next leg of a child's stroll: where it has got to, and the point of the crèche's paths it makes for next (none:
/// the crèche's door, when the model marks out no paths)
struct NextLeg
{
	Stroll stroll;
	std::optional<size_t> point;
};

/// The paths marked out in a crèche model with these many points: whole paths of five points only
[[nodiscard]] constexpr uint32_t PathsIn(size_t points)
{
	return static_cast<uint32_t>(points / k_PointsPerPath);
}

/// The next leg of a child's stroll. A child at the crèche's door takes a path at random from its start. Otherwise it
/// stays on its path (the last one if its path is past the paths there are) and goes on a leg, out along the path's
/// points and then back; after the last leg back it starts afresh on a path at random.
[[nodiscard]] NextLeg NextStrollLeg(Stroll stroll, uint32_t paths, bool atDoor, const IntRandom& random);

/// A child makes for a point of its path straying a little from it: up to half a metre along each way, drawn along
/// the land's second axis first
[[nodiscard]] glm::vec2 Strayed(glm::vec2 point, const FloatRandom& random);

/// Whether the sky is light enough for children to stroll at the crèche
[[nodiscard]] constexpr bool IsDaytime(float skyType)
{
	return skyType >= k_DaySkyLeast;
}

/// What a child at the crèche does, once it has seen to its hunger and what its town wants of it
enum class AtCreche
{
	/// The next leg of its stroll, the crèche's sound playing
	Stroll,
	/// The next leg of its stroll, quietly: a homeless child touching the crèche at night
	StrollQuietly,
	/// Home, the first of its home's people being in
	GoHome,
	/// Nothing yet
	Wait,
};

/// What a child at its town's crèche does: by day, while the crèche works, it strolls; otherwise a child with a home
/// goes home once the first of its home's people is in, and one without strolls on while touching the crèche
[[nodiscard]] constexpr AtCreche AtTheCreche(bool daytime, bool crecheWorks, bool hasHome, bool firstOfHomeIsIn,
                                             bool touchingCreche)
{
	if (daytime && crecheWorks)
	{
		return AtCreche::Stroll;
	}
	if (hasHome)
	{
		return firstOfHomeIsIn ? AtCreche::GoHome : AtCreche::Wait;
	}
	return touchingCreche ? AtCreche::StrollQuietly : AtCreche::Wait;
}

/// Whether a building of this kind takes over as a town's crèche when the crèche goes. As in the original game this is
/// any civic building, not only another crèche: the game tests the crèche's kind with a mask its kind shares with
/// every civic building but the wonder.
[[nodiscard]] constexpr bool TakesOverAsCreche(AbodeType type)
{
	constexpr auto k_CrecheMask = static_cast<uint32_t>(AbodeType::Creche) & 0xFFU;
	return (static_cast<uint32_t>(type) & k_CrecheMask) != 0;
}

/// One of a town's other buildings, in the town's order (the newest first), as a crèche to take over from one going
struct OtherBuilding
{
	bool working {false};
	bool takesOver {false};
};

/// The town's crèche once one of its crèches goes: when it was the town's own, the first of its other working buildings
/// that takes over as one does so; otherwise the town keeps the crèche it has. The index of the one taking over, none
/// when the town is left without.
[[nodiscard]] std::optional<size_t> CrecheAfterLoss(bool lostWasTheTowns, std::span<const OtherBuilding> others);

} // namespace openblack::creche
