# Multiplayer rules

The rules of a multiplayer game: teams and clans, the winning conditions and time limit of patch 1.2, and the points of
the online ranking.

**Progress: 0/21 done, 0 partial — 0%**

## Teams and clans

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Players form teams in the front end | todo | no network code in openblack |
| A team's leader can play for a clan, whose creature it then uses (patch 1.2) | n/a | clan creatures came from the original's servers; the server side is rebuilt in bwgame-service, see [clans.md](clans.md) |
| Clan details and lists | n/a | the original's servers are gone; rebuilt in bwgame-service, see [clans.md](clans.md) |

## Winning conditions (patch 1.2)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers killed | todo |  |
| Villagers eaten by the creature | todo |  |
| Villagers born in the player's towns | todo |  |
| Houses built | todo |  |
| Wonders built | todo |  |
| Food in the town stores | todo |  |
| Wood in the town stores | todo |  |
| Towns taken over | todo |  |
| Prayer power generated | todo |  |
| Buildings smashed by the player | todo |  |
| Buildings smashed by the creature | todo |  |
| Belief in the player across the world | todo |  |
| New towns built (a town centre, a store and at least one lived-in house) | todo |  |
| Villagers converted | todo |  |
| Trees grown | todo |  |
| Each condition has a value, and once reached it stays done | todo | each map starts the values at its own defaults, e.g. food and wood 9,000 and 4,000 on [Two Gods](maps/two_gods.md), 5,000 and 5,000 on [Three Gods](maps/three_gods.md), 1,000 and 1,000 on [Four Gods](maps/four_gods.md), and 11,000 on the online-only originals of [Island Wars](maps/island_wars.md) and [Firestorm](maps/firestorm.md) |
| Conditions are mixed and matched; the first to complete all wins | todo |  |
| Others are ranked by the share of conditions done; ties go to the most influence | todo |  |
| A time limit from 10 minutes to 24 hours; at its end the highest share wins | todo |  |

## Points

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Internet games earn points for the online ranking, more for beating a better player | n/a | the original's ranking servers are gone |
| Creatures from the original's games online keep what they learn | todo | see online_services.md |
