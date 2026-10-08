# Challenge natives: towns, players and rival gods

The challenge scripts' functions for towns (belief, desires, stores, buildings, worship), players (alignment, alliances, land balance) and the rival gods the computer plays. The game has 464 of these functions in all; the language statement each comes from is shown in italics, and "called" counts are calls in the shipped `challenge.chl`. How the virtual machine runs them is in [../engine/script_vm.md](../engine/script_vm.md); what each challenge is about is in [../story/](../story/).

**Progress: 0/49 done, 1 partial — 1%**

## Used by the shipped scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Gives a player's good or evil alignment: *alignment of player ‹zero›* (called 2 times in 2 scripts) | todo | `GetAlignment` logs "not implemented" |
| Has a town build a building at a position, with a desire: *build building at ‹position› desire ‹desire›* (called 28 times in 9 scripts) | todo | `BuildBuilding` logs "not implemented" |
| Gives how much food or wood a store holds: *get resource ‹resource› in ‹container›* (called 11 times in 7 scripts) | todo | `GetResource` logs "not implemented" |
| Puts food or wood into a store: *add resource ‹resource› ‹quantity› to ‹container›* (called 5 times in 3 scripts) | todo | `AddResource` logs "not implemented" |
| Takes food or wood out of a store: *remove resource ‹resource› ‹quantity› from ‹container›* (called 9 times in 4 scripts) | todo | `RemoveResource` logs "not implemented" |
| Gives a town's or villager's belief in a player: *get ‹object› belief for player ‹player›* (called 21 times in 9 scripts) | todo | `BeliefForPlayer` logs "not implemented" |
| Sets an object's belief in a player relative to others: *set ‹object› player ‹player› relative belief ‹belief›* (called once in 1 script) | todo | `ObjectRelativeBelief` logs "not implemented" |
| Moves a rival god's hand to a position at a speed: *move computer player ‹player› to ‹position› speed ‹speed› [with fixed height]* (called 68 times in 26 scripts) | todo | `MoveComputerPlayerPosition` logs "not implemented" |
| Turns a rival god (computer player) on or off: *enable/disable computer player ‹player›* (called 3 times in 3 scripts) | todo | `EnableDisableComputerPlayer311` logs "not implemented" |
| Gives where a rival god's hand is: *computer player ‹player› position* (called 8 times in 6 scripts) | todo | `GetComputerPlayerPosition` logs "not implemented" |
| Puts a rival god's hand at a position at once: *set computer player ‹player› position to ‹position› [with fixed height]* (called 10 times in 7 scripts) | todo | `SetComputerPlayerPosition` logs "not implemented" |
| Sets a town's or villager's belief in a player: *set ‹object› player ‹player› belief ‹belief›* (called 5 times in 4 scripts) | todo | `SetPlayerBelief` logs "not implemented" |
| Boosts or lowers one of a town's desires: *set ‹object› desire boost ‹desire› ‹boost›* (called 10 times in 4 scripts) | todo | `SetTownDesireBoost` logs "not implemented" |
| Whether a rival god is ready: *computer player ‹player› ready* (called 23 times in 12 scripts) | todo | `ComputerPlayerReady` logs "not implemented" |
| Turns a rival god on or off (second form): *enable/disable computer player ‹player›* (called 9 times in 7 scripts) | todo | `EnableDisableComputerPlayer345` logs "not implemented" |
| Creates a random villager of a tribe at a position: *create random villager of tribe ‹tribe› at ‹position›* (called once in 1 script) | todo | `CreateRandomVillagerOfTribe` logs "not implemented" |
| Sets what a scaffold builds, its size and whether it destroys what is under it: *set ‹object› building properties ‹type› size ‹size› [destroys when placed]* (called 8 times in 5 scripts) | todo | `SetScaffoldProperties` logs "not implemented" |
| Sets a rival god's personality (a named set of behaviours) on or off: *set computer player ‹player› personality ‹aspect› ‹probability›* (called 25 times in 5 scripts) | todo | `SetComputerPlayerPersonality` logs "not implemented" |
| Makes a rival god do an action at once: *force computer player ‹player› action ‹action› [‹obj1›] [‹obj2›]* (called 11 times in 6 scripts) | todo | `ForceComputerPlayerAction` logs "not implemented" |
| Adds an action to a rival god's queue: *queue computer player ‹player› action ‹action› [‹obj1›] [‹obj2›]* (called 4 times in 1 script) | todo | `QueueComputerPlayerAction` logs "not implemented" |
| Gives the town with a number from the land script: *get town with id ‹id›* (called 28 times in 14 scripts) | todo | `GetTownWithId` logs "not implemented" |
| Makes a villager a disciple of a job, with or without a sound: *set ‹object› disciple ‹disciple type› [with sound]* (called 3 times in 3 scripts) | todo | `SetDisciple` logs "not implemented" |
| Hands a rival god back to its own thinking: *release computer player ‹player›* (called 26 times in 18 scripts) | todo | `ReleaseComputerPlayer` logs "not implemented" |
| Sets how fast a rival god acts: *set computer player ‹player› speed ‹speed›* (called 4 times in 3 scripts) | todo | `SetComputerPlayerSpeed` logs "not implemented" |
| Gives a rival god as an object: *get computer player ‹player›* (called 2 times in 2 scripts) | todo | `CallComputerPlayer` logs "not implemented" |
| Whether a town may build a worship site: *enable/disable ‹object› build worship site* (called 6 times in 2 scripts) | todo | `SetCanBuildWorshipsite` logs "not implemented" |
| Sets a rival god's attitude to a player: *set computer player ‹player1› attitude to player ‹player2› to ‹attitude›* (called 4 times in 2 scripts) | todo | `SetComputerPlayerAttitude` logs "not implemented" |
| Sets how allied two players are: *set player ‹player1› ally with player ‹player2› percentage ‹percentage›* (called 5 times in 5 scripts) | todo | `SetPlayerAlly` logs "not implemented" |
| Gives how many adults a town or container holds: *adult size of ‹container›* (called once in 1 script) | todo | `IdAdultSize` logs "not implemented" |
| Gives how many adults a building can hold: *adult capacity of ‹container›* (called 2 times in 1 script) | todo | `ObjectAdultCapacity` logs "not implemented" |
| Gives how many worshippers have died in a town: *get worship deaths in ‹town›* (called 2 times in 1 script) | todo | `GetTownWorshipDeaths` logs "not implemented" |
| Gives how many towns a player has: *get player ‹player› town total* (called 4 times in 1 script) | todo | `GetPlayerTownTotal` logs "not implemented" |
| Gives a town's totem: *get totem statue in ‹town›* (called 2 times in 2 scripts) | todo | `GetTotemStatue` logs "not implemented" |
| Gives the time since a player last attacked a town: *get time since player ‹player› attacked ‹town›* (called 2 times in 1 script) | partial | `GetTimeSinceObjectAttacked`: from the town's aggression record; not audited |
| Gives the total health of a town's buildings and villagers: *get building and villager health total in ‹town›* (called 2 times in 1 script) | todo | `GetTownAndVillagerHealthTotal` logs "not implemented" |
| Gives how much has been sacrificed at a worship site: *get ‹worship site› sacrifice total* (called 2 times in 1 script) | todo | `GetSacrificeTotal` logs "not implemented" |
| Clears a rival god's actions: *clear computer player ‹player› actions* (called once in 1 script) | todo | `GameClearComputerPlayerActions` logs "not implemented" |

## Not used by the shipped scripts

The game has these but no shipped script calls them; mods and fan-made challenges can.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Sets a player's alignment: *set the player's alignment (no statement in the language)* (not called by the shipped scripts) | todo | `SetAlignment` logs "not implemented" |
| Gives a player's town nearest a position within a radius: *get nearest town at ‹value› for player ‹value› radius ‹value›* (not called by the shipped scripts) | todo | `GetNearestTownOfPlayer` logs "not implemented" |
| Holds back one of a rival god's behaviours: *set computer player ‹value› suppression ‹value› ‹value›* (not called by the shipped scripts) | todo | `SetComputerPlayerSuppression` logs "not implemented" |
| Finds a building of a kind in a town, built at least so far: *get building ‹value› in ‹value› min built ‹value› [excluding scripted]* (not called by the shipped scripts) | todo | `CallBuildingInTown` logs "not implemented" |
| Gives a rival god's attitude to a player: *get computer player ‹player1› attitude to player ‹player2›* (not called by the shipped scripts) | todo | `GetComputerPlayerAttitude` logs "not implemented" |
| Loads a rival god's personality from a file: *load computer player ‹value› personality ‹value›* (not called by the shipped scripts) | todo | `LoadComputerPlayerPersonality` logs "not implemented" |
| Saves a rival god's personality to a file: *save computer player ‹value› personality ‹value›* (not called by the shipped scripts) | todo | `SaveComputerPlayerPersonality` logs "not implemented" |
| Gives how many a building can hold: *capacity of ‹container›* (not called by the shipped scripts) | todo | `ObjectCapacity` logs "not implemented" |
| Gives how allied two players are: *get player ‹player1› ally percentage with player ‹player2›* (not called by the shipped scripts) | todo | `GetPlayerAlly` logs "not implemented" |
| Sets a player's land balance: *set ‹value› land balance ‹value›* (not called by the shipped scripts) | todo | `SetLandBalance` logs "not implemented" |
| Sets how much an object's belief counts: *set ‹value› belief scale ‹value›* (not called by the shipped scripts) | todo | `SetObjectBeliefScale` logs "not implemented" |
| Adds food or wood to a building site (unconfirmed): *add for building ‹value› to ‹value›* (not called by the shipped scripts) | todo | `GameAddForBuilding` logs "not implemented" |
