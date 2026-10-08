# Football

Lionhead's free Soccer Add-On for Black & White (summer 2001) turns on football: towns can build a pitch from eight
scaffolds, and relaxing villagers play five-a-side matches on it while others watch and do the Mexican wave. The code
for all of it ships in the game. The add-on only switches it on.

**Progress: 2/53 done, 1 partial — 5%**

## The add-on and its switch

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Football is off unless the add-on has switched it on; the game checks once when a game starts | todo | openblack has no football switch; Diego's branch hard-codes it off (`k_FootballEnabled` in its scaffold and building-site ports) |
| The switch only works outside multiplayer: in a multiplayer game football stays off even when installed. Skirmish counts as single-player | todo | of the game's five add-on switches, only the MP3 player and the Pentium 4 one also work in multiplayer |
| The switch is a binary value, `BWFeatureKey`, under `Software\Lionhead Studios Ltd\Black & White\BWSetup` in the registry. It holds a list of 32-bit codes. Each code is a scrambled checksum of the feature's name minus the same checksum of the Windows product ID, so a code only works on the machine it was made for | n/a | copy protection tied to the Windows product ID. openblack should use a plain setting (row above). The product ID is read from the Windows 9x location, which is probably why the unofficial 1.42 patch (April 2019) lists "fixed addons like football mod not activating" |
| The same switch turns on four other add-ons: an MP3 player, creature music moods, the creature dancing to the sound card's line-in, and a Pentium 4 option | n/a | not football; listed because they share the mechanism |
| The switch first appears in patch 1.1. Version 1.00 has the football code but not the feature key, and press reports say the add-on needs patch 1.1 | n/a | checked in the 1.00, 1.10 and 1.20 executables. How 1.00 treated football was not traced |
| The download (about 1.1 MB, Lionhead, listed August 2001) | n/a | the installer is not in the game folder. The ball and goal model `Data\football.l3d` is dated 22 February 2001, so it is part of the base game. What the installer copies besides the registry code is undetermined |

## The pitch

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every tribe's pitch is the same: one model (not in the tribe's style), 6000 wood, 8 scaffolds, up to 6 builders, influence 20, impressiveness 0.0015 | todo | read from the abode table. A bigger pitch model and separate centre-circle, corner-flag and goal models exist, but no table uses them (whether code draws them was not traced) |
| Scaffolds join up to 8 only with football on (7 otherwise), so only then can a pitch be offered | todo | see [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md) |
| A town wants a pitch only with football on, only when it has no pitch and none being built | todo | see [town_desires.md](town_desires.md) |
| The building site's ring is a circle for a pitch | todo | ported on Diego's branch (`BuildingSites.cpp`), not in openblack; see [../building/construction.md](../building/construction.md) |
| Finishing a pitch does not set off the "new building" reaction other civic buildings do | todo | per Diego's branch (`Abodes.cpp`), not in openblack |
| Villagers build it like any building. Its own "going to build the pitch" state does nothing | todo | see [../building/construction.md](../building/construction.md) |
| Scripts can place a pitch like any other building | done | `CREATE_ABODE` via `src/ECS/Archetypes/AbodeArchetype.cpp`. One multiplayer map's Tibetan town has one |
| The pitch drapes over the land | done | `MorphWithTerrain` in `AbodeArchetype.cpp` (the pitch is a morphing object) |
| Nothing collides with it, and the creature need not walk round it | partial | not an obstacle: `physics_classes::IsObstacle`, see [../physics/collisions.md](../physics/collisions.md). The creature walking over it is not modelled |
| The pitch takes no damage: anything that would hurt it leaves its life full | todo | |
| Once built, it becomes the town's pitch (one per town) and makes its ball on the centre spot | todo | |
| Whenever the pitch has no ball, it makes a new one on the centre spot | todo | so a lost ball is replaced at once |
| Its question-mark text says a game starts once it is built and nobody is worshipping; the tooltip says "Football Pitch" | todo | see [../hand/pointing_and_tooltips.md](../hand/pointing_and_tooltips.md) |
| With football off, a pitch placed by a script never runs a match, but relaxing villagers can still go to it and wait there | todo | the town updates its pitch only while football is on. Nothing stops villagers joining. What ends their wait is undetermined |

## Who plays

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A villager taking up relaxation picks the most wanted of the town's pitch, its god's worship place and the town's artefacts | todo | see [../villager/play_and_gossip.md](../villager/play_and_gossip.md) |
| The pitch is wanted at 1.5 until the first match has ended. After a match it is not wanted for 3 minutes, then at a random 0 to 1 | todo | |
| Villagers under 15 never join. Nobody joins once both teams have five and there is a referee | todo | |
| The first to arrive becomes the referee. Then the home team fills to five, then the away team, and the rest come to watch | todo | the referee is given no actions of his own |
| Players walk to a random spot within 4 m of the centre and wait for kick-off | todo | |
| Town players wear their own clothes: there are no kits or tribe teams, just home and away | todo | |
| Players and spectators stay in the town and its desires: every football state counts toward the town's playtime | todo | |

## The match

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game counts time in turns of a tenth of a second. A match waits, plays, pauses after a goal, and can be stopped | todo | |
| With fewer than three a side, waiting players are sent away after 180 s | todo | |
| With three or more a side, after 30 s the ball is put on the centre spot (unless it is in flight). Kick-off comes once fewer than five players are still walking | todo | |
| At kick-off and every 15 s of play each side is re-ranked by distance to a goal. The first half attack, the rest defend and the last keeps goal, so five play 2-2-1 and three play 1-1-1 | todo | which goal they are ranked from was not determined. The ten-place start table is in [../story/minigames.md](../story/minigames.md) |
| If a side has no players, or both have only one, the match goes back to waiting | todo | |
| A town match stops once it has run 3 minutes and playtime and relaxation are no longer what the town wants most. Players and the first 40 spectators go home | todo | matches started by a script never stop this way |
| A goal counts when the ball crosses a goal mouth taken from the goal model, and only after a kick has set it moving | todo | whether a ball thrown by the hand can score was not determined |
| After a goal the scorers celebrate and the others mourn for 7 s (70 turns), then play resumes. The pitch does not put the ball back itself; it is restarted as a dead ball | todo | the restart is inferred from the dead-ball rule below |
| The score is kept per side, but the tally and the creature's "my side scored" test credit the side the villagers mourn for. Only the debug text shows the tally | todo | reproduce as-is: celebrations follow the villagers' reading |
| When the ball leaves the pitch, one side's first-listed player fetches it for a restart while the rest stand paused. The game compares the two sides' first players and sends the one further from the ball | todo | see ../villager/tools_and_carried_items.md for the carried ball |
| The ball is removed if it goes more than 100 m from the pitch, into water, or more than 80 m away in a god's hand, and a new one appears on the centre spot | todo | |
| Each kick launches the ball on an arc that lands on its target, with flight time = distance ÷ kick speed, under gravity 9.81 | todo | kick speeds per role: [../story/minigames.md](../story/minigames.md). Ball bounce: [../physics/object_dynamics.md](../physics/object_dynamics.md) |
| There are no fouls, throw-ins, corners, injuries or full-time whistle in a town match | todo | nothing in the rules hurts players. The creature's "foul" is below |
| When a match ends the town gains belief: 0.01 times up to twice a measure of how long the match ran | todo | the measure reads a match count nothing in the football code sets, so the amount is undetermined |
| Leaving a match by any other state (picked up, scared, called away) takes a player off the team. A spectator who leaves keeps their wave place until they are gone | todo | |

## Spectators

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The first 40 spectators take places 9° apart all round the pitch, just outside it, scattered by up to 2 m. Later ones stand at random points round it | todo | |
| They watch the pitch before kick-off and the ball in play, picking one of three watching animations at random, and others after a goal | todo | |
| Mexican wave: each turn the next of the 40 places, if filled and standing still, starts the wave. After all 40, the wave rests 256 turns (25.6 s), then goes round again | todo | |
| A match impresses those watching (football has its own kind of impressiveness) | todo | see ../worship/ |

## The player and the creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand can pick up and throw the ball (it shows the ball's in-hand help) and pick up players | todo | see ../hand/ |
| The football is a toy ball for the creature: it may kick it about and it never hurts it | todo | see [../nature/toys.md](../nature/toys.md) |
| The creature can act on a villager who is playing: pick up the ball (first, two times in three, a short gesture when its hand is empty) and throw it at the goal its side attacks. The kick-at-goal, clear and catch actions all do this | todo | see [../creature/town_actions.md](../creature/town_actions.md) |
| It can stomp on the ball, and as "goalkeeper foul" stomp on the first-listed player of the other side | todo | |
| It celebrates when its side has just scored and commiserates when the other side has. Creatures of even-numbered players are on the home side | todo | |
| Its god throwing the football into the goal, or catching it, is a deed it can notice | todo | see [../creature/learning_by_observation.md](../creature/learning_by_observation.md). Where the game reports these was not found |
| Miracles have no football effect of their own; players can be hurt or killed like anyone | todo | see ../miracles/ |

## The cup final and the footballers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Two special villagers: "Footballer Team 1" in a Norse kit and "Footballer Team 2" in a Celtic kit, belonging to no tribe | todo | see [../villager/special_villagers.md](../villager/special_villagers.md) |
| Both appear in the shipped Land 5 end credits: a Celtic footballer among the ten waving villagers, and a Norse footballer in the special shepherd's place | todo | the credits script is compiled into the game. See [../story/ending.md](../story/ending.md) |
| Cup final: seven Norse (away) and seven Celtic (home) footballers, five on and two subs, a referee and up to 15 fans from each town. The evil advisor says a crowd is forming | n/a | the challenge's script is not compiled into the shipped challenges. See [../story/minigames.md](../story/minigames.md) |
| In the cup final, a player whose health runs out is replaced by a sub, with a cry, the camera following him and the advisor's line ("Hard tackle. Not a foul, though, I say.") | n/a | as above |
| The cup final lasts 300 s or until one side loses all seven. With 30 s left the first Norse sub is set on fire and killed, so the Celts "cheat" | n/a | as above. No winner is declared; everyone is sent home |
| Script commands to find a town's pitch, stop its games, keep it for scripts only, start a match with a chosen referee, and add or remove players (at most five a side) | todo | see [../scripts/challenge_natives_objects_and_world.md](../scripts/challenge_natives_objects_and_world.md) |

## Sounds, statistics and leftovers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The ball has its own collision sound, and kicks play the kick sound | todo | see ../villager/tools_and_carried_items.md and ../audio/ |
| The sound list names a referee's long and short whistle, a huge crowd and a goal sound, but no recordings by those names ship | n/a | no football music or chants exist |
| The game counts no football statistics | n/a | the score lives only in the pitch and its debug text |
| A debug view shows the match state and the score | n/a | developer only; see ../debug/ |
| An older pitch kind (with its own ball and place table) remains, but every villager hook it relies on is empty | n/a | no table or script makes one |
| A "Miraculous soccer ball" miracle was cut: a creature-lesson line and a miracle-icon model remain | n/a | the shipped miracle list has none |
