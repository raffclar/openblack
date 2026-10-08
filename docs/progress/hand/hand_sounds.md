# Hand sounds

Every sound the god hand makes or sets off: gripping the land and the sea, picking things up and putting them down,
tapping, holding and shaking off miracles, crossing influence borders, and the voices of the villages answering what the
hand does. The sound system itself (banks, 3D placement, music) is in [../audio/](../audio/).

**Progress: 15/38 done, 2 partial — 42%**

## Gripping the land and the sea

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Gripping the land plays one of six land-grab sounds, picked at random | done | `Game::PlayHandGrabSound` |
| The land-grab sound is heard flat, not placed in the world | done | `Game::PlayHandGrabSound` (no position) |
| Gripping the sea plays the ten water sounds one after another, in turn | done | `Game::PlayHandGrabSound` |
| The water sound is placed on the water's surface where the hand went in | done | `Game::PlayHandGrabSound` (just above sea level) |
| Gripping is silent while a script holds the cinema bars | todo | (unconfirmed which scripted moments mute it) |

## Picking up and putting down

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Picking an object up plays the pick-up sound | done | `HandGrabSystem::Take` (sample 10) |
| Taking food from a store or field plays the food pick-up sound | todo | |
| Taking wood from a store or forest plays the wood pick-up sound | todo | |
| Food or wood let fall from the hand lands with a thud sized by the amount: a small or a big pile sound | partial | the thud is done for miracle piles (`MagicResources.cpp`, `piles::PileSoundSample`); not for piles from the hand |
| Putting a tree back in the ground plays a planting sound, one of three | todo | |
| Pulling a tree out of the ground plays a tree breaking sound | todo | see [tug.md](tug.md) |
| A scaffold taken into the hand plays its appearing sound, and its vanishing sound when it leaves | todo | |
| Something thrown at another thing plays an impact sound where it hits | todo | (unconfirmed which sample) |
| A thrown rock or fireball whooshing past the camera plays one of five rock or fireball whooshes | partial | Any body in the physics passing within 10 m of the camera faster than 20 m/s plays one of the five rock whooshes (`DynamicsSystem`, `turn::` rules); the fireball's whoosh isn't checked |
| Dropping an animal from a height squashes it with one of four squash sounds | todo | (unconfirmed it is the hand's drop that plays it) |

## Tapping

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Knocking on houses, tapping rocks and scaffolds, opening reward chests | todo | the sounds of each tap are listed in [clicking_and_activating.md](clicking_and_activating.md) |
| Tapping a one-shot miracle bubble pops it | done | `MagicSystem.cpp` (bubble pop) |
| Tapping a leash post clicks | done | `LeashSystem.cpp` |
| Opening a reward chest plays the chest and reward stings | todo | |
| Tapping a script's highlighted marker plays its chime | todo | |

## Miracles in the hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Bands flying onto the hand play the power-up band sound | done | `MiracleFxSystem::SeedInHand` |
| A held miracle hums in a loop that follows the hand | done | `MagicSystem::StartHoldLoop` |
| Shaking a miracle off plays the shake sound | done | `MagicSystem.cpp` |
| A miracle that can't be cast where it is let go plays the failure sound | done | `MagicSystem.cpp` |
| The announcer names the power-up the hand has reached | done | `visuals::PowerUpVoiceSample` |
| A recognised gesture plays its sound | done | see [../gesture/gesture_effects.md](../gesture/gesture_effects.md) |

## Influence and worship

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand crossing an influence border plays a sound once, at the hand, while the game runs | done | `InfluenceSystem.cpp` |
| Moving a village totem plays its grinding sound | todo | see [totem.md](totem.md) |
| The totem's raising and lowering are followed by the guidance sounds while it moves | todo | |
| The local player hears a heartbeat as the guidance's warning | todo | see [../audio/](../audio/) |

## The villages answering the hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A deed of the hand that impresses a village sets off a group cheer, louder the more the player stands out from the other gods | todo | see [../worship/](../worship/) |
| A deed that loses belief sets off a group groan, by the same measure | todo | |
| Making a disciple has the village call out the job given, at most so often | todo | see [../villager/](../villager/) |
| Dropping food or wood into a town makes the town answer, by how much it had: plenty, some or little | todo | see [../town/](../town/) |
| A villager dying in the hand or by its throw sets off the death cries | todo | see [../villager/](../villager/) |

## The creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Stroking and slapping the creature has it make the sounds of its reaction | done | `CreatureAudioSystem` (reaction animations' sounds); see [../creature/](../creature/) |
| A glow on the creature's hands carries a looping glow sound | todo | see [hand_effects_and_glows.md](hand_effects_and_glows.md) |
| Giving the creature something plays the sound of it taking it | todo | see [creature_contact.md](creature_contact.md) |
