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
#include <optional>
#include <string_view>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

/// What goes on with the villagers around a player's temple, gathered for the temple villagers' debug window and the
/// inspector: the temple's worship sites and the places at them its worshippers go to, the player's towns that worship
/// there, the villagers worshipping, on their way or simply near, what each is doing and where it is going, and the
/// worshippers walking about inside the temple while the player is in it. It only reads the game.
namespace openblack::temple_villagers
{

/// How a villager stands with the temple
enum class Standing : uint8_t
{
	/// Counted at a worship site of the temple: dancing, hiding in it, eating or resting
	Worshipping,
	/// On its way to one of the temple's sites
	OnTheWay,
	/// Near the temple, there for something else
	Nearby,
};
[[nodiscard]] std::string_view Name(Standing standing);

/// How a villager stands with the temple: counted at a site, on its way to one, or within the radius of it; none when it
/// is neither
[[nodiscard]] std::optional<Standing> StandingOf(bool countedAtSite, bool onTheWay, float distance, float radius);
/// The order the window lists them in: worshippers first, then those on the way, then those nearby, each nearest the
/// temple first, and by entity last so that the list doesn't shuffle
[[nodiscard]] bool ListedBefore(Standing a, float distanceA, entt::entity entityA, Standing b, float distanceB,
                                entt::entity entityB);

/// The places at a worship site its worshippers go to, in the order the window shows them
enum class SitePlace : uint8_t
{
	Gate,
	Rest,
	Altar,
	Door,
	Centre,

	_Count
};
[[nodiscard]] std::string_view Name(SitePlace place);

struct Site
{
	entt::entity site {entt::null};
	Tribe tribe {Tribe::NORSE};
	/// Its place round the temple, 0 to 5
	uint8_t place {0};
	bool built {false};
	uint32_t dancers {0};
	uint32_t foodInPot {0};
	/// Its worshippers, newest first
	std::vector<entt::entity> worshippers;
	std::vector<entt::entity> towns;
	/// Where each of its places is on the land, by SitePlace
	std::array<std::optional<glm::vec2>, static_cast<size_t>(SitePlace::_Count)> places {};
};

struct Town
{
	entt::entity town {entt::null};
	uint32_t id {0};
	entt::entity site {entt::null};
	/// The share of its people its totem wants at worship, 0 to 1
	float share {0.0f};
	uint32_t population {0};
	uint32_t worshipping {0};
	uint32_t onTheWay {0};
	/// How many more it wants at worship, as the game counts them when its share is set (fewer than none: too many)
	int32_t needed {0};
	/// Where new villagers are put down for it: by its first building, else its gathering place
	std::optional<glm::vec3> home;
};

/// A villager's walk: where it is going, the step it is making towards it, and the circle it is going round
struct Walk
{
	glm::vec2 goal {0.0f};
	std::optional<glm::vec2> stepGoal;
	/// Its move state's name, as the walk code names them: linear, orbit and so on
	std::string_view moveState;
	std::optional<glm::vec2> circleCentre;
	float circleRadius {0.0f};
	/// Metres a second
	float speed {0.0f};
};

struct Villager
{
	entt::entity villager {entt::null};
	Standing standing {Standing::Nearby};
	Tribe tribe {Tribe::NORSE};
	VillagerNumber number {VillagerNumber::Housewife};
	bool child {false};
	bool male {false};
	uint32_t age {0};
	float life {1.0f};
	entt::entity town {entt::null};
	/// Its town's worship site
	entt::entity site {entt::null};
	VillagerStates top {VillagerStates::InvalidState};
	VillagerStates final {VillagerStates::InvalidState};
	uint16_t turnsInState {0};
	glm::vec3 position {0.0f};
	/// From the temple, across the land
	float distance {0.0f};
	std::optional<Walk> walk;
};

/// One of the worshippers walking about inside the temple, placed in the world
struct InsideWalker
{
	entt::entity villager {entt::null};
	glm::vec3 position {0.0f};
	glm::vec3 target {0.0f};
	bool walking {false};
	/// Seconds since it last started or stopped walking, and how long it stands for when it stops
	float timer {0.0f};
	float pause {0.0f};
};

struct Inside
{
	/// The player is in the temple
	bool active {false};
	std::vector<InsideWalker> walkers;
	/// The edges of the floors they walk, in pairs of points, in the world
	std::vector<glm::vec3> floorEdges;
};

struct Survey
{
	entt::entity temple {entt::null};
	glm::vec3 templePosition {0.0f};
	std::vector<Site> sites;
	std::vector<Town> towns;
	std::vector<Villager> villagers;
	Inside inside;
};

/// The player's temple and the villagers around it, those near counted within the radius of its middle, in metres.
/// Without the temple it holds nothing. The floor's edges are gathered only when asked, as there are many.
[[nodiscard]] Survey Take(PlayerNames player, float radius, bool floorEdges);

} // namespace openblack::temple_villagers
