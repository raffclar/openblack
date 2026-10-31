/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <string_view>
#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{

/// The lines of a land that give player one a temple to the south and a Celtic town to the north that offers the food
/// miracle, with a full storage pit and two abodes of people
constexpr std::string_view k_PlayerOneTownScript = R"(CREATE_CITADEL("0.0,-90.0", 0, "PLAYER_ONE", 5690, 1000)
CREATE_TOWN(900, "0.0,40.0", "PLAYER_ONE", 0, "CELTIC")
SET_TOWN_BELIEF_CAP(900, "PLAYER_ONE", 2.0)
CREATE_NEW_TOWN_SPELL(900, "FOOD")
SET_TOWN_BELIEF(900, "PLAYER_ONE", 1.0)
CREATE_TOWN_CENTRE(900, "-4.0,41.0", "CELTIC_ABODE_TOWN_CENTRE", 0, 1000, 0)
CREATE_ABODE(900, "10.0,90.0", "CELTIC_ABODE_STORAGE_PIT", 6282, 1000, 3000, 3000)
CREATE_ABODE(900, "40.0,60.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("36.0,62.0", "40.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("36.0,62.0", "40.0,60.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("36.0,62.0", "40.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("36.0,62.0", "40.0,60.0", "CELTIC_FORESTER", 20)
CREATE_ABODE(900, "-40.0,60.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("-44.0,62.0", "-40.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("-44.0,62.0", "-40.0,60.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("-44.0,62.0", "-40.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("-44.0,62.0", "-40.0,60.0", "CELTIC_FORESTER", 20)
)";

/// The same land, with a town that offers the water miracle
constexpr std::string_view k_WaterTownScript = R"(CREATE_CITADEL("0.0,-90.0", 0, "PLAYER_ONE", 5690, 1000)
CREATE_TOWN(900, "0.0,40.0", "PLAYER_ONE", 0, "CELTIC")
SET_TOWN_BELIEF_CAP(900, "PLAYER_ONE", 2.0)
CREATE_NEW_TOWN_SPELL(900, "WATER")
SET_TOWN_BELIEF(900, "PLAYER_ONE", 1.0)
CREATE_TOWN_CENTRE(900, "-4.0,41.0", "CELTIC_ABODE_TOWN_CENTRE", 0, 1000, 0)
CREATE_ABODE(900, "10.0,90.0", "CELTIC_ABODE_STORAGE_PIT", 6282, 1000, 3000, 3000)
CREATE_ABODE(900, "40.0,60.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("36.0,62.0", "40.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("36.0,62.0", "40.0,60.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("36.0,62.0", "40.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("36.0,62.0", "40.0,60.0", "CELTIC_FORESTER", 20)
CREATE_ABODE(900, "-40.0,60.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("-44.0,62.0", "-40.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("-44.0,62.0", "-40.0,60.0", "CELTIC_FORESTER", 20)
CREATE_VILLAGER_POS("-44.0,62.0", "-40.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("-44.0,62.0", "-40.0,60.0", "CELTIC_FORESTER", 20)
)";

/// The temple, its worship site and the near side of the town
Framing SiteFraming()
{
	return {.shot = Shot::Overview, .include = {glm::vec2(-50.0f, -130.0f), glm::vec2(50.0f, -20.0f)}};
}

} // namespace

void testbed_scenarios::AddWorshipSiteScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "worship.site_ring_and_flags",
	    .name = "The ring of light, the need flags and the chant at a worship site",
	    .facet = Facet::Miracles,
	    .description = "Lines of a land give player one a temple to the south and a Celtic town to the north. All of the "
	                   "people go to worship at the site, which stands built. The camera looks at the site from near.",
	    .expected = "A ring of light in red stands round the dance of the site. It rises as the store of prayer power "
	                "of the site fills. Three flags stand by the site: food, rest and hiding. Each flag shows only "
	                "when its need is more than its least need, and it rises with the need. The chant of the Celtic "
	                "worshippers plays from the dance while the camera is near the site.",
	    .framing = SiteFraming(),
	    .landScript = k_PlayerOneTownScript,
	    .worshipShare = 1.0f,
	});
	all.push_back({
	    .id = "worship.site_motes",
	    .name = "The motes of prayer power from a worship site to its miracle",
	    .facet = Facet::Miracles,
	    .description = "The land of the ring scenario. All of the people go to worship at the site, which stands built. "
	                   "The town offers the water miracle. After one minute the site gives a water miracle to the "
	                   "hand, and the hand pours it for 8 seconds between the site and the town. It pours again each "
	                   "20 seconds.",
	    .expected = "Each time the site spends 100 prayer power on the water miracle, a mote in the colour of player "
	                "one flies from the water icon of the site to the miracle. While the site strains, the ring and the motes "
	                "throb dark and back.",
	    .framing = SiteFraming(),
	    .miracles = {{.type = MagicType::Water,
	                  .point = {0.0f, -40.0f},
	                  .handOffset = {0.0f, -40.0f},
	                  .delaySeconds = 60.0f,
	                  .holdSeconds = 8.0f,
	                  .repeatSeconds = 20.0f,
	                  .byHand = true,
	                  .fromWorship = true,
	                  .fromSite = true}},
	    .landScript = k_WaterTownScript,
	    .worshipShare = 1.0f,
	});
	all.push_back({
	    .id = "worship.leash_feeds_site",
	    .name = "A creature tied to the food flag of a worship site feeds the site",
	    .facet = Facet::Leash,
	    .description = "The land of the ring scenario. All of the people go to worship at the site, which stands built. "
	                   "A tiger that knows the food miracle stands east of the temple. The tiger gets the rope leash, "
	                   "then the hand ties the leash to the food flag of the site.",
	    .expected = "First the tiger does the order on the flag, as for a tie to any thing: it walks to the flag and "
	                "looks at it. When that is done, it walks to the altar of the site to cast the food miracle there. "
	                "While the tie stays and the food flag is more than six tenths of its full height, the tiger "
	                "tries to feed the site again each time it completes its work. The inspector shows the plan "
	                "CastMagicFoodByWorshipSite on the altar.",
	    .framing = SiteFraming(),
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .offset = {40.0f, -90.0f}, .facingDegrees = 270.0f}},
	    .landScript = k_PlayerOneTownScript,
	    .commands = {{.kind = Command::Kind::KnowMiracle, .delaySeconds = 0.5f, .value = static_cast<size_t>(MagicType::Food)},
	                 {.kind = Command::Kind::PutOnLeash, .creature = 0, .delaySeconds = 1.0f, .leash = LeashType::Rope},
	                 {.kind = Command::Kind::TieLeashToWorshipSiteFlag, .creature = 0, .delaySeconds = 1.0f, .value = 0}},
	    .worshipShare = 1.0f,
	});
	all.push_back({
	    .id = "worship.site_takes_food",
	    .name = "Food from the food miracle goes into the store of a worship site",
	    .facet = Facet::Miracles,
	    .description = "The land of the ring scenario. All of the people go to worship at the site, which stands built. "
	                   "A tiger stands east of the site and looks at it. After 20 seconds the hand pours the food "
	                   "miracle on the food pot of the site for 5 seconds.",
	    .expected = "The food of the miracle falls into the site, and the food in the store of the site goes up to "
	                "about 1000. No new pile of food stays on the land by the site. The inspector shows the amount of "
	                "the food pot of the site.",
	    .framing = SiteFraming(),
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .offset = {40.0f, -50.0f}, .facingDegrees = 270.0f}},
	    .miracles = {{.type = MagicType::Food,
	                  .point = {-4.0f, -51.0f},
	                  .handOffset = {-4.0f, -51.0f},
	                  .delaySeconds = 20.0f,
	                  .holdSeconds = 5.0f,
	                  .byHand = true}},
	    .landScript = k_PlayerOneTownScript,
	    .worshipShare = 1.0f,
	});
}
