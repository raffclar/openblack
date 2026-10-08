# Worship sites

Each tribe a player wins over gets a worship site beside the player's temple, where that tribe's villagers dance to make
prayer power. The site holds the tribe's miracle icons, a food store for the dancers and an altar. Artefacts put down at a site gain worth there: see [../town/artefacts.md](../town/artefacts.md).

**Progress: 0/25 done, 2 partial — 4%**

## The site in the world

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts place worship sites (built and planned) for the players' citadels | todo | worship site commands are stubs in `src/LHScriptX/FeatureScriptCommands.cpp` |
| A site stands round the temple at its tribe's distance from the citadel, facing out from it | todo | the distance is in the worship site table (`GWorshipSiteInfo::radiusFromCitadel`, `src/InfoConstants.h`); nothing uses it |
| One site per tribe: every town of the same tribe a player owns is assigned to that tribe's site | todo | |
| A site is made for a newly claimed town's tribe if the player has none yet | todo | links to town conversion, see `../town/` |
| A town the player loses is taken off its site; a site with no towns left goes (unconfirmed) | todo | |
| The site has its tribe's model, base and the player's colours | todo | meshes only listed in `src/3D/AllMeshes.cpp` |
| A ring of light stands round the altar in the player's colour, as bright as the site's prayer battery is full | todo | see [../rendering/light_beams.md](../rendering/light_beams.md) |
| The site has a gate the worshippers come in by, a dance floor, a rest place and a food place | todo | |
| Worship site upgrades: built on from a planned upgrade, they make the site bigger (unconfirmed what else they change) | todo | |
| The site can be set on fire, burning about its centre | todo | the fire system (`src/ECS/Systems/Implementations/FireSystem.cpp`) has no site to burn |
| Scripts can stop a player building worship sites | todo | stub in `src/CHLApi.cpp` |
| Scripts can read how many of a town's people died worshipping | todo | stub in `src/CHLApi.cpp` |

## Food for the dancers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The site has its own food pile that the dancers eat from | todo | |
| How much food the site needs is worked out from its dancers and how hungry they are | todo | |
| Villagers carry food from their town's store to the site when it runs short | todo | see `../villager/` |
| Disciples and the creature bring fish and crops straight to the site | todo | |
| Food poured on the site by the food miracle or dropped there by hand goes into its store | todo | the miracles pour food into stores (`src/ECS/Systems/Implementations/MagicResources.cpp`), but there's no site to take it |
| Casting food on a site teaches the creature that feeding worshippers is a thing its god does | partial | the deed is recognised (`src/Magic/MiracleDeeds.cpp`); there are no sites to cast on |
| The site shows a sign of what it needs (food) over it | todo | |
| Computer players drop the resources they give at their own site | todo | |

## Icons, artefacts and shelter

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Artefacts kept at the site make power-ups cheaper by the artefact multiplier | todo | the multiplier is in the table, unused |
| In danger, a town's villagers run to the site to hide | todo | |
| A site with more worshippers than it has room for is overloaded; only so many dancers are drawn | todo | `maxDancersVisible` in the table, unused |
| The creature can be sent to the site, and treats it as a place to work, e.g. casting food there | todo | see `../creature/` |
| A cheat gives a site endless prayer power | partial | only in the prayer power debug sandbox (`src/Debug/Magic.cpp`) |

The miracle icons round the site, and summoning from them, are in `../miracles/`.
