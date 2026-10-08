# Gesture recognition

The game records the path the hand's cursor takes across the screen and compares it with a file of gesture templates
to tell when the player has drawn a shape such as a circle, a spiral or a scribble. It only ever checks for the
gestures it is waiting for at that moment.

**Progress: 30/34 done, 1 partial — 90%**

## The gesture templates

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The templates are read once at start-up from `Data\Gestures.jty` | done | `components/gestures` (GestureFile), loaded through the resource cache by `src/ECS/Systems/Implementations/GestureSystem.cpp`; test `GestureFile.ReadsTheGamesTemplates` |
| The file holds 81 templates for 23 gesture kinds, several templates per kind (ten hearts, eleven R shapes, nine threes …) so loose drawings still match | done | `components/gestures`; test `GestureMatcher.TheGamesTemplatesRecogniseTheirOwnShapes` |
| The square spiral and square wave templates added for Creature Isle are in the file and recognised | done | The square spiral is the leash gesture in the original game too; the square wave is only used by a hidden script (see [leash_gestures.md](leash_gestures.md)) |
| Each template stores its points normalised to a unit square, the turn at each point and the heading leaving it in eighths of a turn | done | `components/gestures/include/GestureFile.h` |
| Each template says whether it may be drawn mirrored (only the circle and the star), whether the starting direction matters and whether the shape's width against its height matters | done | `src/Gestures/GestureMatcher.cpp` |
| The template file can be written back, as the developers' in-game recording tool did | partial | `GestureFile::Write` round-trips the file (test `GestureFile.WritesAndReadsBackTheSameTemplates`); there is no tool to record new templates by drawing them in game |

## Recording the hand's path

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The cursor is sampled about 34 times a second (every 28 ms), at the game's pace rather than the frame rate | done | `GestureSystem::Update`; test `GestureSystem.SamplesTheCursorAtTheGamesPace` |
| No mouse button needs to be held: the path is recorded whenever the cursor moves | done | `src/ECS/Systems/Implementations/GestureSystem.cpp` |
| The last 80 samples are kept, older ones fall off | done | `src/Gestures/GestureRecorder.h`; test `GestureRecorder.KeepsTheLastEightyPoints` |
| Each sample also remembers the point of land under it, so a gesture can be placed in the world | done | `GestureRecorder::AddPoint` |
| Over the sky or off the land, the last land point is kept with the new screen point | done | test `GestureRecorder.OverNothingTheLastLandPointStandsIn` |
| Holding the cursor still for about two seconds (70 identical samples) forgets the path | done | test `GestureRecorder.HeldStillItForgetsThePath` |
| Corners are found as the path is drawn: the sample where the path turns most since the last corner, if it turns at least about 17 degrees | done | `GestureRecorder::FindCorner`; test `GestureRecorder.ARightAngleIsACorner` |
| Samples closer than 4 pixels on both axes count as the same place | done | `src/Gestures/GestureRecorder.h` |
| A corner too close to the one before (under 12 pixels, or 4 pixels when the last stretch is small) is merged into it, keeping the sharper turn | done | `GestureRecorder::MergeCorner` |
| Each corner records the turn from the way in to the way out, and the way out as one of eight directions | done | test `GestureAngles.HeadingsTurnsAndDirections` |
| Pixel distances are measured on the actual screen | done | Deliberate deviation: openblack measures on a screen scaled to 768 pixels high so gestures feel the same at any resolution; identical at 1024×768 (audit of the casting group) |
| After a gesture is recognised the path is forgotten and nothing is recorded for 0.4 seconds | done | `gesture::k_RecognisedPauseSeconds` in `src/Gestures/GestureRequests.h` |
| Moving the camera forgets the path being drawn, except while the camera is shaking | done | `GestureSystem::Update`; test `GestureSystem.MovingTheCameraWipesThePathUnlessItShakes` |
| A camera that follows something, or the camera of a creature fight in an arena, lets the player keep drawing while it moves | todo | Neither camera exists yet (see ../camera/) |
| Nothing is recorded while the hand is not over the world: in the temple, in cut scenes, with the interface off | done | `Frame::overWorld` in `GestureSystemInterface.h`; test `GestureSystem.NothingIsRecordedUntilTheCursorIsOverTheLand` |
| Pressing the button during a creature fight forgets the path | todo | No fight interface for the player yet |
| Starting a cast on release or a power-up starts the path again from the current point | todo | The path is only forgotten after a recognition, a camera move or leaving the world |

## Matching a path with the templates

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Only the gestures being waited for are checked, in a fixed order; the first one recognised is acted on and there is no best-match search | done | `src/Gestures/GestureRequests.cpp` (`Requests`), `GestureSystem::Update` |
| A gesture's templates are tried in the file's order and the first that fits is taken | done | `gesture::Recognise` in `src/Gestures/GestureMatcher.cpp` |
| A template fits when the turns at the path's corners add up to the template's turns, never straying by more than about 34 degrees on the way | done | `GestureMatcher::MatchTemplate`; test `GestureMatcher.ADifferentShapeIsntRecognised` |
| A small turn (under about 30 degrees) on either side may be skipped when that brings the two closer | done | `gesture::k_SmallTurn` |
| The match may start at any corner of the path, so a stroke drawn before the gesture does not spoil it | done | `GestureMatcher::MatchTemplate` |
| Templates that check direction need the path to set off the same way as the template | done | `src/Gestures/GestureMatcher.cpp` |
| A circle or a star drawn the other way round is recognised as its mirror image | done | test `GestureMatcher.ACircleTheOtherWayRoundIsItsMirrorImage` |
| Templates that check their shape need the path to be about as wide against its height (very tall, very wide, or in between) | done | `AspectFits`; tests `GestureMatcher.AspectBands`, `GestureMatcher.RecognisesAScribbleButNotATallOne` |
| A gesture can be drawn anywhere on the screen and at any size | done | test `GestureMatcher.RecognisesACircleAnywhereAtAnySize` |
| A recognised gesture is placed on the land under the middle of the box round the samples it was drawn with | done | `GestureMatcher::GestureBox`, `GestureSystem` |
| A circle's size is the half width of its box, measured across the land at the depth of its middle, a little enlarged | done | `gesture::CircleRadius`; test `GestureMatcher.ACirclesPlaceAndSize` |
| Recognised gestures are sent to the other players in a network game with where they were drawn | n/a | No network play (see ../multiplayer/) |
