# Creature Mode and the status panel

The camera can lock onto a creature and follow it about while the screen shows how damaged, hungry and tired it is.
Hovering the hand over any creature brings up the same panel, with the reward the hand has given it so far. General
camera controls are in [../camera](../camera/).

**Progress: 28/31 done, 1 partial — 92%**

## Locking onto a creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| C locks the camera onto the player's own creature, and pressed again gives the camera back | done | `src/Creature/CreatureMode.*`, `CreatureModeSystem`; test `CreatureKeyLocksOntoYourCreatureAndLetsGo` |
| Locked onto another god's creature, C moves over to the player's own | done | `creature_mode::OnCreatureKey` |
| Double clicking a creature, anyone's, locks onto that one | done | test `DoubleClickIsTwoQuickPressesOnTheSameCreature` |
| A double click is a second press within half a second, inside a small box of the first, on the same creature | done | `creature_mode::DoubleClick` |
| The cursor keys alone, dragging the land, entering the temple, opening the editor or a script's cinema bars give the camera back | done | `CreatureModeSystem::Update`; test `CursorKeysAloneGiveTheCameraBack` |
| The camera is handed back exactly as the player left it | done | `CreatureModeSystem` keeps the player's camera |
| With no creature, C does nothing | done | `creature_mode::OnCreatureKey` |

## The follow camera

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The camera looks at the middle of the creature's body, by its height for its size | done | `src/Camera/CreatureFollow.*`; tests `LooksAtTheMiddleOfTheCreature`, `CreatureHeightGrowsWithSize` |
| It starts the creature's viewing distance away from afar, or about as close as it already is | done | tests `StartsAtTheViewingDistanceFromAfar`, `StaysAboutAsCloseWhenAlreadyClose` |
| It keeps between a least and a most distance and never lower than about 14 degrees above | done | test `KeepsWithinItsBounds` |
| It eases after the creature, arriving in two seconds as it starts and one later | done | test `EasesInTwoSecondsAtFirstAndOneLater` |
| Shift and the cursor keys turn it round the creature and tilt it | done | test `ShiftAndCursorKeysTurnAndTilt` |
| Ctrl and the cursor keys turn it and draw it in and out | done | test `CtrlAndCursorKeysTurnAndZoom` |
| The mouse wheel draws it in and out, counted twice as the game does | done | test `TheWheelZoomsTwice` |
| Ctrl and Shift together swing it round to where the land falls away, for a clear view, tilting towards about 22 degrees | done | tests `ClearView*`, `ClearingDistanceIsDrawnTowardsFiftyToAHundred` |
| A creature turning too fast for the camera is followed more loosely | todo | (unconfirmed) |
| The camera follows the creature into a fight's view and back | partial | see [fighting.md](fighting.md) |

## The status panel

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| With the hand over any creature, the left of the screen shows its damage, hunger and tiredness as bars | done | `src/Creature/CreatureStatusPanel.*`, `Game.cpp`; test `ShowsOnHover` |
| Below them, the reward the hand has given it so far, from "Bad Boy!" to "Good Boy!" or "No Reward" | done | test `RewardWords`; see [learning_from_feedback.md](learning_from_feedback.md) |
| Damage is life lost, hunger the energy missing, tiredness the exhaustion | done | test `ValuesComeFromTheBody` |
| Values are clamped as the game does and shown as whole percentages cut short | done | tests `ValuesAreClampedAsTheGameDoes`, `PercentagesAreCutShort` |
| Bars fill yellow; the reward bar fills from its middle, red for punishment and green for reward | done | tests `BarsFillAndColour`, `BarFillRunsInsideTheFrame` |
| The panel sits on a see-through black box fading to its right, laid out by the screen's size | done | tests `LayoutWithTheReward`, `LayoutFollowingACreature` |
| While the camera follows a creature, the panel shows its three bars near the top of the screen, without the reward | done | test `LayoutFollowingACreature` |
| The labels and reward words come from the game's texts | done | `CreatureStatusPanel` names |
| The panel is drawn afresh each frame it is wanted, without fading in or out | done | `CreatureStatusPanel.h` |
| The hand's "Interact" tooltip shows over the player's own creature, awake | done | `creature_hand::ShowsInteractTip`; test `TheInteractTipIsForTheOwnCreatureOnly` |
| Not shown inside the temple | done | `Game.cpp` |

## Passing out

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature passes out when its damage, hunger or tiredness reaches 100% | done | tests `PassesOutWhenAStatusReachesAHundredPercent`, `PassingOutAgreesWithTheBodysFainting` |
| It is carried to its pen to come round: the home it was given, else by its player's temple | done | test `PenIsTheHomeThenTheTempleThenTheFallback`; see [home_and_pen.md](home_and_pen.md) |
| The player is told their creature has fainted and been taken home | todo | see [lessons_and_help.md](lessons_and_help.md) |
