# The Greedy Farmer

A silver scroll on land 2: a Celtic farmer asks the player to stop a gang of five hungry children from stealing his
herd of ten cows, which they lead off one at a time to a campfire hideout and slaughter. The quest ends
when the farmer dies, the herd is gone, the children are all dead, or a child is killed beside the gang and the rest
lose their nerve; three of those endings strengthen the local town's Lightning miracle. The land as a whole is in
[../land_2.md](../land_2.md), the land's control script in [../../scripts/land2_script.md](../../scripts/land2_script.md),
the challenge program in [../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md).

**Land:** 2 · **Giver:** a Celtic farmer at his house on the edge of the Celtic town (town id 4) · **Script:** GreedyFarmer · **Reward:** the Lightning miracle and its second power-up level put into the local town (not for the "all cows gone" ending) · **Repeatable:** no

Sources: the quest's original script source (`GreedyFarmer.txt`, ten scripts) and the land's trigger in
`LandControl2.txt`, both checked line by line against the PC game's compiled `challenge.chl`; the shared helpers it
runs (the scroll notify loop and the standard reminder); the game's English text table for every line quoted; the
executable (read-only) for what putting a miracle in a town does. openblack's state is judged on the physics work tree
(`ob-wt-physics`): of the 60 script functions this quest and its helpers call, 46 still only log "not implemented" in
`src/CHLApi.cpp` (among them every flock, dialogue, advisor, snapshot, highlight, camera-move, text, property and
object-move function), `CREATE` makes only scenery and rocks (`CreateScriptObject`), so no cow, child, farmer or marker
is made, and the land's control script stops at its first unwritten function long before the quest's trigger (see
[../land_2.md](../land_2.md) row 1). The quest therefore never appears; every row below is todo.

**Progress: 0/143 done, 0 partial — 0%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script starts a background check on the Celtic town with id 4 (the source comment on it reads "Celtic - Greedy Farmer") | todo | the control script never gets this far in openblack (see [../land_2.md](../land_2.md)) |
| The check waits 8 minutes, then looks whether the town belongs to the player; if not, it waits another 8 minutes and looks again, for ever, so the quest can only start on an 8-minute beat after the player has won the town | todo | town ownership read is in the land script; the wait is the script VM's sleep |
| Once the town is the player's the quest starts in the background and the check stops; it starts only once | todo | |
| Nothing is made before that: the herd, the children and the farmer all appear only after the town is won | todo | |
| A campfire (a bonfire object) is lit at the children's hideout, on low ground about 180 from the farmer's house as soon as the quest starts | todo | `CREATE` would make this one (a mobile static, `CreateScriptObject`), but the quest never runs |
| Ten cows are made one at a time in the farmer's field, each within 2 of the field's centre, and each only at a moment when the field is not on screen, so the player never sees them pop in | todo | `CREATE` makes no animals; field-of-view test `PosFieldOfView` is a stub |
| The cows form a herd that keeps its members within an inner radius of 5 and an outer radius of 30, and are set to move as a herd; 2 seconds later they start grazing | todo | `FlockCreate`, `FlockAttach`, `ChangeInnerOuterProperties`, `SetScriptState` stubs |
| Each cow gets its own watcher script from the moment it is made (see "Saving cows") | todo | |
| The script then waits until the hideout is off screen and makes five boys there, Celtic farmer children, each set to age 10, and puts them into a gang (a flock with inner radius 3, outer radius 5) | todo | `CREATE` makes no villagers; `SetProperty` (age) stub |
| The children start stealing at once, before the scroll is even shown (see "The cattle thieves") | todo | |
| A silver challenge scroll is then put up at the farmer's house | todo | `CreateHighlight` stub |
| While the scroll waits, whenever the camera is within 100 of it and it is on screen, at most once every 30 seconds and only when no cut scene is running, the evil advisor steps out, points at the scroll and says "Hey. There's a job for you. Wanna do it?" | todo | shared notify helper; `SpiritEject`, `SpiritPointPos`, `RunText` stubs |
| The scroll waits until the player clicks the scroll or the farmer's house itself; the scroll is then made active | todo | `GameThingClicked`, `SetActive` stubs |
| The quest's main script waits on the scroll and does not watch for the endings meanwhile, so the children can steal the whole herd before the player ever clicks; the "all cows gone" ending then plays straight after the introduction | todo | evidence: the notify helper is run as a blocking script before the main watch loop |
| The farmer himself is made at his house only when the scroll is clicked | todo | |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Clicking starts a cut scene (widescreen, the camera and dialogue taken from the player, the game at normal speed) with the generic script music, number 2 | todo | `StartCameraControl`, `StartDialogue`, `StartGameSpeed` stubs; `SetWidescreen`, `StartMusic` work but are never reached |
| The farmer is drawn in high detail for the scene | todo | `SetHighGraphicsDetail` stub |
| The camera's starting view is remembered so it can be given back | todo | |
| The farmer walks from his house to his soap box, a spot beside it, while the camera glides over 3 seconds to a low view of the soap box | todo | `MoveGameThing`, `MoveCameraPosition`, `MoveCameraFocus` stubs |
| The scene waits until the farmer is within 3 of the soap box and the camera has arrived; he then turns to face the camera and plays his idle animation on a loop | todo | `SetFocus`, `SetScriptUlong`, `SetScriptState` stubs |
| After 1 second the camera drifts over 5 seconds to a closer view of him | todo | |
| The quest is recorded in the player's challenge log: a snapshot with the title "The Greedy Farmer", success 0 and alignment 0, the current camera view, and a reminder that replays the good advisor's line "Those hungry children are stealing the farmer's cattle Shall we get involved?" | todo | `Snapshot` stub; the reminder is the shared standard reminder script (the advisor who owns the line steps out to say it) |
| Farmer: "Excuse me powerful being. I need your help." | todo | `RunText`, `TextRead` stubs |
| Farmer: "Children have been stealing my cattle." He plays a disgruntled crowd animation once | todo | |
| 2 seconds into that line the camera cuts to the gang of children, 13 to the side and 9 above them, and follows them | todo | `SetFocusFollow` stub; `SetCameraPosition`/`SetCameraFocus` work but the scene never runs |
| Farmer, over the shot of the children: "Stop it you little thieves!" | todo | |
| After 3 seconds the camera cuts back to the close view of the farmer | todo | |
| He goes back to his idle animation on a loop | todo | |
| Farmer: "Please, you must act to stop them." This line waits for the player to click on before going on | todo | the "with interaction" form of `RunText` |
| The camera glides back to the player's starting view over 2 seconds | todo | |
| The farmer is set to wander around his soap box (with the script's wander values 6, 4 and 20) and goes back to normal detail; the music stops and the cut scene ends, keeping the dialogue | todo | `SetScriptStatePos`, `SetScriptFloat`, `SetScriptUlong` stubs; what the three wander values mean is undetermined (engine wander state not traced) |
| Both advisors step out and the dialogue is cleared | todo | `GameClearDialogue` stub |
| Evil advisor: "Kill someone. The rich farmer dude?" | todo | |
| Good advisor: "Think of the poor hungry children." | todo | |
| Evil advisor: "I am. " | todo | the text table line ends in a space |
| The dialogue then ends; neither advisor is sent home here | todo | |

## The herd

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The herd grazes the field: the script picks a random spot within 20 (each way) of where the herd first stood, retrying until the spot is at least 20 from the herd's current position, and sends the herd there | todo | `MoveGameThing` stub; `RANDOM` works |
| It then waits 300 seconds plus a random 0 to 300 before the next move; the random part is drawn afresh on every check of the wait (see quirks) | todo | |
| The grazing stops when the herd is empty or the quest has ended | todo | `IdSize` stub |

## The cattle thieves

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The gang is slowed to speed 0.4 and walks to where the herd is at that moment, waiting until it is within 5 of that spot | todo | `SetProperty` (speed) stub |
| A cow is taken out of the herd and one boy of the gang is picked | todo | `FlockDetach`, `CallIn` stubs |
| The cow is slowed to speed 0.4 and told to stand where it is; the boy walks up to it until he is within 5 | todo | |
| The cow is then put into the gang as its leader; cow and boy both walk to the gang and the script waits until both are within 10 of it | todo | `FlockAttach` (as leader) stub |
| The cow is sent to the hideout at speed 0.4, cow and boy are set to move as a flock, and the gang walks to the hideout | todo | |
| The script waits until the gang is within 10 of the hideout and the cow within 1 of it; a cow-slaughter sound plays at the cow, it is taken out of the gang and deleted with a fade | todo | `PlaySoundEffect`, `ObjectDelete` stubs |
| A count of slaughtered cows goes up by one (the count is never used; see cut parts) | todo | |
| The theft is abandoned at once if the cow or the boy stops existing, or if the cow is picked up by the player's hand or is in the player's creature's hand; the gang then goes back for another cow | todo | `GetProperty` (held), `InCreatureHand` stubs |
| The gang keeps stealing one cow after another until the quest ends; if no cow or no boy can be had it simply tries again | todo | |

## Saving cows

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Whenever a cow is picked up by the hand or held by the player's creature, its watcher waits until it is let go, then puts it back into the herd at speed 0.4, so a dropped cow walks back to the herd | todo | `GetProperty`, `InCreatureHand`, `FlockAttach` stubs |
| A cow counts as stolen once it is in the gang | todo | `FlockMember` stub |
| A stolen cow that ends up back in the herd counts as saved: the good advisor steps out and says "You saved a cow. The Farmer will be pleased." and goes home | todo | |
| The first cow saved adds 0.4 to the quest's alignment total; later saves only repeat the line | todo | |
| A cow's watcher stops when the cow's health reaches 0 or the quest ends | todo | |

## The angry farmer

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After the introduction the farmer watches for the gang: when it comes within 40 of him he goes after it | todo | |
| If the camera is within 100 of him, he is on screen and no cut scene is starting, he shouts one of two lines picked at random: "Oi! Them's my cows!" or "Stop it you little thieves!" | todo | `RandomUlong`, `RunText` stubs |
| He chases the gang until he is 20 or more from his soap box or within 10 of the gang | todo | |
| Then, on the same camera conditions, he shouts "Come back with my cattle you swines!" | todo | |
| He walks back to his soap box and waits until he is within 10 of it | todo | |
| He chases again only after the gang has been more than 40 from him | todo | |
| The chasing stops when he dies, the quest ends or the herd is empty | todo | |

## Endings: what is checked

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After the introduction the main script watches the quest every 1.1 seconds | todo | |
| Each boy has his own watcher, checking every 1.1 seconds; a dead boy (health 0 or less) still in the gang is taken out of it | todo | `GetProperty`, `FlockMember`, `FlockDetach` stubs |
| If the dead boy is not flying through the air and lies within 10 of the gang, the gang is marked as scared and he is remembered as the dead boy | todo | |
| A boy killed more than 10 from the gang, or while flying, only leaves the gang; the next check can still find him near the gang once he has landed | todo | evidence: the check repeats every 1.1 s for that boy until a scare is marked or the quest ends |
| The cow being led is counted in the gang, so the main script allows for it: if the gang holds a cow and nothing else, every boy is gone | todo | `CallIn` (find a cow in the gang), `IdSize` stubs |
| The first time the gang is smaller than five boys (plus the led cow), the evil advisor steps out: "Bit of killing. Nice one." waits 2 seconds and goes home; this is said once | todo | it is also said when a boy leaves the gang for any other reason |
| The endings are tested in this order, the first that holds wins: the farmer's health is 0 or less; the herd is empty; the gang is empty or holds only a cow; the gang is scared | todo | evidence: the order of the end conditions in the compiled watch loop |
| When an ending fires, any cut scene, game speed, dialogue and camera control running in the watch loop are ended at once, so the evil advisor's "Nice one" can be cut off | todo | evidence: each end condition's handler closes widescreen, game speed, dialogue and camera control |

## Ending: the farmer is killed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Good advisor steps out: "The farmer's dead. Outrageous." and goes home | todo | |
| 0.6 is taken off the alignment total (total −0.6, or −0.2 if a cow was saved first) | todo | |
| The log entry is updated: success 1.0, that alignment, the same title and reminder | todo | `UpdateSnapshot` stub |
| The reward is given (see Reward) | todo | |
| Who killed him does not matter (the player, the creature, a miracle or Khazar's or Lethys's side) | todo | evidence: the test is only his health |

## Ending: the herd is gone

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Reached when the herd's size is 0, whether the children stole every cow or the player, the creature or anything else killed or removed them | todo | |
| The farmer walks to his soap box; the script waits until he is within 3 of it, then until the camera is within 100 of the soap box with it on screen (it waits for ever if the player never looks) | todo | |
| A cut scene glides the camera over 3 seconds to a view of the soap box | todo | |
| The log entry is updated: success 1.0 with the alignment total unchanged (0, or +0.4 if a cow was saved) | todo | |
| Farmer: "All my cows are gone. You're no god of mine!" | todo | |
| After the line is read and 1 more second, the camera glides back to the player's view over 2 seconds and the scene ends | todo | |
| No reward is given for this ending | todo | evidence: it is the only ending that does not set the reward flag |

## Ending: all the children are killed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Reached when the gang is empty or holds only the cow it was leading; in practice when each boy dies away from the gang (a death beside it scares the others first) | todo | |
| The script waits until the camera is within 100 of the soap box and it is on screen | todo | |
| A cut scene cuts the camera straight to the soap box view; the farmer is sent walking to the soap box | todo | |
| Evil advisor steps out: "Way to go, Boss. You killed all the children." and goes home | todo | |
| The scene waits until the farmer is within 2 of the soap box, then 1 second; Farmer: "I wanted you to stop them stealing the cows, not kill them all!" | todo | |
| The camera cuts back to the player's view and the scene ends | todo | |
| 1.0 is taken off the alignment total (total −1.0, or −0.6 if a cow was saved), and the log entry is updated with success 1.0 | todo | |
| The reward is given | todo | |

## Ending: the children are scared off

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Reached when a boy dies beside the gang (see the checks above); no camera condition, the scene starts wherever the player is looking | todo | |
| A cut scene starts; each living boy other than the dead one walks to stand around the body (2 in front, 2 behind, 2 to either side, and one diagonally behind) | todo | |
| The camera glides over 6 seconds to a view 10 to the side and 10 above a boy of the gang | todo | which boy is the one found in the gang as the scene starts |
| The gang turns to look at the body; the camera glides over 6 seconds to a closer view (8 to the side, 5 above) | todo | `SetFocus` on a flock, stub |
| A boy: "I'm scared. I'm not stealing cows any more." | todo | |
| A boy: "Me neither. I'm off." | todo | |
| The dialogue box is closed and, a tenth of a second later, a new snapshot is taken (not an update): success 1.0, the alignment total unchanged (0, or +0.4 if a cow was saved) | todo | `GameCloseDialogue`, `Snapshot` stubs |
| The scene ends and the gang is broken up, the boys going their own way | todo | `FlockDisband` stub |
| The reward is given | todo | |

## Alignment and success values

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The snapshot records the quest in the player's challenge log with its title, a success fraction, an alignment value, a camera view and a reminder script; later updates change the success and alignment of that entry | todo | `Snapshot`, `UpdateSnapshot` stubs |
| Introduction: success 0.0, alignment 0.0 | todo | |
| The total starts at 0 and only one thing raises it: the first cow saved, +0.4 | todo | |
| Farmer killed: −0.6 → −0.6 (−0.2 with a saved cow), success 1.0 | todo | |
| Herd gone: no change → 0 (+0.4 with a saved cow), success 1.0 | todo | |
| Children all killed: −1.0 → −1.0 (−0.6 with a saved cow), success 1.0 | todo | |
| Children scared off: no change → 0 (+0.4 with a saved cow), success 1.0 | todo | |
| Every ending counts as full success (1.0), including losing the whole herd | todo | |

## Reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Whatever the ending, any cows left are handed over to the local town (the town within 100 of a point about 60 from the farmer's house) | todo | `FlockAttach` with a town, stub; what the town does with the herd is undetermined (engine side not traced) |
| For the farmer-killed, children-killed and scared endings, the Lightning miracle and its second power-up level are added to that town's miracles (the source comment calls it "a lightning bolt PU 1 for the LocalTown") | todo | the engine adds each miracle type to the town's held miracles; `SetMagicInObject` stub; see [../rewards.md](../rewards.md) |
| A cut scene flies the camera over 3 seconds to the town centre | todo | |
| The evil advisor steps out: "The Lightning Bolt just got better." The dialogue then ends; the advisor is not sent home by the script and the camera is not moved back | todo | |
| The local town is found by position, not by id; that it is the land script's town id 4 is inferred from the land script's comment and the positions, not checked | todo | |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The farmer, if alive, keeps wandering around his soap box; nothing removes him | todo | |
| After the scare the boys stay in the world as ordinary children; after the other endings the gang simply stops stealing | todo | the gang is broken up only in the scare ending |
| The campfire at the hideout stays | todo | |
| Nothing in the land script reads this quest's result | todo | evidence: no other script uses the quest's finished flag |

## Advisors' comments

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Evil advisor (scroll notify): "Hey. There's a job for you. Wanna do it?" | todo | |
| Good advisor (reminder): "Those hungry children are stealing the farmer's cattle Shall we get involved?" | todo | the line has no full stop between its two sentences in the text table |
| The intro exchange: evil "Kill someone. The rich farmer dude?", good "Think of the poor hungry children.", evil "I am. " | todo | |
| Good advisor on a saved cow: "You saved a cow. The Farmer will be pleased." | todo | |
| Evil advisor on the first child lost: "Bit of killing. Nice one." | todo | |
| Good advisor on the farmer's death: "The farmer's dead. Outrageous." | todo | |
| Evil advisor on all the children dead: "Way to go, Boss. You killed all the children." | todo | |
| Evil advisor on the reward: "The Lightning Bolt just got better." | todo | |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The introduction plays the generic script music number 2 and stops it at the end of the scene | todo | `StartMusic`/`StopMusic` work in openblack but the scene never runs |
| Each slaughtered cow plays the script cow-slaughter sound at the hideout | todo | `PlaySoundEffect` stub |
| No other music or sound is started by the quest | todo | |

## Creature involvement

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A cow in the player's creature's hand counts like one in the player's hand: the theft is dropped and the cow goes back to the herd when released | todo | `InCreatureHand` stub |
| The creature killing the farmer, the children or the cows leads to the same endings as the player doing it; the script never asks who did it | todo | |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The grazing wait's random part is re-drawn on every check, so the wait ends as soon as the elapsed time beats a fresh draw; it ends a little after 5 minutes rather than anywhere from 5 to 10 | todo | evidence: the compiled wait re-calls the random function each pass; the sleep test compares elapsed time against the value each pass (openblack's VM `Opcode21Sleep` does the same) |
| The quest's alignment total is a global that is never reset; harmless because the quest runs once | todo | |
| The farmer's first idle loop is given a count of −11 where the later one uses −1, apparently a typo; its effect is undetermined | todo | |
| A "Nice one" comment is also triggered by a boy leaving the gang for reasons other than death | todo | |
| The "herd gone" and "children killed" scenes wait for the camera to come to the soap box with no time limit, so the reward and the end can be put off for ever | todo | |
| The scare ending writes a second, new snapshot instead of updating the first | todo | |
| Source comments describe other lines than those used: "Gutted you killed the farmer", "Oi! Stop nicking me cows you little rascals!", "I'm too old for this shit!", "You stoopid fugger. I wanted you to stop them stealing the cows, not kill them!!", "The children are gone! Problem solved.", "The cows are all gone, dispute solved!" | todo | placeholder comments; the shipped text is quoted in the rows above |
| A comment on the herd-gone ending says it "should make the Farmer request you to click"; no such click is asked for | todo | |
| The introduction's comment calls them children "from the village", but the boys are made at the hideout by the script, not taken from the town | todo | |

## Unused or cut parts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Cut start condition: the comments say the children would appear only when the town's storage pit ran alarmingly low, and the scroll only after at least one cow had been stolen; the storage pit is looked up and a food and a starving counter set up, but none is ever used, and the wait for the first stolen cow is commented out | todo | |
| The hideout house is looked up but never used; the slaughtered-cow count is kept but never read; an unused local alignment total and a commented-out debug size exist | todo | |
| A commented-out wander-around state for the children, and a commented-out condition that a cow only counts as saved once more than 50 from the gang | todo | |
| A stand-alone test challenge that just runs the quest exists in the sources but is not in the compiled game | todo | |
| Unused lines in the text table: good advisor "Someone is praying to you here."; evil advisor "We should punish someone."; good advisor "But those children are hungry!" (an alternative advisor exchange) | todo | no script uses them |
| Unused boys' lines: "Well I'm still going to. I'm hungry."; "He's dead! We should stop this. It's gone too far."; "Let's get back to the Village."; and "I'm scared. I don't wanna do this any more." (the scare scene's comment quotes this one, but the script uses "I'm scared. I'm not stealing cows any more.") | todo | suggests a cut ending where the farmer's death ends the thieving and one boy refuses to stop |
| Two filler lines under the quest's prefix: "Sending synchronised map file" and "NOT USED" | todo | |
