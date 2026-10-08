# Placement

Where the god hand sits in the world, how large it is drawn and which way it faces. The game keeps the hand under the
cursor at all times, hanging over the land, the sea, the object or the temple wall the cursor points at.

**Progress: 22/29 done, 1 partial — 78%**

## Where the hand sits

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand's fingertips lie on the line of sight through the cursor, so it is always under the cursor on screen | done | `Game::PlaceHand` |
| It is pulled back from the land towards the camera by its own height, so its fingers hang down to the land | done | `Game::PlaceHand` (hand height 3.2 at standard size) |
| Over the sea it rests on the water | done | `Game::PlaceHand` (sea level) |
| It eases out to land further from the camera slowly and in to nearer land quickly | done | `Game::PlaceHand` (0.28 s out, 0.1 s in) |
| It is kept between 2 units and the hand's reach from the camera | done | `Game.h` (limits), reach from `CameraHelp` |
| With nothing under the cursor (the sky) it keeps its distance and stands upright | done | `src/Game.cpp` (Update) |
| Over an object it hangs where the line of sight meets the object's own mesh, standing on that face | partial | openblack meets the object's physics body, not its mesh, then pulls back as over land |
| Holding something over an object, it is pulled back further by half the size of what it holds, and more near a creature | todo | The hand holds objects now (`HandGrabSystem`); its placement doesn't pull back for them yet |
| Over a worship icon it hangs at the icon, a set distance short of it | todo | not done |
| Gripping the land it stays on the gripped point and moves with it | done | `Game::PlaceHand` |
| Dragging by the edge it holds its distance from the camera over 0.4 seconds, never past the land | done | `Game::PlaceHand` |
| It moves by the frame's real time, or by the game's time while a script holds the cinema bars | done | `Game::HandStepSeconds` |
| Its speed and direction of movement are measured each frame for throws and spins | done | `src/Magic/HandMotion.cpp` |
| Held to a creature it rests on the creature's body under the cursor | done | `CreatureHandSystem`; see [creature_contact.md](creature_contact.md) |
| In the temple it hangs a little short of the room's surface and turns to face it | done | `Game::PlaceHand`, `Game.cpp` (temple normal); see [../temple/](../temple/) |
| Scripts can point the hand and camera at the player's temple | todo | not done |
| Scripts can hold the hand at a set place | todo | only testbed scenarios can (`MagicSystem::GetDrivenHand`) |

## Size and facing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand is scaled so it spans 3.2 units, growing past 150 units from the camera so it keeps about the same size on screen | done | `HandAnimation::SizeAtDistance`; test `HandAnimationTest.StandardSizeSpansThreeUnitsTwo` |
| It faces along the line of sight through the cursor, laid level | done | `src/3D/HandOrientation.cpp`; tests `HandOrientation.*` |
| Looking straight down it keeps its heading | done | test `HandOrientation.KeepsItsHeadingLookingStraightDown` |
| Its up eases over 0.4 seconds to the slope of the land, or the face of the object, under it | done | `Game::OrientHand`; test `HandOrientation.OnASlopeItsUpIsTheSlopesAndItsFrontTheHeadingLaidAlongIt` |
| The slope is only taken afresh when the cursor moves across the screen | done | `Game::OrientHand` |
| Dragging the land it holds its up, and stands straight up when let go | done | `Game::OrientHand` |
| A right hand is the left hand's mesh mirrored | done | `src/Game.cpp` (scale), `EngineConfig::rightHandedHand` |
| The player picks a left or right hand in the options | done | `src/Gui/GameMenu.cpp` (left-handed setting), `Game::HandleInterfaceAction` |
| Holding a miracle or pouring food and wood lifts and tips the hand | done | `src/Magic/HandHoldPoser.cpp`; see [../miracles/](../miracles/) |

## Showing and hiding

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand is hidden while the menu is open and while a script's cinematic has the interface | done | `src/Game.cpp` (drawHand) |
| Scripts can make the hand invisible and bring it back | todo | (unconfirmed which commands do it) |
| Where the hand can't be drawn, a plain mouse pointer is drawn instead | todo | (unconfirmed when the game falls back to it) |
