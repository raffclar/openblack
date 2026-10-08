# Casting and globes

How the player takes a miracle into the hand, holds it, powers it up, casts it with the mouse and drops it, and how
the one-shot globes look and are taken. Gesture recognition itself is in [../gesture/](../gesture/); the prayer power
side is in [prayer_cost.md](prayer_cost.md); dispensers, icons and where globes come from are in
[dispensers_and_seeds.md](dispensers_and_seeds.md).

**Progress: 40/54 done, 7 partial — 81%**

## Buttons and pressing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The right button casts the held miracle; the left button taps globes and grabs as usual | done | `src/Game.cpp` input, `src/Magic/CastInput.cpp` |
| Gesture miracles (fireball, storm, shields, flocks) arm on press and cast on release | done | `src/Magic/CastInput.cpp`; test `CastInput.AGestureSeedIsArmedOnPressAndCastOnRelease` |
| Held miracles (lightning, food, wood, water) lock on press, cast at once, then apply once a game turn where the hand points | done | `src/Magic/CastInput.cpp`; test `CastInput.AHeldSeedLocksOnCastsAtOnceAndAppliesOnceATurn` |
| Placed miracles (forest, heal, teleport, blast, creature spells) cast at once on press | done | test `CastInput.APlacedSeedIsCastOnPress` |
| A creature spell can only be cast on a creature | done | test `CastInput.ACreatureSpellIsCastOnlyOnACreature` |
| An object under the hand is tried before the land | done | test `CastInput.AnObjectUnderTheHandComesFirst` |
| A press outside the player's influence does nothing at all: no puff, no sound, even over a creature | done | test `CastInput.APressOutsideThePlayersInfluenceDoesNothingAtAll`; testbed `miracles.hand_press_outside_influence` |
| A press before the seed is ready fails with the failure puff and sound | done | test `CastInput.APressBeforeTheSeedIsReadyFails`; testbed `miracles.hand_press_too_soon` |
| A press where the miracle's cast rule refuses (needs land, needs influence, out of the map) fails with the puff and the failure sound | done | test `CastInput.PressingWhereItCantBeCastFails`; `FailCast` in `src/ECS/Systems/Implementations/MagicSystem.cpp` |
| Land for a cast rule means a map cell without water, whatever its height | done | test `SpellBehaviours.LandForACastRuleIsACellWithoutWaterWhateverItsHeight` |
| The failure puff is drawn at full size (the camera-distance scaling applies only to effects a player's miracle owns) | done | camera-distance rule in `src/Particles/ParticleUpdateRules.cpp` |
| A second press while a gesture seed is armed is ignored | done | test `CastInput.ASecondPressWhileArmedIsIgnored` |
| While a gesture seed is armed a humming loop plays at the hand, following it, and stops hard on release or cancel | done | hold loop in `MagicSystem.cpp`; test `CastInput.CancellingStopsTheHumOrStoresTheHeldMiracleBack` |
| Storm and shields are cast at the circle drawn while the button is held; a circle is remembered 5 s; with no circle the cast fails | done | `src/Magic/CastInput.cpp`; testbed `miracles.hand_circle_casts` |
| A locked miracle moved over a place it can't apply skips that turn but stays locked; only letting go ends it | done | `MagicSystem::ProcessTurn` |
| Letting go of a held miracle stores its remaining prayer power and age back in the seed, so it can be pressed again until spent | done | `src/ECS/Systems/Implementations/MagicHeldSeed.cpp` |
| After a cast the seed is deleted, kept in the hand, or follows its miracle, as its record says; a success sparkle shows as it leaves the hand | done | test `SpellLifetime.TheSeedStaysGoesOrFollowsByItsRecord` |
| A seed is deleted together with its miracle | done | `MagicSystem.cpp` (core audit item 41) |
| A held miracle carried outside its cast rule (over water, out of influence) is dropped from the hand | done | `MagicSystem.cpp` (core audit item 39) |
| A hand-cast miracle is placed at the hand's reported point | done | `MagicSystem.cpp` (core audit item 38) |
| Every hand cast lasts the miracle's player timer times the seed's multiplier | done | `src/Magic/SpellRules.cpp`; test `SpellRules.ASeedsCastScalesThePrayerPowerAndTimeAndAFireSeedIsAlwaysTheSameSize` |
| Force-feedback effects on recognising and casting | n/a | openblack has no force-feedback devices |

## Holding a seed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A seed from an icon needs 1.5 s before it is ready; a globe or dispenser seed is ready at once | done | `src/Magic/SpellSeedRules.cpp`; test `SpellSeedRules.ASeedsChargeAndReadiness` |
| Becoming ready cancels a press made too early | done | `MagicSystem::ProcessTurn` |
| The hand holds a seed as a not-ready miracle until it is ready, then by the seed's hold (above, side or magic), refreshed each game turn | partial | `src/Magic/HandHoldPose.cpp`, `src/Magic/HandHoldPoser.cpp`; test `HandHoldPose.ASeedIsHeldAsAMiracleNotYetReadyUntilItIsReady`. Open re-audit: whether the hold-pose code belongs in its current namespace (3D vs Graphics rule) |
| Each hold shows a still frame of its hand animation chosen by the seed's size, with no leaning | done | test `HandHoldPose.EachHoldTakesAStillFrameOfItsAnimation` |
| The hand rises by how it holds (a little above, more for magic, by the seed's height at the side) | done | test `HandHoldPose.TheHandRisesByHowItHolds` |
| The seed's model sits below the hand, turned with it (forest half round, ground flock a quarter) | done | tests `HandHoldPose.UprightTheSeedTakesTheHandsTurn`, `HandHoldPose.TheSeedTurnsHalfRoundForARightHandAndByItsOwnTurn` |
| The held seed and hand sway as the cursor runs ahead, and roll about the line to the camera | done | tests `HandHoldPose.TheCursorRunningAheadSwaysTheHandUpToThreeTenthsOfARadian`, `HandHoldPose.TheHandRollsBackAboutTheLineToTheCamera` |
| Taking or losing a seed fades the hand's pose over 0.13 s; becoming ready or powering up does not | done | test `HandHoldPose.TakingASeedFadesTheDrawnHandOverThirteenHundredths` |
| The seed's model shows only once the seed is ready | done | `MagicSystem.cpp` |
| The in-hand effect sits among the fingertips (at the hand point for water), starts as the seed enters, is drawn only once ready and is sized by the hand's distance and the seed's power | done | `HandAnimation::LeafBoneCentre` in `src/3D/HandAnimation.cpp`; `MagicHeldSeed.cpp` |
| Creature potions (phials) are held at the side of the hand | partial | side hold for seeds 12-27 done (user defect fixed); the in-game re-audit of this user-reported defect is still owed |
| A holder effect (fire, lightning, heal, shield, storm, water, teleport) plays round a globe or a held seed of that kind | done | `src/ECS/Systems/Implementations/MiracleFxSystem.cpp` |

## Globes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Tapping a globe puts its miracle in the hand, fully charged and ready, with the bubble-pop sound at the hand; the globe goes | done | `TapOrb` in `MagicSystem.cpp` |
| A globe shows its miracle's model or holder effect inside a transparent spinning sphere with a running glint | done | `src/Magic/MiracleVisuals.cpp`, `src/Graphics/RendererMiracles.cpp`; test `MiracleVisuals.TheGlintRunsThroughItsSixteenCellsEighteenASecond` |
| An extreme globe has one coloured ring per power-up level, turning, in the player's colour (the local player's for a neutral globe) | done | tests `MiracleVisuals.AnExtremeGlobeHasARingForEachPowerUp`, `MiracleVisuals.TheRingsAreTurnedAsTheGameTurnsThem`; testbed `miracles.extreme_globes` |
| A globe's seed is made at the player's icon for that miracle when they have one (so it can be powered up and refunds there) | partial | rule in `MagicHeldSeed.cpp`, but no player has icons until worship sites exist (`PlayerHasSpellIcon` always says no) |
| A creature judges a globe it finds by its owner: one belonging to another player can be stolen | todo | nothing models a globe's owner for creatures; the original's creature mind has "belongs to another player" and "stealable" tests for globes |
| A creature weighs a globe by its miracle: aggressive, compassionate, playful or health-restoring | todo | nothing; the original's creature mind tests a globe for each of these (whether a creature can eat one is unconfirmed) |
| Globes are saved and loaded with the game | todo | no save system |

## Power-ups (extreme miracles)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Drawing a power-up gesture over a ready, uncast icon seed raises it to the next level (fire, lightning, storm, heal, water, food, blast) | partial | works for seeds summoned in the testbed (`miracles.gesture_power_up_storm`); in the game only icon seeds power up and there are no icons until worship |
| A power-up waits for the icon to charge the extra; a scribble cancels the pending level | todo | blocked on worship icons; levels apply at once |
| Extreme versions use their own cast effect, in-hand effect, cost and strength | done | `src/Magic/MagicTables.cpp` level lookup |
| Taking or powering up a seed flies five bands onto the hand, plays the band sound and the announcer names the level | done | `src/Magic/MiracleVisuals.cpp`; tests `MiracleVisuals.BandsFlyOnAndOffAndGo`, `MiracleVisuals.TheAnnouncerNamesThePowerUp`; testbed `miracles.hand_power_up_bands` |
| Bands spin round the hand, faster further up the arm; bracelets stay for each power-up after a delay; the hand glows in the player's colour | done | tests `MiracleVisuals.BandsSpinRoundTheHandFurtherUpTheArmFaster`, `MiracleVisuals.BraceletsCountThePowerUpsAndWaitForTheMiracleToSettle`, `MiracleVisuals.TheGlowFlowsBackwards` |
| A tribe's power spins the tribe's name round the hand, rises as a column on casting, with the tribe's voice | partial | `src/Magic/TribalPowerSpin.cpp`; test `TribalPowerSpin.*`; testbed `miracles.hand_tribal_power` forces it, as tribal power above 1 needs worship. Open re-audit: namespace placement of this code |

## Dropping and cancelling

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A scribble drops the held seed with the shake sound (quiet) and the bands flying off; outside the power-up path it needs the hand in influence | done | `src/Gestures/GestureRequests.cpp`, `DiscardHeldSeed` in `MagicSystem.cpp`; testbed `miracles.gesture_scribble_cancel` |
| A dropped seed gives its prayer power back to its worship site; a globe seed with no icon just vanishes | partial | `src/Magic/PrayerRules.cpp` refunds to the player's prayer power as a stand-in; no worship sites |
| Putting a seed down on a dispenser, worship site or icon returns it there | todo | nothing |
| Leaving the hand records the last miracle (for the repeat gesture), cancels the icon's charge and removes the bracelets | partial | bracelets go; repeat gesture and icon charging wait for worship |

## Choosing a miracle and moving with it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Choosing a miracle by gesture (spiral, or reverse spiral for creature spells, then the miracle's gesture, 30 s to finish) and repeating the last one (40 s) | todo | blocked on worship icons; recognition itself in [../gesture/](../gesture/) |
| A thrown miracle leaves along the hand's smoothed velocity (four fifths of the way in a tenth of a second, speed mapped up to 200) | done | `src/Magic/HandMotion.cpp`; test `HandMotion.TheHandsMovementGetsFourFifthsOfTheWayInATenthOfASecond` |
| A miracle spins by how the hand's path turns sideways as it is cast | done | test `MiracleVisuals.TheHandSpinsAMiracleByHowItsWayTurns` |
| The held miracle and globes are saved and restored with the game | todo | no save system |
