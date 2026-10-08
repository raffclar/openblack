# Creature physiology

A creature's body lives through time: it gets hungry, tired, thirsty, hot or cold, needs to poo, grows stronger from
work and fatter from overeating, is healed by sleep and faints when exhausted, starved or out of life. Each species has
its own rates, from the game's creature tables.

**Progress: 39/52 done, 7 partial — 82%**

## Life and damage

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature has a life from 0 to 1, shown on the status panel as damage | done | `creature_physiology::Needs::life`; `CreatureStatusPanel.cpp` |
| Miracles, fire and thrown things take life away | partial | damage from miracles and fire goes through `WorldObjects.cpp` damage; damage from being hit by thrown objects is in [../physics](../physics/) and unconfirmed for creatures |
| A creature with no life left is knocked out, not killed | done | `WorldObjects.cpp` knocks it out through `CreatureFightSystem::KnockOut` |
| The heal miracle gives life back, and scripts can make a creature unable to die (a killing miracle restores it instead) | done | `MagicLiving.cpp`; `Creature::canDie`, set by the tutorial's developer script call; Lethys's kidnapping at the end of Land 2 uses the same switch: see [../story/losing_and_game_over.md](../story/losing_and_game_over.md) |
| Sleeping and resting heal it by the species' rate | done | `creature_physiology::SleepTurn`; test `SleepHealsAndRestsThenWakes` |
| Fights take a quarter of the fight health lost off the real life | done | see [fighting.md](fighting.md) |
| Low life feeds the wish to rest and get better, and fear and anger from being damaged | partial | the body drives the restoring-health source (`SourceValue`); see [desires.md](desires.md) |
| A creature only dies for good when a script says so | done | `CreatureFightSystem::KillPermanently` |

## Energy, hunger and fat

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Energy runs down over time at the species' rate; hunger is energy missing | done | `TickTurn`, `Hunger`; test `EnergyRunsDownSlowerForBigOrSleepingCreatures` |
| Bigger creatures use energy more slowly, down to half as fast at size 2, and much more slowly asleep or resting | done | `k_EnergySizeFactor`, `k_RestingEnergyDivisor`; same test |
| Below half energy it burns fat, getting thinner | done | test `HungryCreaturesBurnFat` |
| Eating fills it by the food's value, up to its size; a big meal pushes energy past full | done | `creature_physiology::Eat`; test `EatingFillsItUpFattensAndBuildsPoo` |
| Overeating makes it fatter by the species' factor | done | same test |
| Fatness shows on the body slowly, a step each turn | done | see [appearance.md](appearance.md) |
| Each meal is counted | done | `Needs::meals` |
| What it eats changes it: villagers eaten make it more evil (unconfirmed how), poisoned food makes it sick and holds back its hunger for a while | todo | see [feeding_and_thrown_things.md](feeding_and_thrown_things.md) |
| Starving, it faints | done | `ShouldFaint`; test `FaintingOnlyForGrownUpOwnedCreatures` |

## Tiredness and sleep

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Moving tires it at the species' rate; young creatures up to four times faster, less as they age | done | test `MovingTiresYoungAndHungryCreaturesFaster` |
| Walking with too little energy tires it faster still | done | same test |
| Actions cost energy and tiredness by the table, less for bigger creatures | done | `ApplyActionCost`; test `ActionsCostEnergyAndBuildStrength` |
| Casting miracles costs energy and tires it | done | see [creature_casting.md](creature_casting.md) |
| Exhausted, it moves only slowly | done | test `ExhaustedCreaturesGoSlowly` (locomotion) |
| Asleep it rests at the species' rate; it never wakes in the first few turns, wakes fully rested in the day or nearly rested after sleeping long enough for its size | done | `SleepTurn`; test `SleepHealsAndRestsThenWakes` |
| Night makes it sleepy | partial | the body drives the night tiredness source from the sky's clock (`SourceValue`, `IsNight`); see [desires.md](desires.md) |
| Fully exhausted, it faints | done | `ShouldFaint` |

## Thirst, poo and sickness

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Thirst builds up over the species' time to dehydrate, once grown enough | done | test `ThirstBuildsUpOnceGrownEnough` |
| Drinking quenches it fully | done | `creature_physiology::Drink` |
| Eating builds up poo by the energy gained | done | `Eat`; test `EatingFillsItUpFattensAndBuildsPoo` |
| Having a poo empties it and leaves a lump on the ground behind it, sized by the creature | partial | `CreaturePhysiologySystem::Poo`; the game throws the lump back along the ground, here it is set down there |
| Being sick throws drops from its mouth that lie on the land a while and fade | done | `CreaturePhysiologySystem::Puke` |
| Illness, from the ill spell or bad food, makes it want to be sick | partial | the ill spell drives it (see [../miracles](../miracles/)); illness from food is todo |
| Itchiness makes it want to scratch; only the itchy spell makes it itch | done | `Needs::itchiness`, the itchy spell in `MagicCreatureSpells.cpp` |
| The creature can get high, which makes its eyes stoned and its desires odd; eating magic mushrooms and toadstools causes it | todo | see [feeding_and_thrown_things.md](feeding_and_thrown_things.md) |

## Warmth and weather

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Warmth follows how far the temperature where it stands is from what its species likes, through the game's sigmoid | done | test `WarmthFollowsTheSigmoidOfTheTemperature`; temperature from `WeatherSystem` |
| Too cold it wants to get warmer and shivers; too hot it wants to get colder and shows it | partial | the sources are driven; shivering and showing hotness are actions in [object_actions.md](object_actions.md) |
| Fire on or near it warms it, and water cools it (unconfirmed) | todo | |

## Strength

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Carrying heavy things while moving makes it stronger | done | test `CarryingMakesItStronger` |
| Actions add strength by the table | done | test `ActionsCostEnergyAndBuildStrength` |
| Strength fades over time at the species' rate (unconfirmed) | partial | `Species::strengthDecay` is read; whether it is applied as the game does is unconfirmed |
| Strength shows on the body as weak or strong | done | see [appearance.md](appearance.md) |
| Strength sets how heavy a thing it can pick up | todo | see [object_actions.md](object_actions.md) |

## Ageing, fainting and the rest

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It ages a tick per species' length of game time | done | test `ItAgesOncePerTickOfGameTime` |
| A new creature starts with its species' energy and warmth | done | `creature_physiology::Start`; test `ANewBodyStartsFromItsSpecies` |
| The body changes nothing before the first stages of growing up | done | test `NothingChangesBeforeTheBodyStage` |
| Only creatures owned by a player and grown far enough faint | done | `ShouldFaint` |
| Fainted, it lies still, is carried to its pen and comes round no longer quite exhausted, starved or parched | done | `WakeFromFaint`; `CreatureFightSystem` carries it home; see [home_and_pen.md](home_and_pen.md) |
| How far a creature sees depends on its size | done | `creature_look` range; see [idle_behaviour.md](idle_behaviour.md) |
| The body drives the desire sources: hunger, tiredness, thirst, poo, warmth, itch, health | done | `SourceValue`; tests `TheBodyDrivesItsDesireSources`, `LeftAloneItsBodyDrivesItsHungerThirstAndPoo` |
| Each species has its own rates from the game's tables | done | `CreaturePhysiologySystem` reads the creature info rows |
| Scripts can read and set the body's values (warmth, fatness, energy, itchiness, poo, exhaustion, thirst, fight health) | todo | `GET_PROPERTY`/`SET_PROPERTY` are stubs in `CHLApi.cpp` |
| The body is saved with the game | todo | no saved games; see [../engine](../engine/) |
| The debug spawner shows and sets every value | done | `CreatureSpawnerBody.cpp` (openblack only) |
