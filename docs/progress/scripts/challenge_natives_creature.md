# Challenge natives: creatures

The challenge scripts' functions for creatures: creating and loading them, their desires, actions and learning, the leash, fighting, development and the rival gods' creatures. The game has 464 of these functions in all; the language statement each comes from is shown in italics, and "called" counts are calls in the shipped `challenge.chl`. How the virtual machine runs them is in [../engine/script_vm.md](../engine/script_vm.md); what each challenge is about is in [../story/](../story/).

**Progress: 1/64 done, 9 partial — 9%**

## Used by the shipped scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Creates a creature of a species from another creature's mind at a position: *create_creature_from_creature ‹creature› ‹scale› at ‹position› ‹type›* (called 5 times in 5 scripts) | todo | `CreatureCreateRelativeToCreature` logs "not implemented" |
| Teaches a creature every action: *teach ‹creature› all* (called once in 1 script) | todo | `CreatureLearnEverything` logs "not implemented" |
| Teaches a creature (or makes it forget) an action on a kind of object: *teach ‹creature› ‹type of action› ‹action› ‹knows›* (called 137 times in 5 scripts) | todo | `CreatureSetKnowsAction` logs "not implemented" |
| Sets how much a creature wants to do the things on its agenda: *set ‹creature› priority ‹priority›* (called once in 1 script) | todo | `CreatureSetAgendaPriority` logs "not implemented" |
| Makes a creature do an action (eat, throw, pick up, cast, dance and so on) on an object: *force ‹value› ‹value› ‹value› ‹value›* (called 168 times in 46 scripts) | todo | `CreatureDoAction` logs "not implemented" |
| Whether a creature is holding an object in its hand: *‹obj› in ‹creature› hand* (called 80 times in 29 scripts) | todo | `InCreatureHand` logs "not implemented" |
| Sets how strong one of a creature's desires is: *set ‹creature› desire ‹desire› to ‹value›* (called 5 times in 3 scripts) | todo | `CreatureSetDesireValue` logs "not implemented" |
| Turns one of a creature's desires on or off: *set ‹creature› desire ‹desire› ‹active›* (called 5 times in 3 scripts) | todo | `CreatureSetDesireActivated78` logs "not implemented" |
| Turns one of a creature's desires on or off (the second form, taking the creature as an object): *set ‹creature› desire ‹active› ‹value›* (called 5 times in 4 scripts) | todo | `CreatureSetDesireActivated79` logs "not implemented" |
| Sets the most one of a creature's desires can grow to: *set ‹creature› desire maximum ‹desire› to ‹value›* (called 4 times in 2 scripts) | todo | `CreatureSetDesireMaximum` logs "not implemented" |
| Gives the player a creature (makes an existing creature the player's own): *set player_creature to ‹creature›* (called 3 times in 1 script) | todo | `CreatureSetPlayer` logs "not implemented" |
| Gives the object a creature is going for: *get target object for ‹obj›* (called once in 1 script) | todo | `GetTargetObject` logs "not implemented" |
| Takes a creature's leash off: *detach ‹creature› leash* (called 20 times in 12 scripts) | partial | `DetachObjectLeash`: `LeashSystem::TakeOff`; not audited |
| Gives a creature a single desire for a time, overriding the rest: *set ‹creature› only desire ‹desire› [time ‹value›]* (called 8 times in 5 scripts) | todo | `SetCreatureOnlyDesire` logs "not implemented" |
| Takes the single desire off a creature: *set ‹creature› disable only desire* (called 3 times in 3 scripts) | todo | `SetCreatureOnlyDesireOff` logs "not implemented" |
| Gives a player's creature: *get player ‹player› creature* (called 97 times in 64 scripts) | todo | `CallPlayerCreature` logs "not implemented" |
| Sets a creature's stage of development (how much it knows and how big it is): *set ‹creature› ‹stage› development* (called 18 times in 17 scripts) | todo | `SetCreatureDevStage` logs "not implemented" |
| Swaps the player's creature for another species, keeping its mind: *swap creature from ‹from creature› to ‹to creature›* (called 2 times in 2 scripts) | todo | `SwapCreature` logs "not implemented" |
| Whether a creature is on a leash: *‹object› leashed* (called 10 times in 7 scripts) | partial | `IsLeashed`: `LeashSystem::IsLeashed`; not audited |
| Sets where a creature lives: *set ‹creature› home position ‹position›* (called 4 times in 2 scripts) | todo | `SetCreatureHome` logs "not implemented" |
| Whether a creature is fighting: *‹object› fighting* (called 16 times in 6 scripts) | todo | `IsFighting` logs "not implemented" |
| Whether a creature's leash works: *enable/disable leash on ‹creature›* (called 15 times in 6 scripts) | partial | `SetLeashWorks`: `LeashSystem::SetWorks`; not audited |
| Loads the player's own creature into the land at a position: *load my_creature at ‹position›* (called 4 times in 4 scripts) | todo | `LoadMyCreature` logs "not implemented" |
| Gives the position of a creature's backside (where it poops): *arse position of ‹object›* (called once in 1 script) | todo | `GetArsePosition` logs "not implemented" |
| Whether a creature is leashed to an object: *‹object› leashed to ‹target›* (called 7 times in 4 scripts) | partial | `IsLeashedToObject`: `LeashSystem::TiedTo`; not audited |
| Gives how strongly a creature is interacting with something: *get ‹creature› interaction magnitude* (called 3 times in 2 scripts) | todo | `GetInteractionMagnitude` logs "not implemented" |
| Whether a species of creature is available to choose: *creature ‹type› is available* (called 5 times in 1 script) | todo | `IsCreatureAvailable` logs "not implemented" |
| Gives how many times a creature has done an action: *number of times action ‹action› by ‹creature›* (called 4 times in 1 script) | todo | `GetActionCount` logs "not implemented" |
| Gives which kind of leash a creature is on: *get ‹object› leash type* (called once in 1 script) | partial | `GetObjectLeashType`: `LeashSystem::TypeOf`, none counted as 0; not audited |
| Marks a creature as being in a development (training) script: *enable/disable ‹creature› development script* (called 12 times in 11 scripts) | todo | `CreatureInDevScript` logs "not implemented" |
| Whether the leash is drawn: *enable/disable leash draw* (called 8 times in 2 scripts) | partial | `SetDrawLeash`: `LeashSystem::SetDrawn`; not audited |
| Makes two creatures friends or not: *enable/disable ‹creature› friends with ‹target creature›* (called 3 times in 2 scripts) | todo | `CreatureForceFriends` logs "not implemented" |
| Loads a creature of a species from a mind file for a player at a position (the rival gods' creatures): *load creature ‹type› ‹mind filename› player ‹player› at ‹position›* (called 4 times in 4 scripts) | todo | `LoadCreature` logs "not implemented" |
| Sets how a creature's creed (its learnt behaviour) is shown: hand, scale, power, time: *set ‹creature› creed properties hand ‹hand glow› scale ‹scale› power ‹power› time ‹time›* (called 12 times in 4 scripts) | todo | `SetCreatureCreedProperties` logs "not implemented" |
| Gives a creature a name: *set ‹creature› name ‹text id›* (called 8 times in 8 scripts) | todo | `SetCreatureName` logs "not implemented" |
| Puts a player's leash on or takes it off, as the leash key does: *toggle player ‹player› leash* (called 4 times in 3 scripts) | partial | `ToggleLeash`: presses the player's leash key in `LeashSystem`; not audited |
| Lets a creature fight on its own or not: *enable/disable ‹creature› auto fighting* (called 4 times in 1 script) | todo | `SetCreatureAutoFighting` logs "not implemented" |
| Queues a fight move for a creature: *queue ‹creature› fight move ‹move›* (called 3 times in 1 script) | todo | `SetCreatureQueueFightMove` logs "not implemented" |
| Gives a creature's fight action: *get ‹creature› fight action* (called 5 times in 1 script) | todo | `GetCreatureFightAction` logs "not implemented" |
| Gives how many hits a creature has queued in a fight: *get ‹creature› fight queue hits* (called 2 times in 1 script) | todo | `CreatureFightQueueHits` logs "not implemented" |
| Lets the player's creature into the temple or keeps it out: *enable/disable creature in temple* (called 3 times in 3 scripts) | todo | `SetCreatureInTemple` logs "not implemented" |
| Lets a creature's size follow the land's scale, or not: *enable/disable ‹creature› auto scale [‹size›]* (called 8 times in 7 scripts) | todo | `CreatureAutoscale` logs "not implemented" |

## Not used by the shipped scripts

The game has these but no shipped script calls them; mods and fan-made challenges can.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Turns all of a creature's desires off: *turn off all of a creature's desires (no statement in the language)* (not called by the shipped scripts) | todo | `CreatureTurnOffAllDesires` logs "not implemented" |
| Teaches a creature whether an action is good to do with a kind of object: *teach a distinction about an action (no statement in the language)* (not called by the shipped scripts) | todo | `CreatureLearnDistinctionAboutActivityObject` logs "not implemented" |
| Resets the count of how many times a creature has done an action: *initialise number of ‹value› for ‹value›* (not called by the shipped scripts) | todo | `CreatureInitialiseNumTimesPerformedAction` logs "not implemented" |
| Gives how many times a creature has done an action: *get number of ‹value› for ‹value›* (not called by the shipped scripts) | todo | `CreatureGetNumTimesActionPerformed` logs "not implemented" |
| Turns the creature's own help (its comments on what it is doing) on or off: *creature help on/off (no statement in the language)* (not called by the shipped scripts) | todo | `SetCreatureHelp` logs "not implemented" |
| Whether a creature's strongest desire is a given one: *desire of ‹value› is ‹value›* (not called by the shipped scripts) | todo | `CreatureDesireIs` logs "not implemented" |
| Makes a creature finish what it is doing at once: *force ‹creature› finish* (not called by the shipped scripts) | todo | `CreatureForceFinish` logs "not implemented" |
| Ties a creature's leash to an object: *attach ‹creature› leash to ‹object›* (not called by the shipped scripts) | partial | `AttachObjectLeashToObject`: `LeashSystem::TieTo`; ported from the leash research, not audited |
| Ties a creature's leash to the player's hand: *attach ‹creature› leash to hand* (not called by the shipped scripts) | partial | `AttachObjectLeashToHand`: `LeashSystem`; ported from the leash research, not audited |
| Lets a creature attack its own player's town or not: *enable/disable ‹value› attack own town* (not called by the shipped scripts) | todo | `SetAttackOwnTown` logs "not implemented" |
| Whether a creature goes back to how it was when the spells on it wear off: *enable/disable ‹value› spell reversion* (not called by the shipped scripts) | done | `CreatureSpellReversion`: core audit: fixed (creature taken first) |
| Gives how strong one of a creature's desires is: *get ‹value› desire ‹value›* (not called by the shipped scripts) | todo | `GetDesire` logs "not implemented" |
| Teaches a creature everything except one action: *teach ‹value› all excluding ‹value›* (not called by the shipped scripts) | todo | `CreatureLearnEverythingExcluding` logs "not implemented" |
| Turns a creature's reactions on or off: *enable/disable ‹value› reaction* (not called by the shipped scripts) | todo | `CreatureReaction` logs "not implemented" |
| Whether a creature is interacting with an object: *‹value› interacting with ‹value›* (not called by the shipped scripts) | todo | `CreatureInteractingWith` logs "not implemented" |
| Gives the species a creature is opposite to: *get ‹god› opposite creature type* (not called by the shipped scripts) | todo | `OpposingCreature` logs "not implemented" |
| Gives the position of a creature's belly: *belly position of ‹object›* (not called by the shipped scripts) | todo | `GetBellyPosition` logs "not implemented" |
| Whether a creature is fighting on its own: *‹creature› is auto fighting* (not called by the shipped scripts) | todo | `IsAutoFighting` logs "not implemented" |
| Queues a fight miracle for a creature: *queue ‹creature› fight spell ‹spell›* (not called by the shipped scripts) | todo | `SetCreatureQueueFightSpell` logs "not implemented" |
| Queues a fight step for a creature: *queue ‹creature› fight step ‹step›* (not called by the shipped scripts) | todo | `SetCreatureQueueFightStep` logs "not implemented" |
| Lets a creature leave a fight or not: *enable/disable fight exit* (not called by the shipped scripts) | todo | `SetFightExit` logs "not implemented" |
| Whether a creature can get to a position: *‹position› valid for creature* (not called by the shipped scripts) | todo | `PosValidForCreature` logs "not implemented" |
