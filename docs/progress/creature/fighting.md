# Fighting

Creatures fight each other in duels inside a circular arena. The player directs their own creature with the hand,
clicking the opponent's body high, middle or low to strike, their own creature to block and the ground to step, or
leaves it to fight by itself as it has learnt to. Nobody dies in a fight: the loser faints, is carried home and recovers.

**Progress: 32/56 done, 10 partial — 66%**

## Starting a fight

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An angry creature can choose to fight another creature it sees | done | `CreatureFightSystem` takes fights the mind chooses; see [decision_making.md](decision_making.md) |
| Tying the leash from the player's creature to another creature starts a fight | done | `CreatureFightSystem` (the leash's act-on hook); see [leash.md](leash.md) |
| A creature too badly hurt won't start a fight | done | `creature_fight` health threshold; test `OutcomesAndStarting` |
| Asked to fight, the other creature can refuse, and the player is told why | todo | the help text of refusals is in [lessons_and_help.md](lessons_and_help.md) |
| Neither creature may already be fighting or lying knocked out | done | `CreatureFightSystemInterface::StartResult::Busy` |
| The creature that starts it makes an arena between the two, sized by the bigger creature, with a cap | done | `MakeArena`, `ArenaRadius`; test `ArenaIsSizedByTheBiggerCreatureAndCapped` |
| An arena already near is reused rather than a new one made | todo | (unconfirmed how near) |
| The arena is marked out on the land and shown while the fight lasts | todo | |
| The creatures walk to their places on either side, point at the arena, and play their fight start | partial | they take their places and play the start (`ArenaSpot`, the start state); pointing at the arena is todo |
| Scripts can start a fight and set where the loser goes | partial | `StartFight` is used by the debug tools; `SET_FIGHT_EXIT`, `GET_ARENA` are still stubs in `CHLApi.cpp` |

## The player's controls

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Clicking the opponent's body strikes at the band clicked: high, middle or low | done | `Press`, `BandAt`; test `ClickHeightPicksTheBand` |
| Clicking their own creature blocks | done | `CreatureFightSystem::Press` |
| Clicking the ground steps forward, back or sideways, by the longer axis of the click | done | test `GroundClickStepsAlongTheLongerAxis` |
| Stepping back or sideways is only allowed within the arena; forward always | done | test `StepsBackOrSidewaysOnlyWithinRange` |
| Holding the click charges a blow, up to 1.2 seconds, playing a power-up while it waits | done | test `ChargeRunsToOnePointTwoSeconds` |
| Moves queue up, at most twelve; a click can replace the queue | done | test `QueueHoldsTwelveAndAClickReplacesIt` |
| Being hit takes away a charged blow still waiting | done | test `GettingHitTakesAWaitingBlowAway` |
| Moves are taken from the queue only in range, from the stance or a block, and a blow only once charged | done | test `OrdersAreTakenInRangeAndOnceCharged` |
| A special move, the species' big blow, at twice the force | partial | the move plays and lands at twice the force; how the player calls it in the game is unconfirmed and not wired to the hand |
| Casting a miracle in a fight: attacking ones at the opponent, defending ones on itself | partial | `CreatureFightSystem::CastFightSpell` casts the game's fight miracles; the player can only order it from the debug spawner |
| Miracle icons appear by the arena for the miracles the creature can use in fights, to pick from | todo | |
| The hand's tooltip during a fight says what a click will do | todo | |
| The player taking the creature over gives it back to the computer after 15 seconds without a move, 3 at the start | done | test `PlayerMovesTakeBackControlAndTeach` |

## Blows and damage

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each creature has a fight health, starting at half its life plus a half, and a stamina | done | test `HealthStaminaAndLife` |
| A blow's force is the attacker's size and strength against the defender's, times its speed, within limits | done | test `DamageFollowsTheFormula` |
| A blocked blow does a tenth of the damage | done | test `ABlockedBlowDoesATenth` |
| Blows land where the attacker's hand reaches; the creature steps in to land the chosen band | done | test `ABlowIsChosenThatLandsAtTheBand` |
| The fighters never pass through each other | done | test `FightersNeverPassThroughEachOther` |
| A blow throws the victim back or reels it to the side, top or bottom by where it lands, with a wobble | done | test `RecoilsByHeightAndDirection` (top or bottom for high blows is a guess, noted in `CreatureFight.h`) |
| Blows cut, bruise or graze, leaving wounds, some bleeding | partial | test `WoundsByTheBlow`; which wound each blow leaves is guessed by where it lands (see [marks.md](marks.md)) |
| Sparkles fly where blows land | todo | |
| Blow and block sounds, the crowd's cheers, and fight music | partial | blow sounds play from the animations ([animation.md](animation.md)); fight music is todo ([../audio](../audio/)) |
| Miracles cast at a fighter make it reel | done | `CreatureFightSystem::Recoil` |
| Stamina comes back slowly each turn | done | `RegainStamina` |
| The game keeps counts of attacks, blocks and steps for the statistics | todo | kept in the player's help record and shown on Tech Stats ([../interface/statistics_counted.md](../interface/statistics_counted.md)) |

## Fighting by itself

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Left alone the creature chooses its own moves by what its opponent is doing | done | `ChooseMove`; tests `ComputerChoosesByWhatTheOpponentDoes`, `OpponentStatesAsTheComputerSeesThem` |
| How aggressively it fights follows what it has learnt: each move the player makes nudges it towards attack or defence | done | test `PlayerMovesTakeBackControlAndTeach`, `ComputerKeepsTheGameTiers` |
| A creature's first fight starts it leaning by its alignment: evil attacks, good defends | done | `CreatureFight.h` first-fight rule |
| Scripts can make a creature fight by itself whatever the player does, and ask whether it does | partial | `SetAutoFighting` exists; `SET_CREATURE_AUTO_FIGHTING` and `IS_AUTO_FIGHTING` are stubs |
| Scripts can queue blows, steps and spells for a creature and read its moves | todo | `SET_CREATURE_QUEUE_FIGHT_*`, `GET_CREATURE_FIGHT_ACTION`, `CREATURE_FIGHT_QUEUE_HITS` are stubs |
| Computer players' creatures fight with the computer's tactics | partial | they fight by the same computer moves; the computer player's choice of when to fight is in [../multiplayer](../multiplayer/) |

## The end of a fight

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| At no fight health a creature faints; the other wins | done | `CreatureFightSystem`, test `OutcomesAndStarting` |
| A quarter of the fight health lost comes off the creature's real life | done | `HealthStaminaAndLife` |
| The loser lies out cold for a while by its size, is carried home (fading out and in), rests until healthy enough and gets up | done | `KnockOut`, `CreatureKnockedOut`, `HomeOf` |
| Spells on a knocked-out creature are brought to their end | done | `CreatureFightSystem::KnockOut` |
| After the fight each creature responds: the winner celebrates, the loser sulks, and they remember how they feel about each other | partial | the finish animation plays; the response and the change of attitude are todo |
| Creatures never die in a fight; only a script kills one for good | done | `KillPermanently` |
| A fight can be called off with no winner | done | `AbortFight` |
| The fight panel shows each creature's name over a bar of fight health and a thinner bar of stamina | done | `src/Creature/CreatureFightHud.*`; test `LayoutAndColours` |
| The camera goes to watch the player's creature fight from the side of the arena, and the player can leave the fight view | partial | `CreatureFightSystem` frames the duel; openblack follows the fighters as they move where the game keeps still, and leaving the view is todo |
| The fight shows text on the screen as it ends | todo | |
| Villagers nearby gather to watch a fight and react to it | todo | see [../villager](../villager/) |
| Creatures and animals nearby react to a fight | todo | see [reactions.md](reactions.md) |
| Fights won earn belts and medals shown in the Creature Cave | todo | see [creature_cave.md](creature_cave.md) |
| Arenas are saved and loaded with the game | todo | no saved games |
| The tutorial for the learning to fight stage of growing up shows how to fight | todo | see [development_phases.md](development_phases.md) |
