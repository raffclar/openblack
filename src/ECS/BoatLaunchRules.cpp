/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BoatLaunchRules.h"

#include <cmath>

#include <algorithm>

#include "Common/ModelInverseSquareRoot.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
/// The quarter turn the boat's clips are drawn with, and the eighth its sailing is turned by, as the game holds them
constexpr double k_QuarterTurn = 1.5707963705062866;
constexpr double k_EighthTurn = 0.7853981852531433;

/// Turns a matrix's axes about the vertical before everything else it does: its first axis takes in its third, and its
/// third gives up its first, as much as the angle's sine
void TurnAxes(glm::mat4& m, double angle)
{
	const double c = std::cos(angle);
	const double s = std::sin(angle);
	for (glm::length_t row = 0; row < 3; ++row)
	{
		const double first = m[0][row];
		const double third = m[2][row];
		m[0][row] = static_cast<float>((c * first) + (s * third));
		m[2][row] = static_cast<float>((c * third) - (s * first));
	}
}

/// The boat's model is mirrored across its length, which the mirrored clips turn the right way round again
void Mirror(glm::mat4& m)
{
	m[0] = glm::vec4(-glm::vec3(m[0]), m[0].w);
}

/// A turn about the vertical, the first axis towards +z by the angle's sine
glm::mat4 TurnedBy(double angle)
{
	const auto c = static_cast<float>(std::cos(angle));
	const auto s = static_cast<float>(std::sin(angle));
	glm::mat4 m(1.0f);
	m[0] = glm::vec4(c, 0.0f, s, 0.0f);
	m[2] = glm::vec4(-s, 0.0f, c, 0.0f);
	return m;
}

glm::mat4 Placed(const glm::mat4& turn, const glm::vec3& place)
{
	auto m = turn;
	m[3] = glm::vec4(place, 1.0f);
	return m;
}

/// The sounds of the launch, with the times of the clip they play at
struct LaunchSound
{
	int32_t time;
	int32_t sample;
};
constexpr std::array<LaunchSound, 3> k_LaunchSounds {{{100, 0x3E}, {1500, 0x3D}, {3900, 0x3C}}};

/// The brown sand flies while the boat runs down the beach, after this and up to the next; the white water after that
constexpr int32_t k_SandFrom = 1130;
constexpr int32_t k_WaterFrom = 3900;
constexpr int32_t k_WaterUntil = 6500;
constexpr uint32_t k_SandColour = 0xB88C38u;
constexpr uint32_t k_WaterColour = 0xFFFFFFu;

/// The sailors on the beach: how far each stands from the dry dock towards the sea and along the beach, and how far
/// into its clip it plays
constexpr std::array<float, boat_launch::k_LaunchSailors> k_SailorAcross {5.2f, 5.3f, 5.5f, 5.0f, 5.0f};
constexpr std::array<float, boat_launch::k_LaunchSailors> k_SailorAlong {1.0f, -0.7f, 0.0f, 0.4f, -0.2f};
constexpr std::array<int32_t, boat_launch::k_LaunchSailors> k_SailorClipOffset {5, 500, 1500, 455, 2000};
/// They stand three metres apart along the beach, the middle of them ten metres from the dry dock
constexpr float k_SailorSpacing = 3.0f;
constexpr float k_SailorsMiddle = 2.5f;
constexpr float k_SailorsAlongFromDock = 10.0f;
/// Worn out, cheering, worn out, in turn, once the boat is away
constexpr std::array<AnimId, 3> k_AfterPushing {AnimId::POverworked1, AnimId::PCrowdWon_2, AnimId::POverworked2};

/// The wake: each sprite runs back from fifteen metres behind the bow over a hundred, growing from ten metres across to
/// forty, lying on the sea
constexpr float k_WakeLifeScale = 1.0f / 6000.0f;
constexpr float k_WakeRun = 100.0f;
constexpr float k_WakeStart = 15.0f;
constexpr float k_WakeGrowth = 30.0f;
constexpr float k_WakeSmallest = 10.0f;
constexpr float k_WakeHeight = 0.2f;
constexpr float k_WakeAspect = 0.5f;
constexpr float k_WakeAngle = std::bit_cast<float>(0x407B53D2u);
constexpr uint8_t k_WakeCell = 0x31;
/// A sprite shows only once it is a fifth of the way through its life
constexpr double k_WakeShowsFrom = 0.2;
} // namespace

boat_launch::Step boat_launch::Advance(Clocks& clocks, int32_t milliseconds, const ClipLengths& lengths)
{
	const auto stepWakes = [&clocks, milliseconds] {
		for (auto& wake : clocks.wakes)
		{
			wake = (wake + milliseconds) % k_WakeLife;
		}
	};
	if (clocks.stage == Stage::Launch)
	{
		clocks.elapsed += milliseconds;
		clocks.previousBoatTime = clocks.boatTime;
		// The boat waits for the sailors' first push
		if (!clocks.pushing || clocks.elapsed > k_PushFor)
		{
			clocks.boatTime += milliseconds;
		}
		if (clocks.boatTime > lengths.launch - k_LaunchEndsEarly)
		{
			// The sailing starts afresh, its wake already moving this frame
			clocks = Clocks {.stage = Stage::Sailing};
			stepWakes();
			return Step::SailingBegan;
		}
		clocks.sinceSpray += milliseconds;
		// Once the boat is away the sailors stop pushing, and their clocks start again
		if (clocks.boatTime > k_PushEnds)
		{
			if (clocks.pushing)
			{
				clocks.elapsed = 0;
			}
			clocks.pushing = false;
		}
		return Step::Continues;
	}
	clocks.boatTime = lengths.sailingLoops ? (clocks.boatTime + milliseconds) % std::max(lengths.sailing, 1)
	                                       : std::min(clocks.boatTime + milliseconds, lengths.sailing - 1);
	clocks.elapsed += milliseconds;
	if (clocks.elapsed > k_SailingLasts)
	{
		return Step::Ended;
	}
	stepWakes();
	return Step::Continues;
}

bool boat_launch::TakeSpray(Clocks& clocks)
{
	if (clocks.stage != Stage::Launch || clocks.sinceSpray <= k_SprayEvery)
	{
		return false;
	}
	clocks.sinceSpray = 0;
	return true;
}

std::vector<int32_t> boat_launch::SoundsPassed(int32_t before, int32_t now)
{
	std::vector<int32_t> samples;
	for (const auto& sound : k_LaunchSounds)
	{
		if (before < sound.time && sound.time < now)
		{
			samples.push_back(sound.sample);
		}
	}
	return samples;
}

std::optional<boat_launch::Spray> boat_launch::SprayAt(int32_t boatTime)
{
	if (boatTime <= k_WaterFrom)
	{
		if (boatTime > k_SandFrom)
		{
			return Spray {.size = 5.0f, .colour = k_SandColour, .offset = glm::vec3(0.0f)};
		}
		return std::nullopt;
	}
	if (boatTime < k_WaterUntil)
	{
		return Spray {.size = 7.0f, .colour = k_WaterColour, .offset = glm::vec3(-10.0f, 7.0f, 0.0f)};
	}
	return std::nullopt;
}

glm::mat4 boat_launch::RootBetween(const glm::mat4& from, const glm::mat4& to, float t)
{
	return ((to - from) * t) + from;
}

glm::vec3 boat_launch::LaunchPlace(const glm::mat4& root, const glm::vec3& dock)
{
	return glm::vec3(Placed(glm::mat4(1.0f), dock) * root[3]);
}

glm::mat4 boat_launch::LaunchMatrix(const glm::mat4& root, const glm::vec3& dock, float liftedBy)
{
	auto m = Placed(glm::mat4(1.0f), dock) * root;
	TurnAxes(m, k_QuarterTurn);
	Mirror(m);
	m[3].y += liftedBy;
	return m;
}

glm::mat4 boat_launch::SailingMatrix(const glm::mat4& root, const glm::vec3& start, int32_t elapsed)
{
	// Carried to the south west, as much along x as along z
	const float way = gutils::ModelInverseSquareRoot(2.0f) * -1.0f;
	const float travelled = static_cast<float>(elapsed) * k_SailingSpeed;
	const glm::vec3 place {(way * travelled) + start.x, start.y + (travelled * 0.0f), start.z + (way * travelled)};
	auto m = Placed(TurnedBy(k_EighthTurn), place) * root;
	TurnAxes(m, k_QuarterTurn);
	Mirror(m);
	return m;
}

boat_launch::Sailor boat_launch::LaunchSailor(size_t index, const Clocks& clocks, const glm::vec3& dock)
{
	const float along = ((static_cast<float>(index) - k_SailorsMiddle) * k_SailorSpacing) + k_SailorAlong.at(index);
	return {
	    .xz = {dock.x + k_SailorAcross.at(index), along + dock.z + k_SailorsAlongFromDock},
	    .heading = std::bit_cast<float>(0xBFC90FDBu),
	    .clip = clocks.boatTime > k_PushEnds ? k_AfterPushing.at(index % k_AfterPushing.size()) : AnimId::PPushObject,
	    .clipTime = k_SailorClipOffset.at(index) + clocks.elapsed,
	};
}

const std::array<boat_launch::DeckPart, 8>& boat_launch::DeckParts()
{
	static const std::array<DeckPart, 8> k_Parts = [] {
		constexpr double k_HalfTurn = 3.1415927410125732;
		const auto at = [](glm::mat4 m, const glm::vec3& place) { return Placed(m, place); };
		const auto scaled = [](float scale) {
			auto m = glm::mat4(scale);
			m[3][3] = 1.0f;
			return m;
		};
		const auto facingAft = TurnedBy(k_HalfTurn);
		return std::array<DeckPart, 8> {{
		    // Two cows eating, between the masts
		    {.mesh = MeshId::AnimalCow1,
		     .clip = AnimId::ACowEat_2,
		     .local = at(TurnedBy(-1.0), {-1.778f, 10.78f, 1.83f}),
		     .clipOffset = 0},
		    {.mesh = MeshId::AnimalCow1,
		     .clip = AnimId::ACowEat_2,
		     .local = at(TurnedBy(static_cast<double>(-0.7f)), {-1.778f, 10.78f, 3.83f}),
		     .clipOffset = 1255},
		    // A pile of the grain they were given
		    {.mesh = MeshId::SpellGrainPile,
		     .clip = std::nullopt,
		     .local = at(scaled(0.26f), {-5.708f, 10.854f, 3.199f}),
		     .clipOffset = 0},
		    // A woman and a sailor at the bow, arms out
		    {.mesh = MeshId::PersonNorseFemaleA1,
		     .clip = AnimId::PTitanic,
		     .local = at(glm::mat4(1.0f), {-0.14f, 13.213f, -19.657f}),
		     .clipOffset = 0},
		    {.mesh = MeshId::PersonNorseSailor,
		     .clip = AnimId::PTitanic,
		     .local = at(glm::mat4(1.0f), {-0.14f, 13.213f, -18.9f}),
		     .clipOffset = 500},
		    // Two sailors sitting on the side, swinging their legs
		    {.mesh = MeshId::PersonNorseSailor,
		     .clip = AnimId::PSittingSwingingLegs,
		     .local = at(facingAft, {-6.25f, 11.424f, -7.227f}),
		     .clipOffset = 0},
		    {.mesh = MeshId::PersonNorseSailor,
		     .clip = AnimId::PSittingSwingingLegs,
		     .local = at(facingAft, {-5.25f, 11.424f, -7.227f}),
		     .clipOffset = 2345},
		    // One scrubbing the deck
		    {.mesh = MeshId::PersonNorseSailor,
		     .clip = AnimId::PScrubbs,
		     .local = at(facingAft, {5.881f, 10.741f, 0.174f}),
		     .clipOffset = 0},
		}};
	}();
	return k_Parts;
}

glm::mat4 boat_launch::OnDeck(const DeckPart& part, const glm::mat4& boat)
{
	return boat * part.local;
}

boat_launch::Wake boat_launch::WakeAt(int32_t phase, const glm::mat4& boat)
{
	const float life = static_cast<float>(phase) * k_WakeLifeScale;
	auto position = glm::vec3(boat * glm::vec4(0.0f, 0.0f, (life * k_WakeRun) - k_WakeStart, 1.0f));
	position.y = k_WakeHeight;
	return {
	    .position = position,
	    .halfWidth = (life * k_WakeGrowth) + k_WakeSmallest,
	    .aspect = k_WakeAspect,
	    .angle = k_WakeAngle,
	    .alpha = static_cast<uint8_t>(std::trunc((1.0f - life) * 255.0f)),
	    .cell = k_WakeCell,
	    .shown = static_cast<double>(life) > k_WakeShowsFrom,
	};
}
