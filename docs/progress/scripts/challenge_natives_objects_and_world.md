# Challenge natives: objects, villagers and the world

The challenge scripts' functions for objects in the world: finding, creating, moving and deleting them, their properties and script states, flocks, containers, fire, clicks and hits, special effects, mist, games and walking paths. The game has 464 of these functions in all; the language statement each comes from is shown in italics, and "called" counts are calls in the shipped `challenge.chl`. How the virtual machine runs them is in [../engine/script_vm.md](../engine/script_vm.md); what each challenge is about is in [../story/](../story/).

**Progress: 7/122 done, 7 partial — 9%**

## Used by the shipped scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Whether the player has clicked an object: *‹object› clicked* (called 84 times in 37 scripts) | todo | `GameThingClicked` logs "not implemented" |
| Puts a villager, animal or other living thing into one of its script states (walking to a place, dancing, sitting and so on): *state ‹object› ‹state›* (called 893 times in 150 scripts) | todo | `SetScriptState` logs "not implemented" |
| Puts a living thing into a script state that takes a position: *state [state] [position] (a state statement that takes a position)* (called 20 times in 16 scripts) | todo | `SetScriptStatePos` logs "not implemented" |
| Puts a living thing into a script state that takes a number with a fraction: *state [state] [number] (a state statement that takes a fraction)* (called 20 times in 16 scripts) | todo | `SetScriptFloat` logs "not implemented" |
| Puts a living thing into a script state that takes a whole number: *state [state] [number] (a state statement that takes a whole number)* (called 862 times in 140 scripts) | todo | `SetScriptUlong` logs "not implemented" |
| Reads one of an object's properties (health, age, food, wood, altitude, belief, scale, speed and dozens more): *‹prop› of ‹object›* (called 1421 times in 257 scripts) | todo | `GetProperty` logs "not implemented" |
| Changes one of an object's properties: *‹prop› of ‹object› = ‹val›* (called 636 times in 175 scripts) | todo | `SetProperty` logs "not implemented" |
| Gives an object's position: *[ ‹obj id› ]* (called 4112 times in 351 scripts) | partial | `GetPosition`: the object's position from its transform; objects openblack never creates give the origin |
| Moves an object to a position at once: *set ‹obj id› position to ‹position›* (called 127 times in 54 scripts) | partial | `SetPosition`: moves the object and snaps it to the ground; the game's handling of heights and of living things' states unconfirmed |
| Finds an object of a kind at a position: *get ‹type› [‹subtype›] at ‹position› [excluding scripted]* (called 82 times in 43 scripts) | todo | `Call` logs "not implemented" |
| Creates an object of a kind (villager, animal, rock, building, toy, marker and so on) at a position: *marker ‹type› ‹subtype› at ‹position›* (called 1818 times in 264 scripts) | partial | `Create`: rocks, mobile statics and the football (which joins the nearest town's playthings, `dc312635`) are created; every other kind gives no object (nought) |
| Makes a living thing walk to a position, ending within a radius of it: *move ‹object› position to ‹position› [radius ‹radius›]* (called 528 times in 156 scripts) | todo | `MoveGameThing` logs "not implemented" |
| Turns an object to face a position: *set ‹object› focus to ‹position›* (called 601 times in 138 scripts) | todo | `SetFocus` logs "not implemented" |
| Creates an empty flock at a position: *flock at ‹position›* (called 29 times in 20 scripts) | todo | `FlockCreate` logs "not implemented" |
| Adds an animal or villager to a flock, optionally as its leader: *attach ‹obj› to ‹flock› [as leader]* (called 102 times in 43 scripts) | todo | `FlockAttach` logs "not implemented" |
| Takes a member out of a flock: *detach [‹obj›] from ‹flock›* (called 18 times in 10 scripts) | todo | `FlockDetach` logs "not implemented" |
| Breaks a flock up: *disband ‹flock›* (called 9 times in 7 scripts) | todo | `FlockDisband` logs "not implemented" |
| Gives how many things a flock, town or container holds: *size of ‹container›* (called 56 times in 19 scripts) | todo | `IdSize` logs "not implemented" |
| Whether an object is in a flock: *‹obj› in ‹flock›* (called 3 times in 2 scripts) | todo | `FlockMember` logs "not implemented" |
| Gives the position of the player's hand in the world: *hand position* (called 11 times in 5 scripts) | partial | `GetHandPosition`: always the first hand's position |
| Deletes an object, plainly, fading it away, exploding it or with the temple's explosion: *delete ‹obj› with fade/with explode/with temple explode* (called 376 times in 121 scripts) | todo | `ObjectDelete` logs "not implemented" |
| Finds an object of a kind within a radius of a position: *get ‹type› [‹subtype›] at ‹position› radius ‹radius› [excluding scripted]* (called 159 times in 70 scripts) | todo | `CallNear` logs "not implemented" |
| Starts a special effect (sparkles, smoke, explosions and so on) at a position for a time: *create special effect ‹effect› at ‹position› [time ‹duration›]* (called 132 times in 49 scripts) | todo | `SpecialEffectPosition` logs "not implemented" |
| Starts a special effect on an object for a time: *create special effect ‹effect› on ‹target› [time ‹duration›]* (called 62 times in 17 scripts) | todo | `SpecialEffectObject` logs "not implemented" |
| Makes villagers perform a dance around a place for a time: *make ‹obj› dance ‹type› around ‹position› [time ‹duration›]* (called 2 times in 2 scripts) | todo | `DanceCreate` logs "not implemented" |
| Finds an object of a kind inside a town, flock or container: *get ‹type› [‹subtype›] in ‹container› [excluding scripted]* (called 24 times in 13 scripts) | todo | `CallIn` logs "not implemented" |
| Whether a living thing has finished the animation or state the script gave it: *‹obj› played* (called 319 times in 97 scripts) | todo | `Played` logs "not implemented" |
| Finds an object of a kind inside a town or container and within a radius of a position: *get ‹type› [‹subtype›] in ‹container› at ‹pos› radius ‹radius› [excluding scripted]* (called 3 times in 3 scripts) | todo | `CallInNear` logs "not implemented" |
| Makes a living thing play an animation instead of its state's own: *set ‹obj› anim ‹anim type›* (called 56 times in 21 scripts) | todo | `OverrideStateAnimation` logs "not implemented" |
| Makes villagers around an object react to it (run away, gather round and so on) (unconfirmed): *attach reaction ‹object› ‹reaction›* (called once in 1 script) | todo | `CreateReaction` logs "not implemented" |
| Gives the text of what the hand would do with an object: *get action text for ‹obj›* (called 2 times in 2 scripts) | todo | `GetActionTextForObject` logs "not implemented" |
| Fills a town, flock or container with things of a kind: *populate ‹obj› with ‹quantity› ‹type› [‹subtype›]* (called 9 times in 8 scripts) | todo | `PopulateContainer` logs "not implemented" |
| Throws an object off along a heading at a speed: *set ‹value› velocity heading ‹value› speed ‹value›* (called 3 times in 3 scripts) | todo | `SetHeadingAndSpeed` logs "not implemented" |
| Whether an object is blown about by the wind: *enable/disable ‹object› affected by wind* (called 17 times in 10 scripts) | todo | `SetAffectedByWind` logs "not implemented" |
| Finds an object of a kind inside a town or container but not near a position: *get ‹type› [‹subtype›] in ‹container› not near ‹pos› radius ‹radius› [excluding scripted]* (called once in 1 script) | todo | `CallInNotNear` logs "not implemented" |
| Gives the state a living thing is in: *state of ‹obj›* (called 2 times in 2 scripts) | todo | `GetObjectState` logs "not implemented" |
| Gives the land's height at a position: *land height at ‹position›* (called 9 times in 4 scripts) | done | `GetLandHeight`: the land's height |
| Forgets the object the player last clicked: *clear clicked object* (called 43 times in 16 scripts) | todo | `ClearClickedObject` logs "not implemented" |
| Hands an object back to the game after a script has controlled it (it goes back to its own life): *release ‹obj›* (called 216 times in 96 scripts) | partial | `ScriptObjectsSystem::ReleaseFromScript` (`410656e9`, `d39f8402`): control is cleared; a villager standing in the map is set to decide what to do as a script sets it (its state kept as previous, its wait reset), one in the physics decides once out; a creature gives up its action; open: the villager's own release (its flock, town or the homeless), an animal's (wander, its flock), a script container's contents |
| Gives how many poisoned things a container holds: *poisoned size of ‹container›* (called once in 1 script) | todo | `IdPoisonedSize` logs "not implemented" |
| Whether an object is poisoned: *‹obj› poisoned* (called 2 times in 1 script) | todo | `IsPoisoned` logs "not implemented" |
| Finds an object of a kind in a container that isn't poisoned: *get not poisoned ‹type› [‹subtype›] in ‹container› [excluding scripted]* (called once in 1 script) | todo | `CallNotPoisonedIn` logs "not implemented" |
| Whether the player can move an object: *enable/disable ‹obj› moveable* (called 97 times in 37 scripts) | todo | `SetIdMoveable` logs "not implemented" |
| Whether the player can pick an object up: *enable/disable ‹obj› pickup* (called 145 times in 55 scripts) | todo | `SetIdPickupable` logs "not implemented" |
| Whether an object is on fire: *‹obj› on fire* (called 3 times in 2 scripts) | done | `IsOnFire`: fire audit: OK |
| Whether there is fire within a radius of a position: *fire near ‹position› radius ‹radius›* (called 3 times in 3 scripts) | done | `IsFireNear`: fire audit: OK |
| Poisons an object (food) or clears the poison: *enable/disable ‹obj› poisoned* (called 5 times in 1 script) | todo | `SetPoisoned` logs "not implemented"; what poison does is in [../resources/poison_and_mushrooms.md](../resources/poison_and_mushrooms.md) |
| Sets an object's temperature (fire spreads by temperature): *set ‹obj› temperature ‹temperature›* (called 5 times in 3 scripts) | done | `SetTemperature`: fire audit: OK |
| Sets an object on fire at a burning speed, or puts it out: *enable/disable ‹object› on fire ‹burn speed›* (called 36 times in 12 scripts) | done | `SetOnFire`: fire audit: OK |
| Sends an object towards a target over a time: *set ‹obj› target ‹position› time ‹time›* (called 8 times in 8 scripts) | todo | `SetTarget` logs "not implemented" |
| Makes a villager walk one of the paths laid in the land, forwards or backwards between two points: *set ‹object› forward/reverse walk path ‹camera enum› from ‹val from› to ‹val to›* (called 4 times in 2 scripts) | todo | `WalkPath` logs "not implemented" |
| Whether an object is of a kind: *‹object› type ‹type› [‹subtype›]* (called 10 times in 9 scripts) | todo | `IsOfType` logs "not implemented" |
| Forgets the object last hit: *clear hit object* (called 5 times in 3 scripts) | todo | `ClearHitObject` logs "not implemented" |
| Whether an object has been hit: *‹object› hit* (called 5 times in 4 scripts) | todo | `GameThingHit` logs "not implemented" |
| Gives the object a player's hand or a creature is holding: *get held by ‹value›* (called 24 times in 16 scripts) | todo | `GetObjectHeld199` logs "not implemented" |
| Lets a living thing's animation be sped up or slowed down: *enable/disable ‹creature› anim time modify* (called 2 times in 1 script) | todo | `SetAnimationModify` logs "not implemented" |
| Gives the kind of an object: *get ‹object› type* (called once in 1 script) | todo | `GameType` logs "not implemented" |
| Gives the sub-kind of an object (which animal, which building and so on): *get ‹object› sub type* (called 10 times in 9 scripts) | todo | `GameSubType` logs "not implemented" |
| Creates an object of a kind at a position with an angle and a scale: *create with angle ‹angle› and scale ‹scale› ‹type› [‹subtype›] at ‹position›* (called 93 times in 29 scripts) | partial | `CreateWithAngleAndScale`: only rocks and mobile statics are created, with the angle and scale; other kinds give nought |
| Turns an object (a dispenser, a gate and so on) on or off: *enable/disable ‹object› active* (called 26 times in 22 scripts) | todo | `SetActive` logs "not implemented" |
| Whether an object still exists: *‹obj id› exists* (called 388 times in 145 scripts) | partial | `ThingValid`: true for any live entity; the game's own test (the object still alive in its world) unconfirmed |
| Stops villagers reacting to an object in one way (unconfirmed): *detach reaction ‹object› ‹reaction›* (called once in 1 script) | todo | `RemoveReactionOfType` logs "not implemented" |
| Gives how much of its animation a living thing has played: *get ‹object› played percentage* (called 2 times in 1 script) | todo | `PlayedPercentage` logs "not implemented" |
| Creates a patch of mist of a colour, size and transparency: *create mist at ‹pos› scale ‹scale› red ‹r› green ‹g› blue ‹b› transparency ‹transparency› height ratio ‹height ratio›* (called once in 1 script) | todo | `CreateMist` logs "not implemented" |
| Fades mist from one size and transparency to another over a time: *set ‹mist› fade start scale ‹start scale› end scale ‹end scale› start transparency ‹start transparency› end transparency ‹end transparency› time ‹duration›* (called once in 1 script) | todo | `SetMistFade` logs "not implemented" |
| Gives the object a player's hand or a creature is holding (second form): *get held by ‹creature›* (called 4 times in 2 scripts) | todo | `GetObjectHeld273` logs "not implemented" |
| Draws an object in full detail however far away it is: *enable/disable ‹object› high graphics detail* (called 333 times in 66 scripts) | todo | `SetHighGraphicsDetail` logs "not implemented" |
| Turns a villager into a skeleton or back: *enable/disable ‹object› skeleton* (called 9 times in 6 scripts) | todo | `SetSkeleton` logs "not implemented" |
| Gives a spot visual (a glow or mark) a target position: *add ‹object› target at ‹position›* (called 6 times in 3 scripts) | todo | `AddSpotVisualTargetPos` logs "not implemented" |
| Gives a spot visual a target object: *add ‹object› target on ‹target›* (called 7 times in 6 scripts) | todo | `AddSpotVisualTargetObject` logs "not implemented" |
| Makes an object impossible to destroy, or not: *enable/disable ‹object› indestructible* (called 98 times in 42 scripts) | todo | `SetIndestructable` logs "not implemented" |
| Turns a living thing to keep looking at an object: *set ‹object› focus on ‹target›* (called 2 times in 1 script) | todo | `SetFocusOnObject` logs "not implemented" |
| Lets a living thing stop looking at an object: *release ‹creature› focus* (called 4 times in 3 scripts) | todo | `ReleaseObjectFocus` logs "not implemented" |
| Whether an immersion (force feedback) effect exists: *immersion exists* (called 2 times in 2 scripts) | todo | `ImmersionExists` logs "not implemented" |
| Opens or closes an object (a chest, a gate, a phone box): *open/close ‹object›* (called 12 times in 6 scripts) | todo | `SetOpenClose` logs "not implemented" |
| Finds a living thing of a kind in a state within a radius: *get ‹type› [‹subtype›] in state ‹state› at ‹position› radius ‹radius› [excluding scripted]* (called once in 1 script) | todo | `CallNearInState` logs "not implemented" |
| Gives an object's information bits (unconfirmed): *get ‹object› info bits* (called once in 1 script) | todo | `ObjectInfoBits` logs "not implemented" |
| Whether fire hurts an object: *enable/disable ‹object› hurt by fire* (called 44 times in 15 scripts) | done | `SetHurtByFire`: fire audit: OK |
| Starts a special effect made for the game's cut scenes: *start jc special ‹feature›* (called 10 times in 2 scripts) | todo | `PlayJcSpecial` logs "not implemented" |
| Whether an object is locked in an interaction: *‹object› locked interaction* (called 5 times in 4 scripts) | todo | `IsLockedInteraction` logs "not implemented" |
| Turns a cut-scene special effect on an object on or off: *enable/disable jc special ‹feature› on ‹target›* (called 35 times in 8 scripts) | todo | `ThingJcSpecial` logs "not implemented" |
| Whether a villager is male: *‹object› is male* (called once in 1 script) | todo | `SexIsMale` logs "not implemented" |
| Whether an object is active: *‹object› active* (called once in 1 script) | todo | `IsActive` logs "not implemented" |
| Finds a flying thing of a kind within a radius: *get ‹type› [‹subtype›] flying at ‹position› radius ‹radius› [excluding scripted]* (called 2 times in 1 script) | todo | `CallFlying` logs "not implemented" |
| Fades an object in over a time: *set ‹object› fade in [time ‹time›]* (called once in 1 script) | todo | `SetObjectFadeIn` logs "not implemented" |
| Gives the hand's state (empty, holding, casting and so on): *get hand state* (called 4 times in 4 scripts) | todo | `GetHandState` logs "not implemented" |
| Gives the object the player last clicked: *get object clicked* (called once in 1 script) | todo | `GetObjectClicked` logs "not implemented" |
| Whether an object can be set on fire: *enable/disable ‹object› set on fire* (called 17 times in 7 scripts) | done | `SetSetOnFire`: fire audit: OK |
| Makes a villager carry an object: *set ‹object› carrying ‹carried obj›* (called 5 times in 3 scripts) | todo | `SetObjectCarrying` logs "not implemented" |
| Finds a dead thing within a radius of a position: *get dead at ‹position› radius ‹radius›* (called once in 1 script) | todo | `GetDeadLiving` logs "not implemented" |
| Gives the first thing in a container: *get first in ‹container›* (called 2 times in 2 scripts) | todo | `GetFirstInContainer` logs "not implemented" |
| Gives the next thing in a container: *get next in ‹container› after ‹after›* (called 2 times in 2 scripts) | todo | `GetNextInContainer` logs "not implemented" |

## Not used by the shipped scripts

The game has these but no shipped script calls them; mods and fan-made challenges can.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Gives the object a player or creature last dropped: *get dropped by ‹creature›* (not called by the shipped scripts) | todo | `GetObjectDropped` logs "not implemented" |
| Forgets the object a player or creature last dropped: *clear dropped by ‹creature›* (not called by the shipped scripts) | todo | `ClearDroppedByObject` logs "not implemented" |
| Stops villagers reacting to an object (unconfirmed): *detach reaction ‹object›* (not called by the shipped scripts) | todo | `RemoveReaction` logs "not implemented" |
| Gives where an object is heading: *destination of ‹obj›* (not called by the shipped scripts) | todo | `GetObjectDestination` logs "not implemented" |
| Forgets the position the player last clicked: *clear clicked position* (not called by the shipped scripts) | todo | `ClearClickedPosition` logs "not implemented" |
| Whether the player has clicked near a position: *‹value› clicked radius ‹value›* (not called by the shipped scripts) | todo | `PositionClicked` logs "not implemented" |
| Gives the object under the hand: *get object hand is over* (not called by the shipped scripts) | todo | `GetObjectHandIsOver` logs "not implemented" |
| Finds a poisoned object of a kind in a container: *get poisoned ‹type› [‹subtype›] in ‹container› [excluding scripted]* (not called by the shipped scripts) | todo | `CallPoisonedIn` logs "not implemented" |
| Gives how far along its path a walker is: *get ‹object› walk path percentage* (not called by the shipped scripts) | todo | `GetWalkPathPercentage` logs "not implemented" |
| Gives the slowest speed in a flock: *get slowest speed in ‹flock›* (not called by the shipped scripts) | todo | `GetSlowestSpeed` logs "not implemented" |
| Gives the arena (unused in the shipped scripts) (unconfirmed): *get the arena (no statement in the language)* (not called by the shipped scripts) | todo | `GetArena` logs "not implemented" |
| Gives the football pitch in a town: *get football pitch in ‹town›* (not called by the shipped scripts) | todo | `GetFootballPitch` logs "not implemented"; see [../town/football.md](../town/football.md) |
| Stops all the games in a town: *stop all games for ‹value›* (not called by the shipped scripts) | todo | `StopAllGames` logs "not implemented" |
| Puts a villager into a game (football) for the home or away side: *attach ‹value› to game ‹value› for home/away team* (not called by the shipped scripts) | todo | `AttachToGame` takes its arguments, controlling the pitch and villager (`d39f8402`), and logs "not implemented"; see [../town/football.md](../town/football.md) |
| Takes a villager out of a game's side: *detach ‹value› in game ‹value› from home/away team* (not called by the shipped scripts) | todo | `DetachFromGame` takes its arguments, controlling the pitch and villager (`d39f8402`), and logs "not implemented" |
| Takes the player out of a game's side: *detach player from game ‹value› from home/away team* (not called by the shipped scripts) | todo | `DetachUndefinedFromGame` takes its arguments, controlling the pitch (`d39f8402`), and logs "not implemented" |
| Makes an object answer only to scripts: *enable/disable ‹value› only for scripts* (not called by the shipped scripts) | todo | `SetOnlyForScripts` takes its arguments, controlling the pitch (`d39f8402`), and logs "not implemented" |
| Starts a match with a referee: *start ‹value› with ‹value› as referee* (not called by the shipped scripts) | todo | `StartMatchWithReferee` takes its arguments, controlling the pitch and referee (`d39f8402`), and logs "not implemented" |
| Gives the size of a side in a game: *get size of ‹value› home/away team* (not called by the shipped scripts) | todo | `GameTeamSize` logs "not implemented" |
| Gives the object last hit: *get hit object* (not called by the shipped scripts) | todo | `GetHitObject` logs "not implemented" |
| Gives the object that did the hitting: *get object which hit* (not called by the shipped scripts) | todo | `GetObjectWhichHit` logs "not implemented" |
| Gives how faded an object is: *get ‹object› fade* (not called by the shipped scripts) | todo | `GetObjectFade` logs "not implemented" |
| Whether a villager is a skeleton: *‹object› skeleton* (not called by the shipped scripts) | todo | `IsSkeleton` logs "not implemented" |
| Keeps an object inside an area (unconfirmed): *confine an object (no statement in the language)* (not called by the shipped scripts) | todo | `ConfinedObject` logs "not implemented" |
| Lets an object out of its area (unconfirmed): *release a confined object (no statement in the language)* (not called by the shipped scripts) | todo | `ClearConfinedObject` logs "not implemented" |
| Gives the flock an object belongs to: *get ‹member› flock* (not called by the shipped scripts) | todo | `GetObjectFlock` logs "not implemented" |
| Whether a cut-scene special effect has finished: *jc special ‹feature› played* (not called by the shipped scripts) | todo | `IsPlayingJcSpecial` logs "not implemented" |
| Whether a flock is within its limits: *‹object› within flock limits* (not called by the shipped scripts) | todo | `FlockWithinLimits` logs "not implemented" |
| Clears an actor's mind (unconfirmed): *clear an actor's mind (no statement in the language)* (not called by the shipped scripts) | todo | `ClearActorMind` logs "not implemented" |
| Starts an object over: *restart ‹object›* (not called by the shipped scripts) | todo | `RestartObject` logs "not implemented" |
