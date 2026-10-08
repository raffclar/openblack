# Town actions

What a creature does for or against villages: feeding them, building for them, playing and dancing with the villagers,
making disciples, or stealing from them, eating them and knocking their homes down. A creature learns these by watching
villagers and the player, and the town watches back, fearing or respecting it. The villagers' and the town's own side is
in [../villager](../villager/) and [../town](../town/).

**Progress: 7/67 done, 7 partial — 16%**

## Feeding and supplying a town

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Take food from a field to the town's storehouse | todo | see [../resources](../resources/) |
| Take fish from the sea to the storehouse | todo | |
| Shake fruit from a tree and take it to the storehouse | todo | |
| Cast food and put the magic food in the storehouse | todo | see [creature_casting.md](creature_casting.md) |
| Pull up a tree and take the wood to the storehouse | todo | |
| Cast wood and put the magic wood in the storehouse | todo | |
| Drop a cow (or other animal) into the storehouse as food | todo | |
| Put food from a field, fish or magic food by the worship site for the worshippers | todo | see [../worship](../worship/) |
| Give food to the worshippers | todo | |
| Take food or fish to its own home instead | todo | see [home_and_pen.md](home_and_pen.md) |
| Bring an object to a town and leave it there | todo | |
| The player's putting things into a storehouse catches its eye, and it may copy it | partial | the copying rules exist ([learning_by_observation.md](learning_by_observation.md)); the deeds aren't reported yet |

## Building

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Help build a house: carries wood to a building site and adds it | todo | see [../building](../building/) |
| Help repair a damaged house | todo | |
| Take wood from a tree to a building site | todo | |
| Cast wood by a building site | todo | |
| Take wood from a tree to the workshop, or cast wood by it | todo | |
| Steal scaffolding from a building site | todo | |
| Plant a tree | todo | see [../nature](../nature/) |
| Water a tree or a field with the water miracle, also for a town that needs it | todo | see [creature_casting.md](creature_casting.md) |
| Sprinkle water on crops (and its powered-up version) | todo | |
| Raise a town's totem pole, or lower it | todo | see [../town](../town/) |
| The town's totem raised by the hand catches its eye | todo | see [reactions.md](reactions.md) |

## Playing with villagers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Dance with villagers, telling them where to go and when to start and stop | todo | |
| Dance playfully with villagers watching, or joining in | todo | |
| Dance amorously with villagers | todo | |
| Dance on its own by the sea | todo | |
| Practise a dance | todo | |
| Dance outside a worship site, or around a town's artefact | todo | see [../town/artefacts.md](../town/artefacts.md) |
| Tell the villagers a story | todo | |
| Play a game with the villagers, and see who wins | todo | |
| Play football with villagers: attack, kick and throw at goal, defend, stomp on the ball, clear it, keep goal, catch, foul, celebrate and commiserate | todo | see [../town](../town/); [../town/football.md](../town/football.md#the-player-and-the-creature) |
| Kick a ball around, throw a ball at something | partial | it throws what it picks up nearby; balls aren't special yet (see [object_actions.md](object_actions.md)) |
| Playfully frighten villagers | todo | |
| Playfully interact with a villager, playfully kiss one | todo | |
| Stroke a villager | partial | plan action "stroke" walks up to a villager and plays a fond animation; nothing happens to the villager |
| Take a villager home to sleep | todo | |
| Bring villagers to the worship site | todo | |
| Make a villager a disciple (a breeder, among others) | todo | see [../villager](../villager/) |
| Impress a town: show off, throw things or cast miracles in front of it | partial | the leash can make it want to; the impressing actions only play where they stand ([leash.md](leash.md)) |

## Helping a town in trouble

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Decides whether the nearest town should be helped | todo | |
| Heal hurt villagers with the heal miracle | done | plan action "cast heal spell" on hurt villagers; see [creature_casting.md](creature_casting.md) |
| Put out fires in a town | todo | see [object_actions.md](object_actions.md) |
| Shield a town with the shield miracle | todo | |
| Help a town together with a friend | todo | see [friends_and_other_creatures.md](friends_and_other_creatures.md) |

## Harming a town

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Decides whether the nearest town should be attacked; whether a town is under attack | todo | |
| Eat villagers | done | plan action "eat alive"; see [object_actions.md](object_actions.md) |
| Hurl villagers or things at homes | done | the angry hurl at homes and trees (`CreatureIdleMind`) |
| Stamp on and kick homes, damaging them | partial | homes take damage by the creature's size; whether they collapse is in [../building](../building/) |
| Cast lightning at villagers and homes | done | plan action "cast lightning bolt"; see [creature_casting.md](creature_casting.md) |
| Cast fireballs at a town | todo | |
| Set fire to a town | todo | |
| Sacrifice villagers at the worship site | todo | see [../worship](../worship/) |
| Attack a town together with a friend | todo | |
| Steal food from a farm or from the storehouse | todo | |
| Steal wood from the storehouse | todo | |
| Steal an animal, steal a villager | todo | |
| Steal a miracle's seed (choosing which kind) | todo | |
| Steal an object and put it in its own town, or by its citadel | todo | |
| Eating, holding or throwing villagers frightens their town | done | `CreatureObjectActions::AttitudeTo`; test `TheTownFearsThrowingAndEatingVillagers` |

## What the town makes of it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The town sees what the creature does as nothing much, fearsome or worthy of respect | done | `CreatureObjectActionSystem::ProcessTurn` (town attitude) |
| A fear lasts thirty seconds after the creature stops, respect ten | done | `CreatureObjectActions` keep times |
| Villagers react to the creature: run from it, cheer it, gather to watch | partial | the town's view is kept; the villagers' reactions are in [../villager](../villager/) |
| How impressed a town is with the creature adds to the player's belief there | todo | see [../worship](../worship/) |
| The town's attitude to the creature is shown in the creature's help messages | todo | see [lessons_and_help.md](lessons_and_help.md) |
| The creature's own belief about each town (whose, how big, how it feels) guides what it does to it | partial | see [beliefs_and_opinions.md](beliefs_and_opinions.md) |
| A creature that messes up an action (a dance, a fire, impressing villagers, raising a totem) is told why | todo | see [lessons_and_help.md](lessons_and_help.md) |
