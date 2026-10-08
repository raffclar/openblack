# States overview

The god hand is always in exactly one state, picked afresh every frame from what the interface is doing and what the hand
holds. There are eleven: hidden, normal, camera, tugging, holding, totem, scooping, creature, holding a miracle, playing
a set animation, and the temple. Each state places, turns and animates the hand its own way; changing state blends the
hand from where it was drawn.

**Progress: 19/39 done, 7 partial — 58%**

## Choosing the state

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The state is chosen again every frame, before the hand is placed and drawn | partial | openblack decides each frame in `Game::UpdateHandNavigation` and the creature and magic systems, but has no single state machine |
| Inside the temple the hand is always in the temple state | done | `Game::PlaceHand` (temple branch); see [temple_hand.md](temple_hand.md) |
| Holding a miracle's seed puts it in the miracle state | done | `MagicSystem`, `HandHoldPoser`; see [pouring.md](pouring.md) |
| Holding something being pulled out of the ground puts it in the tugging state | todo | see [tug.md](tug.md) |
| Holding anything else puts it in the holding state | done | `HandGrabSystem` (holding, ready to throw, pulling and scooping states on `HandGrab`) |
| After the hand lets go of something, it waits about 180 milliseconds before going back to normal | todo | (unconfirmed what the wait is for) |
| Working a village's totem puts it in the totem state | todo | see [totem.md](totem.md) |
| Interacting with a creature puts it in the creature state, if a creature is under the hand, else it stays normal | done | `CreatureHandSystem`; see [creature_contact.md](creature_contact.md) |
| A set animation started by a tap (knocking on a house) puts it in the play-animation state until the animation ends | todo | see [hand_animations.md](hand_animations.md) |
| While the interface is switched off (cinematics, some script moments) the hand is hidden | partial | hidden with the menu and while a script's cinematic has the interface (`src/Game.cpp`); no other interface-off cases |
| Otherwise it is in the camera state while a drag of the camera is under way, and the normal state when not | done | `Game::UpdateHandNavigation` |
| Scooping several things up puts it in the scooping state | todo | see [multi_pickup.md](multi_pickup.md) |

## Changing state

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| On a change, the old state is left and the new one entered, each resetting what it eases | partial | openblack resets its zoomers per case in `Game::PlaceHand`, not per state |
| Every change of state blends the hand's bones and place from where they were drawn over 0.13 seconds | done | `src/3D/HandCrossFade.h`, `HandAnimation::StartFade`; tests `HandCrossFade.*` |
| A change of animation inside a state blends the same way | done | test `HandAnimationTest.GrippingCrossFadesToTheGripCycle` |
| Entering the normal state starts the hand's up straight and its distance where it was | done | `Game::OrientHand`, `Game::PlaceHand` |
| Entering the hidden state keeps the last drawn bones, so the hand blends back in from them | todo | |
| Leaving the creature state tells the creature how it was treated, held between -1 and 1 | done | `CreatureHandSystem::Release` |
| Entering the creature interaction pushes a close camera on the creature, and leaving it pops the camera back | todo | see [../camera/](../camera/) |
| A creature interaction ended within 450 milliseconds counts as a click on the player's own creature | done | `creature_hand::WasClick` (openblack's own threshold; unconfirmed it matches) |

## What every state shares

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A spring-damped copy of the cursor trails it by at most 80 pixels on each axis, with a stiffness of 20 | done | `HandAnimation::StepCursorSpring` |
| The trailing gap leans the hand sideways and back and forth through its lean animations, unless smaller than a ten-thousandth | done | `HandAnimation` (minimum lean) |
| The hand's alignment is held between -1 and 1 and its morph updated every frame | done | `HandSystem::UpdateAlignmentMorph`; see [look_and_morph.md](look_and_morph.md) |
| The hand's time runs on the camera's clock, or the frame's while in the temple | partial | openblack uses real time, or game time under a script's cinema bars (`Game::HandStepSeconds`) |
| The hand points along the line of sight through the cursor, turned only when the cursor has moved | done | `Game::OrientHand` |
| Whether the camera's edge hints show depends on the state: the normal, camera and holding states allow them | partial | openblack drops them over creatures and objects only |
| The hand can be swapped between left and right at any time | done | `EngineConfig::rightHandedHand` |
| The hand's state is saved and loaded with the game | todo | see [../engine/](../engine/) |

## Each state

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Hidden: nothing is drawn and nothing moves | partial | see above |
| Normal: hovering under the cursor, feeling objects, offering camera hints at the edges | partial | hover and hints done; the feeling animation over objects is not (see [hand_animations.md](hand_animations.md)) |
| Camera: gripping or dragging the land or the edges | done | see [navigation.md](navigation.md) |
| Tugging: pulling at something rooted until it comes free | todo | see [tug.md](tug.md) |
| Holding: carrying objects and people | todo | see [holding.md](holding.md) |
| Totem: holding a village's totem and sliding it | todo | see [totem.md](totem.md) |
| Scooping: taking up several things at once | todo | see [multi_pickup.md](multi_pickup.md) |
| Creature: held to a creature's body | done | see [creature_contact.md](creature_contact.md) |
| Holding a miracle, and pouring | done | see [pouring.md](pouring.md) |
| Playing a set animation at a fixed place | todo | see [hand_animations.md](hand_animations.md) |
| Temple | done | see [temple_hand.md](temple_hand.md) |
