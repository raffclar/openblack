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

/// The same land, with a town that offers the freeze, heal, storm and shield miracles
constexpr std::string_view k_HolderTownScript = R"(CREATE_CITADEL("0.0,-90.0", 0, "PLAYER_ONE", 5690, 1000)
CREATE_TOWN(900, "0.0,40.0", "PLAYER_ONE", 0, "CELTIC")
SET_TOWN_BELIEF_CAP(900, "PLAYER_ONE", 2.0)
CREATE_NEW_TOWN_SPELL(900, "CREATURE_SPELL_FREEZE")
CREATE_NEW_TOWN_SPELL(900, "HEAL")
CREATE_NEW_TOWN_SPELL(900, "STORM")
CREATE_NEW_TOWN_SPELL(900, "SHIELD")
SET_TOWN_BELIEF(900, "PLAYER_ONE", 1.0)
CREATE_TOWN_CENTRE(900, "-4.0,41.0", "CELTIC_ABODE_TOWN_CENTRE", 0, 1000, 0)
CREATE_ABODE(900, "10.0,90.0", "CELTIC_ABODE_STORAGE_PIT", 6282, 1000, 3000, 3000)
CREATE_ABODE(900, "40.0,60.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
CREATE_VILLAGER_POS("36.0,62.0", "40.0,60.0", "CELTIC_HOUSEWIFE", 20)
CREATE_VILLAGER_POS("36.0,62.0", "40.0,60.0", "CELTIC_FORESTER", 20)
CREATE_ABODE(900, "-40.0,60.0", "CELTIC_ABODE_F", 4328, 1095, 0, 0)
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
	    .id = "worship.way_passes_fire",
	    .name = "Villagers on their way to worship go round a fire and do not stop to fight it",
	    .facet = Facet::Miracles,
	    .description =
	        "The land of the ring scenario. All of the people go to worship at the site, which stands built. "
	        "Two oaks stand by the footpath from the village centre to the site. After twelve seconds, when the people "
	        "are on their way, both oaks catch fire.",
	    .expected = "A villager on its way to worship that comes near a burning oak goes round the fire and then on to "
	                "the site. No villager on its way stops to beat out the fire or to fetch water. The oaks burn down.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-40.0f, -10.0f), glm::vec2(0.0f, 45.0f)}},
	    .objects = {{.type = TreeInfo::Oak, .offset = {-26.0f, -5.0f}}, {.type = TreeInfo::Oak, .offset = {-28.0f, 22.0f}}},
	    .landScript = k_PlayerOneTownScript,
	    .commands = {{.kind = Command::Kind::SetAlight, .delaySeconds = 12.0f, .value = 1, .object = 0},
	                 {.kind = Command::Kind::SetAlight, .delaySeconds = 0.0f, .value = 1, .object = 1}},
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
	    .id = "worship.icon_holder_effects",
	    .name = "The effects of the seeds over the icons of a worship site",
	    .facet = Facet::Miracles,
	    .description = "The land of the ring scenario. The town offers the freeze, heal, storm and shield miracles. All of "
	                   "the people go to worship at the site, which stands built. The camera looks at the site.",
	    .expected = "Each icon of the site shows the effect of the holder of its seed above its palm, in the colours of "
	                "player one, as the stone hands of the village centre do. The storm seed has no model: only its "
	                "effect shows. The glints of the freeze effect sparkle on the points of the spinning phial, and they "
	                "follow the phial as it turns. The effects move only while the icons are on the screen.",
	    .framing = SiteFraming(),
	    .landScript = k_HolderTownScript,
	    .worshipShare = 1.0f,
	});
	all.push_back({
	    .id = "miracles.seed_take_back",
	    .name = "Shield: the hand takes its seed back",
	    .facet = Facet::Miracles,
	    .description = "The land of the icon scenario. All of the people go to worship at the site. After one minute the "
	                   "site gives a shield seed to the hand. The hand draws a circle of radius 25 north of the temple "
	                   "and casts the shield there. The seed then shows over the shield. At 95 seconds, the "
	                   "player's hand takes the seed back at once, as the debug tools take a thing. To do it as a player, "
	                   "press and hold the Action button with the hand over the seed before then.",
	    .expected = "At the take, the shield closes down and its sphere shrinks away. The seed goes into the hand as the "
	                "held miracle, ready at once, and the hand shows the name of the miracle. The seed holds the prayer "
	                "power that the shield had left. The help counts "
	                "event 13 (a seed taken back), then event 2 (a take), but not event 12.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(-40.0f, -70.0f), glm::vec2(40.0f, 10.0f)}},
	    .miracles = {{.type = MagicType::Shield,
	                  .point = {0.0f, -30.0f},
	                  .handOffset = {0.0f, -30.0f},
	                  .delaySeconds = 60.0f,
	                  .byHand = true,
	                  .fromWorship = true,
	                  .fromSite = true,
	                  .circleRadius = 25.0f,
	                  .holdBeforePress = 2.0f}},
	    .landScript = k_HolderTownScript,
	    .commands = {{.kind = Command::Kind::HandTakeShownSeed, .delaySeconds = 95.0f}},
	    .worshipShare = 1.0f,
	});
	all.push_back({
	    .id = "miracles.seed_take_back_to_dispenser",
	    .name = "Shield: the hand puts a seed that it took back into a dispenser",
	    .facet = Facet::Miracles,
	    .description = "As miracles.seed_take_back, with a miracle dispenser of the player's town east of the shield. The "
	                   "dispenser comes as a reward of a challenge, with no globe. At 95 seconds the hand takes the shield "
	                   "seed back. At 98 seconds the hand casts the seed on the dispenser, as the Action button over it "
	                   "does.",
	    .expected = "At the take, the shield closes down and the seed goes into the hand. At the cast on the dispenser, the "
	                "seed leaves the hand. A shield globe comes into view above the dispenser and floats there. The hand "
	                "took the seed back as a seed that did not cast, so the dispenser takes it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {glm::vec2(5.0f, -50.0f), glm::vec2(45.0f, -10.0f)}},
	    .dispensers = {{.type = MagicType::LightningBolt,
	                    .offset = {35.0f, -30.0f},
	                    .charged = false,
	                    .reward = true,
	                    .globeTaken = true}},
	    .miracles = {{.type = MagicType::Shield,
	                  .point = {0.0f, -30.0f},
	                  .handOffset = {0.0f, -30.0f},
	                  .delaySeconds = 60.0f,
	                  .byHand = true,
	                  .fromWorship = true,
	                  .fromSite = true,
	                  .circleRadius = 25.0f,
	                  .holdBeforePress = 2.0f}},
	    .landScript = k_HolderTownScript,
	    .commands = {{.kind = Command::Kind::HandTakeShownSeed, .delaySeconds = 95.0f},
	                 {.kind = Command::Kind::HandCastOnDispenser, .delaySeconds = 3.0f, .value = 0}},
	    .worshipShare = 1.0f,
	});
	all.push_back({
	    .id = "worship.leash_feeds_site",
	    .name = "A creature tied to the food flag of a worship site feeds the site",
	    .facet = Facet::Leash,
	    .description = "The land of the ring scenario. All of the people go to worship at the site, which stands built. "
	                   "A tiger that knows the food miracle stands east of the temple. The tiger gets the rope leash, "
	                   "then the hand ties the leash to the food flag of the site.",
	    .expected = "First the tiger does the order on the flag, as for a tie to any thing: it walks straight toward the "
	                "altar, where the game keeps the flag, and looks at it. When that is done, it walks to about 20 from "
	                "the altar and casts the food miracle there. While the tie stays and the food flag is more than six "
	                "tenths of its full height, the tiger tries to feed the site again each time it completes its work. "
	                "The inspector shows the plan CastMagicFoodByWorshipSite on the flag.",
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
	all.push_back({
	    .id = "worship.site_food_seen",
	    .name = "A creature on the learning leash sees the food miracle poured on a worship site",
	    .facet = Facet::Mind,
	    .description = "The land of the ring scenario. All of the people go to worship at the site, which stands built. "
	                   "A grown-up tiger with no needs stands east of the site, looks at it and gets the learning leash. "
	                   "After 20 seconds the hand pours the food miracle on the food pot of the site for 5 seconds.",
	    .expected = "Each grain that falls into the site shows the player's deed of putting food in a worship site: the "
	                "log shows \"shows deed 0\" for the site. The tiger notices the deed: it points at the site, amazed, "
	                "and watches the camera. The inspector shows deed 0 as the deed that the tiger saw.",
	    .framing = SiteFraming(),
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .offset = {40.0f, -50.0f},
	        .facingDegrees = 270.0f,
	        .needs =
	            NeedOverrides {
	                .energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f, .warmth = 0.0f}}},
	    .miracles = {{.type = MagicType::Food,
	                  .point = {-4.0f, -51.0f},
	                  .handOffset = {-4.0f, -51.0f},
	                  .delaySeconds = 20.0f,
	                  .holdSeconds = 5.0f,
	                  .byHand = true}},
	    .landScript = k_PlayerOneTownScript,
	    .commands = {{.kind = Command::Kind::PutOnLeash, .creature = 0, .delaySeconds = 21.0f, .leash = LeashType::Rope}},
	    .worshipShare = 1.0f,
	});
}
