# Creature animation

Every species shares one list of animations, from the creature spec file: moving, faces, sitting and sleeping, the
actions it plays once, looking, picking up, throwing, eating, fighting, dancing, rewards and punishments, recoils,
gestures on top of the body, pointing and catching. Each animation exists for the species' base mesh and may have its
own version for the evil, good, thin and fat meshes, blended as the body is.

**Progress: 38/56 done, 6 partial — 73%**

## Playing animations on the body

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Animations are known by their place in the creature spec file, which every species shares | done | `CreatureLayers.h`, `Data/ctrspec27.txt`; test `AnimationNames` |
| Each species' animations come from its body file, for its base, evil, good, thin and fat meshes | done | `CreatureRig`, loaded from `Data/CTR/*.cbn` |
| An animation is the base's pulled towards the evil or good and the thin or fat versions, keyframe by keyframe | done | `creature_animation::Blend`; tests `TranslationsBlendLinearlyOnBothAxes`, `RotationsBlendOnTheMatrices` |
| A variant without its own version moves as the base does, adjusted by how its stand differs | done | test `AVariantWithoutItsOwnMovesByItsStand` |
| A bone a variant doesn't move keeps that variant's stand | done | test `BonesAVariantDoesNotMoveTakeItsStand` |
| The rest pose blends as the body does | done | test `TheRestPoseBlendsAsTheBody` |
| Weak and strong bodies move as the base does | done | `CreatureAnimation.h` |
| Bigger creatures play their animations more slowly | done | test `BiggerCreaturesPlayMoreSlowly` |
| Animations can be played mirrored, the left side's bones swapped with the right's | done | `SkeletalLayers`; tests `MirrorBonesPairAcrossTheBody`, `AMirroredAnimationMovesTheOtherSide` |
| Changing animation blends smoothly from the last pose into the new one (unconfirmed how the game does it) | todo | each animation starts from its beginning without blending (`CreatureLayers.h`) |

## Layers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Standing, the creature breathes: once every five seconds at size 1, more slowly the bigger it is | done | tests `BreathingTakesFiveSecondsAtSizeOne`, `TheBreathingPeriodEasesToItsTarget` |
| Its breathing quickens fast and calms slowly | done | test `BreathingCalmsSlowlyAndQuickensFast` |
| An action plays once, then the body stands again; nothing new starts while one plays | done | test `AnActionPlaysOnceThenTheBodyStands` |
| Sitting, sleeping, pooing, being sick and casting are a start, a loop for as long as wanted, and an end | done | test `ASitStartsLoopsUntilToldThenEnds` |
| A loop can hold its last frame, as lying where it fell | done | `CreatureLayers` hold loop |
| A missing animation ends at once | done | test `AMissingAnimationEndsAtOnce` |
| The head turns on top of the body with the look animations | done | see [face_eyes_hair.md](face_eyes_hair.md) |
| The face plays on top of the body | done | see [face_eyes_hair.md](face_eyes_hair.md) |
| A gesture plays once on top of the body: nod, head shake, yawn, thirsty, squirting water, talking | partial | the layer works (test `AGesturePlaysOnce`); the yawn is used when idle, the others wait for the actions that use them |
| Walking and running blend with the stand by speed, keeping the feet on the ground | done | see [locomotion.md](locomotion.md) |
| Reaching blends four animations by where the thing lies | done | `creature_reach`; see [object_actions.md](object_actions.md) |
| Fights play their own states and moves at their own pace | done | see [fighting.md](fighting.md) |

## The animation set

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Moving: stand, walk, run, turning on the spot and stepping off at 0, 90 and 180 degrees each way | done | see [locomotion.md](locomotion.md) |
| A jump to the side (unconfirmed what plays it) | todo | |
| Sleep, poo, be sick, sit | done | the mind's agendas |
| Cast and cast up (the powered-up casting pose) | partial | the casting pose plays (see [creature_casting.md](creature_casting.md)); the powered-up pose is unconfirmed |
| Scatter (unconfirmed what plays it) | todo | |
| The actions played once: summon, angry, hungry, happy, sad, tired, hot, cold, scratch, frightened, sneeze, confused, feeling nice, impress, need a poo, feel playful, play, look at me, taunt, drink, friendly wave, embarrassed, pick me | partial | most are played by the mind's actions (`creature_plan_actions`); embarrassed and play have no action that plays them yet |
| Relaxed stand | todo | |
| Picking up front and back, left and right, holding, taking from the hand | partial | reaching and holding play; taking from the hand is todo (see [object_actions.md](object_actions.md)) |
| Throwing flat and high, tossing away, eating, putting down, lobbing gently | done | `creature_throw`; see [object_actions.md](object_actions.md) |
| Stroking, shaking, smelling and examining what it holds | done | `creature_object_actions::Kind::Keep` |
| Knocking things down front and back, left and right, and kicking low | done | `k_DestroyAnimations` |
| Fainting, getting up, powering up a blow, the creation pose (unconfirmed) | partial | faint, get up and power-up play; the creation pose doesn't |
| Fight start, stance, finish, blows at five heights and the special, twice over, steps, blocks | done | see [fighting.md](fighting.md) |
| Rewards for each part of the body stroked and punishments high, middle and low, hard and gentle | done | see [learning_from_feedback.md](learning_from_feedback.md) |
| Recoils from blows high, middle and low, to each side, top and bottom, and wobbles | done | see [fighting.md](fighting.md) |
| Pointing low and high, left and right | done | `k_PointAnimations` |
| Kissing high and low, howling, laughing, crying, aha, praying, rude gesture | todo | the actions that play them (see [friends_and_other_creatures.md](friends_and_other_creatures.md)) are todo |
| Dances: start, finish and five dance moves | todo | see [town_actions.md](town_actions.md) |
| Catching: stepping right, back and front, catching high and low to each side | partial | The four catch clips are blended (`CreatureCatch`) and the ready phase side-steps with clip 225, mirrored for the left (`cf273ae0`); open: the step is always taken (R20: the game refuses it while moving or into a route obstacle) |
| Three animations reserved for scripts | todo | |

## Animation events

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Sounds are placed on moments of animations: footsteps as feet land, roars as the mouth opens, snores | done | `creature_audio`; tests `EventsFireOnceAcrossFrames`, `LoopWrapsWithoutDoubleFiring` |
| Mirrored animations pass the same events | done | test `MirroredAnimationPassesTheSameEvents` |
| Sounds don't depend on the frame rate | done | test `SoundsDoNotDependOnTheFrameRate` |
| A sound is picked by the creature's size, species, the ground under it and the action | done | tests `SizeKey`, `SurfaceKey`, `TerrainMaterial`, `KeysInBankOrder` |
| Voices come from the species' own bank; other players' creatures are heard in their voices only when a script allows | done | `CreatureAudioSystem`; test `VoiceBank`; see [../audio](../audio/) |
| Footsteps, blows, snores, eating and drinking come from the bank all creatures share | done | `creature_audio::EventKind::Generic` |
| Things are taken hold of and let go at a moment of the animation particular to the species | done | `CreatureObjectActionSystem` |
| Footprints are laid as feet land | done | see [locomotion.md](locomotion.md) |
| Scripts can turn a creature's sounds on or off | todo | `SET_CREATURE_SOUND` is a stub |

## Scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts can play a static or individual animation on a creature | todo | the creature-animation script functions are stubs |
| Scripts can override a thing's state animation | todo | `OVERRIDE_STATE_ANIMATION` is a stub |
| Scripts can turn the animation speed changes on or off | todo | `SET_ANIMATION_MODIFY` is a stub |
| Scripts can play a gesture | todo | `PLAY_GESTURE` is a stub |
| The debug spawner plays any animation, face or gesture | done | `CreatureSpawner.cpp` (openblack only) |
