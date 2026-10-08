/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The influence circles and the hand's crossing of them: the circles Update3DInfluence rebuilds every 10 turns when a
// radius moved (one per citadel and per town with influence), their overlaps, the alpha their draw gives them, and the
// ripple plus the sound a hand that crosses one makes. The drawing is the Renderer's
// (Graphics/RendererInfluence.cpp).

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <array>
#include <span>
#include <vector>

#include "3D/LandMorph.h"
#include "3D/ObjectMatrix.h"
#include "Audio/Audio.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Influence.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandMagicStateInterface.h"
#include "GameClock.h"
#include "Influence.h"
#include "InfluenceState.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
/// The eight players of the original (the per-player state is arrays of 8)
constexpr size_t k_Players = 8;
/// NoteInfluence's threshold (strict). (inferred) a float
constexpr float k_RebuildThreshold = 0.01f;
/// Update3DInfluence runs on the turns that are a multiple of 10
constexpr uint32_t k_RebuildTurns = 10;
/// The curtain's draw: the camera gate, the height of the full alpha, the ramp's 0.01 and 120, the full alpha 0x78
constexpr float k_CameraGate = 100.0f;
constexpr float k_CameraFull = 200.0f;
constexpr float k_RampScale = 0.01f;
constexpr float k_RampAlpha = 120.0f;
constexpr uint8_t k_FullAlpha = 0x78;
/// The hidden closing column's colour, white with alpha 0
constexpr uint32_t k_HiddenClosingColour = 0x00FFFFFFu;
/// The ripple's life and the smallest sprite size
constexpr int32_t k_RippleLifeMs = 2000;
constexpr float k_RippleMinSize = 0.0001f;
/// The ripple's draw: the fade's 0.001 under 1000 ms, the growth's 0.001 x 10, the wrap 14 and 1 / 14 (as a float),
/// the alpha's 255
constexpr int32_t k_RippleFadeMs = 1000;
constexpr float k_RippleFadeRate = 0.001f;
constexpr float k_Milli = 0.001f;
constexpr float k_RippleGrowth = 10.0f;
constexpr float k_RippleWrap = 14.0f;
constexpr float k_RippleInverseWrap = 0.0714285746f;
constexpr float k_RippleAlpha = 255.0f;
/// CrossingPoint: the stop |b - a|^2 <= 1 and the half
constexpr float k_BisectStop = 1.0f;
constexpr float k_Half = 0.5f;
/// pi / 2: the ripple's yaw is the border's tangent
constexpr float k_HalfPi = 1.57079637f;
/// 2 pi as a float, the curtain's (also affine::GetYAngleBetween's). (inferred) the same constant as the bound of
/// the ripple sprites' Random(0, 2 pi): that value was not read
constexpr float k_TwoPi = 6.28318548f;

/// The hand was inside a circle of this player on the previous frame, and that array is filled in. Statics of the
/// process in the original too: clearing the map only empties the circle list, nothing clears these.
struct InfluenceCrossingState
{
	std::array<bool, k_Players> wasInside {};
	bool haveState {false};
	/// The hand's point of the previous call (x and z; y is written 0), which every call overwrites with the current
	/// one, the first included
	glm::vec3 previousPoint {0.0f};
};

/// The hand's crossing state (Locator::handMagicState)
InfluenceCrossingState& Crossing()
{
	if (!Locator::handMagicState::has_value())
	{
		std::fputs("ecs::influence_circles: no hand magic state in the locator (Locator::handMagicState)\n", stderr);
		std::abort();
	}
	return Locator::handMagicState::value().Get<InfluenceCrossingState>();
}

/// The point is inside the circle when dx * dx + dz * dz < r * r (strictly)
bool Inside(const glm::vec3& centre, float radius, const glm::vec3& point)
{
	const float dx = point.x - centre.x;
	const float dz = point.z - centre.z;
	return dx * dx + dz * dz < radius * radius;
}

/// Every column of `circle` inside `other` is hidden. Columns 0..N - 1 keep their RGB with alpha 0, the closing
/// column N becomes white with alpha 0, so its triangles fade towards white: a quirk of the original, kept. (inferred)
/// the column's point is the curtain's ground vertex (cos r + cx, sin r + cz; r + cx, cz for N), which the original
/// computes again the same way
void HideContact(influence::Circle& circle, const influence::Circle& other)
{
	const size_t segments = circle.Segments();
	for (size_t i = 0; i <= segments; ++i)
	{
		if (!Inside(other.centre, other.radius, circle.curtain.positions.at(3 * i)))
		{
			continue;
		}
		for (size_t k = 0; k < 3; ++k)
		{
			auto& colour = circle.curtain.colours.at(3 * i + k);
			colour = i < segments ? colour & 0x00FFFFFFu : k_HiddenClosingColour;
		}
		circle.hidden.at(i) = 1;
	}
}

/// The overlap with one older circle of the list (newest first): nothing when either is dead or their players differ;
/// d is the 3D distance of the centres; d + r < r_older kills this one, else d + r_older < r kills the older one, else
/// each hides its columns inside the other, this one first. A circle killed later in the same construction may already
/// have hidden columns of others (kept)
void Overlap(influence::Circle& circle, influence::Circle& older)
{
	if (circle.dead || older.dead || circle.player != older.player)
	{
		return;
	}
	const glm::vec3 d = circle.centre - older.centre;
	const float distance = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
	if (distance + circle.radius < older.radius)
	{
		circle.dead = true;
	}
	else if (distance + older.radius < circle.radius)
	{
		older.dead = true;
	}
	else
	{
		HideContact(circle, older);
		HideContact(older, circle);
	}
}

/// A new circle (the curtain, the head of the list, the overlaps with the older circles), then every dead circle
/// deleted
void Add(std::vector<influence::Circle>& circles, PlayerNames player, const glm::vec3& centre, float radius)
{
	// the colour index & 7; every vertex takes the player colour of it, alpha 0
	const auto index = static_cast<size_t>(player) & 7u;
	influence::Circle circle {
	    static_cast<PlayerNames>(index),
	    centre,
	    radius,
	    land_morph::InfluenceCurtain(land_morph::CurrentAltitude(), centre, radius, influence::k_CircleColours.at(index)),
	    {}};
	// the hidden flags: (3N + 3) allocated, N + 1 used
	circle.hidden.assign(circle.Segments() + 1, 0);
	circles.insert(circles.begin(), std::move(circle));
	for (size_t i = 1; i < circles.size(); ++i)
	{
		Overlap(circles.front(), circles.at(i));
	}
	std::erase_if(circles, [](const influence::Circle& c) { return c.dead; });
}

/// The first circle of the player that has one of the two points inside and
/// the other outside, or none
const influence::Circle* CrossedCircle(std::span<const influence::Circle> circles, const glm::vec3& previous,
                                       const glm::vec3& current, size_t player)
{
	for (const auto& circle : circles)
	{
		if (static_cast<size_t>(circle.player) == player &&
		    Inside(circle.centre, circle.radius, previous) != Inside(circle.centre, circle.radius, current))
		{
			return &circle;
		}
	}
	return nullptr;
}

/// A new ripple: life 2000 ms, the colour, 7 sprites at the point with the smoke material (materials::k_Smoke) and
/// cell 63, size max(2 i, 0.0001) (the origin is 0, so its rescale keeps it 0), angle Random(0, 2 pi), flag 1; the
/// matrix is RotX(pi / 2) then the yaw. (approximate) its rows are written as exactly (c, 0, s), (s, 0, -c), (0, 1, 0):
/// the original's cos(pi / 2) leaves a residue of about 4e-8 in them
influence::Ripple MakeRipple(const glm::vec3& point, uint32_t colour, float yaw)
{
	influence::Ripple ripple {
	    .point = point,
	    .life = k_RippleLifeMs,
	    .colour = colour,
	};
	const float c = std::cos(yaw);
	const float s = std::sin(yaw);
	ripple.matrix[0] = glm::vec4(c, 0.0f, s, 0.0f);
	ripple.matrix[1] = glm::vec4(s, 0.0f, -c, 0.0f);
	ripple.matrix[2] = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
	ripple.matrix[3] = glm::vec4(point, 1.0f);
	for (size_t i = 0; i < influence::Ripple::k_Sprites; ++i)
	{
		const float size = 2.0f * static_cast<float>(i);
		ripple.sizes.at(i) = size > k_RippleMinSize ? size : k_RippleMinSize;
		ripple.angles.at(i) = game_random::crt::Random(0.0f, k_TwoPi);
		ripple.flags.at(i) = 1;
	}
	return ripple;
}

/// The Y angle from centre to point: atan2(dz, dx), + 2 pi when negative
float YAngle(const glm::vec3& from, const glm::vec3& to)
{
	float angle = std::atan2(to.z - from.z, to.x - from.x);
	if (angle < 0.0f)
	{
		angle += k_TwoPi;
	}
	return angle;
}

/// The ripple of a crossing of `circle`: the inside point is the previous one when the circle contains it, else the
/// current one, both at y = 0; CrossingPoint finds the edge between them; y = max(GetAltitude(point), hand.y);
/// yaw = YAngle(centre, point) + pi / 2
influence::Ripple CrossingRipple(const influence::Circle& circle, const glm::vec3& previous, const glm::vec3& handPosition)
{
	const glm::vec3 current(handPosition.x, 0.0f, handPosition.z);
	const bool previousInside = Inside(circle.centre, circle.radius, previous);
	auto point = influence::detail::CrossingPoint(circle.centre, circle.radius, previousInside ? previous : current,
	                                              previousInside ? current : previous);
	const float altitude = land_morph::CurrentAltitude()(glm::vec2(point.x, point.z));
	point.y = altitude > handPosition.y ? altitude : handPosition.y;
	// the angle and pi / 2 added in double, rounded once to float
	const auto yaw = static_cast<float>(affine::GetYAngleBetween(circle.centre, point) + static_cast<double>(k_HalfPi));
	return MakeRipple(point, influence::k_CircleColours.at(static_cast<size_t>(circle.player) & 7u), yaw);
}
} // namespace

void influence::NoteInfluence(float now, float last)
{
	if (std::fabs(now - last) > k_RebuildThreshold)
	{
		detail::Globals().circlesDirty = true;
	}
}

void influence::ForceNeedUpdateInfluence()
{
	detail::Globals().circlesDirty = true;
}

void influence::Update3DInfluence()
{
	auto& globals = detail::Globals();
	// the dirty flag and GameTurn % 10 == 0
	if (!globals.circlesDirty || game_clock::Turn() % k_RebuildTurns != 0)
	{
		return;
	}
	// the circles are rebuilt from scratch
	globals.circles.clear();
	auto& registry = Locator::entitiesRegistry::value();
	// the first temple of each player, as CitadelInfluenceAt
	std::array<entt::entity, k_Players> citadels {};
	citadels.fill(entt::null);
	registry.Each<const Temple>([&citadels](entt::entity entity, const Temple& temple) {
		const auto index = static_cast<size_t>(temple.owner);
		if (index < citadels.size() && citadels.at(index) == entt::null)
		{
			citadels.at(index) = entity;
		}
	});
	// the player walk stops at the neutral player. (inferred) a player the land did not make has no citadel and no town,
	// so every slot before the neutral one is walked
	for (size_t p = 0; p < static_cast<size_t>(PlayerNames::NEUTRAL); ++p)
	{
		const auto player = static_cast<PlayerNames>(p);
		if (const auto citadel = citadels.at(p); citadel != entt::null)
		{
			const auto* transform = registry.TryGet<const Transform>(citadel);
			const float radius = CitadelRadius(citadel); // the citadel's influence
			if (radius != 0.0f && transform != nullptr)
			{
				Add(globals.circles, player, transform->position, radius);
			}
			if (auto* stored = registry.TryGet<CitadelInfluence>(citadel); stored != nullptr)
			{
				stored->drawnRadius = radius; // always
			}
		}
		// the player's town list (map_cells::TownsOf: the oldest first)
		for (const auto town : ecs::map_cells::TownsOf(player))
		{
			auto* townInfluence = registry.TryGet<TownInfluence>(town);
			const auto* transform = registry.TryGet<const Transform>(town);
			if (townInfluence == nullptr || transform == nullptr)
			{
				continue;
			}
			const float radius = townInfluence->radius;
			if (radius != 0.0f)
			{
				Add(globals.circles, player, transform->position, radius);
			}
			townInfluence->drawnRadius = radius; // always
		}
	}
	globals.circlesDirty = false;
}

std::span<const influence::Circle> influence::Circles()
{
	return detail::GlobalsOrDefault().circles;
}

bool influence::BoundaryShown(PlayerNames player)
{
	const auto index = static_cast<size_t>(player);
	const auto& shown = detail::GlobalsOrDefault().boundaryShown;
	return index < shown.size() && shown.at(index);
}

void influence::ShowBoundary(PlayerNames player)
{
	const auto index = static_cast<size_t>(player);
	auto& shown = detail::Globals().boundaryShown;
	if (index < shown.size())
	{
		shown.at(index) = true;
	}
}

std::optional<uint8_t> influence::CurtainAlpha(float cameraY)
{
	if (cameraY <= k_CameraGate)
	{
		return std::nullopt;
	}
	if (cameraY >= k_CameraFull)
	{
		return k_FullAlpha;
	}
	// truncated: 0..119
	return static_cast<uint8_t>(static_cast<int32_t>((cameraY - k_CameraGate) * k_RampScale * k_RampAlpha));
}

void influence::SetCurtainAlpha(uint8_t alpha)
{
	auto* globals = detail::TryGlobals();
	if (globals == nullptr)
	{
		return;
	}
	for (auto& circle : globals->circles)
	{
		// the alpha cache, then the player's boundary latch
		if (circle.alphaCache == alpha || !globals->boundaryShown.at(static_cast<size_t>(circle.player) & 7u))
		{
			continue;
		}
		circle.alphaCache = alpha;
		const size_t segments = circle.Segments();
		for (size_t j = 0; j <= segments; ++j)
		{
			if (circle.hidden.at(j) != 0)
			{
				continue;
			}
			// the middle vertex of the column, the second colour of the triple
			auto& colour = circle.curtain.colours.at(3 * j + 1);
			colour = (colour & 0x00FFFFFFu) | (static_cast<uint32_t>(alpha) << 24);
		}
	}
}

void influence::UpdateRipples(uint32_t gameTimeIncMs)
{
	auto* globals = detail::TryGlobals();
	if (globals == nullptr)
	{
		return;
	}
	// life -= the game time increment; a negative one deletes the ripple
	std::erase_if(globals->ripples, [gameTimeIncMs](Ripple& ripple) {
		ripple.life -= static_cast<int32_t>(gameTimeIncMs);
		return ripple.life < 0;
	});
}

std::span<const influence::Ripple> influence::Ripples()
{
	return detail::GlobalsOrDefault().ripples;
}

std::array<influence::RippleSprite, influence::Ripple::k_Sprites> influence::DrawRipple(size_t index, uint32_t gameTimeIncMs)
{
	std::array<RippleSprite, Ripple::k_Sprites> sprites {};
	auto* globals = detail::TryGlobals();
	if (globals == nullptr || index >= globals->ripples.size())
	{
		return sprites;
	}
	auto& ripple = globals->ripples.at(index);
	// fade = life < 1000 ? life x 0.001 : 1; g = game time increment x 0.001 x 10
	const float fade = ripple.life < k_RippleFadeMs ? static_cast<float>(ripple.life) * k_RippleFadeRate : 1.0f;
	const float growth = static_cast<float>(gameTimeIncMs) * k_Milli * k_RippleGrowth;
	for (size_t i = 0; i < Ripple::k_Sprites; ++i)
	{
		float size = ripple.sizes.at(i) + growth;
		if (size > k_RippleWrap)
		{
			ripple.flags.at(i) = 0;
			size -= k_RippleWrap * static_cast<float>(static_cast<int32_t>(size * k_RippleInverseWrap));
		}
		// the alpha byte of the ripple's colour. (inferred) s / 14 as a division: only the wrap's 1 / 14 was read
		const auto alpha =
		    static_cast<uint32_t>(static_cast<int32_t>((1.0f - size / k_RippleWrap) * fade * k_RippleAlpha)) & 0xFFu;
		ripple.colour = (ripple.colour & 0x00FFFFFFu) | (alpha << 24);
		ripple.sizes.at(i) = size > k_RippleMinSize ? size : k_RippleMinSize;
		sprites.at(i) = {ripple.sizes.at(i), ripple.angles.at(i), ripple.colour};
	}
	return sprites;
}

glm::vec3 influence::detail::CrossingPoint(const glm::vec3& centre, float radius, glm::vec3 inside, glm::vec3 outside)
{
	for (;;)
	{
		const float dx = outside.x - inside.x;
		const float dz = outside.z - inside.z;
		const glm::vec3 middle = (inside + outside) * k_Half;
		if (dx * dx + dz * dz <= k_BisectStop)
		{
			return middle;
		}
		(Inside(centre, radius, middle) ? inside : outside) = middle;
	}
}

bool influence::HandCrossedInfluence(const glm::vec3& handPosition)
{
	// nothing crossed yet, then the per-player "the hand is inside one of this player's circles" bits, over the list
	// the draw uses
	const auto circles = Circles();
	std::array<bool, k_Players> inside {};
	for (const auto& circle : circles)
	{
		if (Inside(circle.centre, circle.radius, handPosition))
		{
			inside[static_cast<size_t>(circle.player)] = true;
		}
	}
	bool crossed = false;
	if (!Crossing().haveState)
	{
		// the first call only remembers them
		Crossing().wasInside = inside;
		Crossing().haveState = true;
	}
	else
	{
		for (size_t player = 0; player < k_Players; ++player)
		{
			if (inside[player] == Crossing().wasInside[player])
			{
				continue; // unchanged
			}
			Crossing().wasInside[player] = inside[player]; // before the search
			// look for the player's circle whose edge the hand crossed between the previous point
			// and this one; none (a circle that appeared, grew or shrank under a still hand) makes no ripple and no sound
			const auto* circle = CrossedCircle(circles, Crossing().previousPoint, handPosition, player);
			if (circle == nullptr)
			{
				continue;
			}
			// only when that circle's player has its boundary shown, which the border's draw reads too
			if (!BoundaryShown(circle->player))
			{
				continue;
			}
			// the ripple at the head of the ripple list; then the crossing is reported
			auto& ripples = detail::Globals().ripples;
			ripples.insert(ripples.begin(), CrossingRipple(*circle, Crossing().previousPoint, handPosition));
			crossed = true;
		}
	}
	// the point is remembered on every call
	Crossing().previousPoint = glm::vec3(handPosition.x, 0.0f, handPosition.z);
	return crossed;
}

void influence::detail::ResetHandCrossing()
{
	Crossing().wasInside = {};
	Crossing().haveState = false;
	Crossing().previousPoint = glm::vec3(0.0f);
}

void influence::ProcessHandCrossing(const glm::vec3& handPosition)
{
	// the crossing, then the sound only when a ripple was made
	if (!HandCrossedInfluence(handPosition))
	{
		return;
	}
	// a sound effect from bank InGame, sample 52 G_HandThroughInfluence_01, no owner, 3D, not tracked, at the hand's
	// point; mode and loops stay the defaults (3 and 0) and the .sad overrides the mode with 1 (a new channel every
	// time) and gives volume 40 and min / max 100 / 300. Going in and coming out sound the same, and several players'
	// circles crossed at once still sound once.
	// (pending) the force feedback after it, not ported.
	audio::PlayOptions options;
	options.sample = {audio::Bank(audio::SfxBank::InGame), 52};
	options.owner = audio::Owner::None();
	options.is3D = true;
	options.track = false;
	options.position = handPosition;
	audio::PlaySoundEffect(options);
}
