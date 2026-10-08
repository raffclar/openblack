# Challenge natives: advisors, text and help

The challenge scripts' functions for the good and evil advisors, the dialogue box and its spoken text, text drawn on screen, the help system, challenge markers (scrolls) and the tutorial's hand demonstrations and interface levels. The game has 464 of these functions in all; the language statement each comes from is shown in italics, and "called" counts are calls in the shipped `challenge.chl`. How the virtual machine runs them is in [../engine/script_vm.md](../engine/script_vm.md); what each challenge is about is in [../story/](../story/).

**Progress: 0/44 done, 1 partial — 1%**

## Used by the shipped scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Brings the good or evil advisor out onto the screen: *eject good spirit/evil spirit* (called 460 times in 167 scripts) | todo | `SpiritEject` logs "not implemented" |
| Sends the good or evil advisor back off the screen: *send good spirit/evil spirit home* (called 266 times in 101 scripts) | todo | `SpiritHome` logs "not implemented" |
| Makes an advisor point at a place, on screen or in the world: *make good spirit/evil spirit point at ‹position› [in world]* (called 131 times in 61 scripts) | todo | `SpiritPointPos` logs "not implemented" |
| Makes an advisor point at an object, on screen or in the world: *make good spirit/evil spirit point to ‹target› [in world]* (called 19 times in 12 scripts) | todo | `SpiritPointGameThing` logs "not implemented" |
| Shows a line of text (and plays its speech) in the dialogue box, optionally waiting for the player to click on: *say [single line] ‹text id› with interaction/without interaction* (called 1767 times in 264 scripts) | todo | `RunText` logs "not implemented" |
| Shows a line of text for a moment without the dialogue box: *say [single line] ‹string› with interaction/without interaction* (called 2 times in 2 scripts) | todo | `TempText` logs "not implemented" |
| Whether the text on screen has been read (its speech finished or clicked past): *read* (called 1727 times in 267 scripts) | todo | `TextRead` logs "not implemented" |
| Sets how much of the interface the player may use (the tutorial opens it up step by step): *set interaction ‹level›* (called 25 times in 8 scripts) | partial | `SetInterfaceInteraction`: camera controls by level through the camera help system; the level's two interface flags todo |
| Makes an advisor look at an object: *make good spirit/evil spirit look at ‹target›* (called 4 times in 4 scripts) | todo | `LookGameThing` logs "not implemented" |
| Takes the dialogue box for the script (start of the language's dialogue block); waits while another script has it: *begin dialogue (opening the block)* (called 626 times in 265 scripts) | todo | `StartDialogue` logs "not implemented" |
| Lets go of the dialogue box (end of the dialogue block): *end dialogue (closing the block)* (called 748 times in 296 scripts) | todo | `EndDialogue` logs "not implemented" |
| Whether the dialogue box is free for this script: *dialogue ready* (called 15 times in 13 scripts) | todo | `IsDialogueReady` logs "not implemented" |
| Stops an advisor pointing: *stop good spirit/evil spirit pointing* (called 68 times in 38 scripts) | todo | `StopPointing` logs "not implemented" |
| Stops an advisor looking: *stop good spirit/evil spirit looking* (called once in 1 script) | todo | `StopLooking` logs "not implemented" |
| Makes an advisor look at a position: *make good spirit/evil spirit look at ‹position›* (called 10 times in 5 scripts) | todo | `LookAtPosition` logs "not implemented" |
| Makes an advisor cling to the edge of the screen at a place: *make good spirit/evil spirit cling across ‹x percent› down ‹y percent›* (called 21 times in 12 scripts) | todo | `ClingSpirit` logs "not implemented" |
| Makes an advisor fly to a place on the screen: *make good spirit/evil spirit fly across ‹x percent› down ‹y percent›* (called 2 times in 1 script) | todo | `FlySpirit` logs "not implemented" |
| Shows text with a number in it in the dialogue box: *say [single line] ‹string› with number ‹number› with interaction/without interaction* (called once in 1 script) | todo | `RunTextWithNumber` logs "not implemented" |
| Whether an advisor is speaking: *good spirit/evil spirit speaks ‹text id›* (called 13 times in 6 scripts) | todo | `SpiritSpeaks` logs "not implemented" |
| Gives a help text by number: *get ‹object› help* (called 2 times in 2 scripts) | todo | `GetHelp` logs "not implemented" |
| Plays a recorded demonstration of the hand (moving, grabbing, casting) for the tutorial: *start hand demonstration ‹string› [with pause on trigger] [without hand modify]* (called 18 times in 17 scripts) | todo | `PlayHandDemo` logs "not implemented" |
| Whether a hand demonstration has finished: *hand demonstration played* (called 17 times in 16 scripts) | todo | `IsPlayingHandDemo` logs "not implemented" |
| Creates a challenge marker (the bronze, silver or gold scroll and its signpost) at a position: *create highlight ‹type› at ‹position› ‹challenge id›* (called 82 times in 78 scripts) | todo | `CreateHighlight` logs "not implemented" |
| Makes an advisor appear: *make good spirit/evil spirit appear* (called 20 times in 9 scripts) | todo | `SpiritAppear` logs "not implemented" |
| Makes an advisor disappear: *make good spirit/evil spirit disappear* (called 20 times in 9 scripts) | todo | `SpiritDisappear` logs "not implemented" |
| Whether challenge markers are drawn: *enable/disable highlight draw* (called 3 times in 3 scripts) | todo | `SetDrawHighlight` logs "not implemented" |
| Sets the text and category of a challenge marker: *set ‹object› text properties text ‹text› category ‹category›* (called 2 times in 2 scripts) | todo | `HighlightProperties` logs "not implemented" |
| Whether a hand demonstration has reached its trigger: *hand demonstration trigger* (called 33 times in 12 scripts) | todo | `HandDemoTrigger` logs "not implemented" |
| Gives the first help text of a group: *get ‹object› first help* (called once in 1 script) | todo | `GetFirstHelp` logs "not implemented" |
| Gives the last help text of a group: *get ‹object› last help* (called once in 1 script) | todo | `GetLastHelp` logs "not implemented" |
| Clears the dialogue box: *clear dialogue* (called 18 times in 10 scripts) | todo | `GameClearDialogue` logs "not implemented" |
| Closes the dialogue box: *close dialogue* (called 165 times in 70 scripts) | todo | `GameCloseDialogue` logs "not implemented" |
| Draws text on the screen at a place, a size and fading in: *draw text ‹text id› across ‹across› down ‹down› width ‹width› height ‹height› size ‹size› fade in time ‹fade› seconds* (called 32 times in 2 scripts) | todo | `GameDrawText` logs "not implemented" |
| Draws text on the screen for a moment: *draw text ‹string› across ‹across› down ‹down› width ‹width› height ‹height› size ‹size› fade in time ‹fade› seconds* (called 266 times in 1 script) | todo | `GameDrawTempText` logs "not implemented" |
| Fades all drawn text out: *fade all draw text time ‹time› seconds* (called 26 times in 3 scripts) | todo | `FadeAllDrawText` logs "not implemented" |
| Sets the colour of drawn text: *set draw text color red ‹red› green ‹green› blue ‹blue›* (called 61 times in 2 scripts) | todo | `SetDrawTextColour` logs "not implemented" |

## Not used by the shipped scripts

The game has these but no shipped script calls them; mods and fan-made challenges can.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Makes an advisor play an animation across the screen: *make good spirit/evil spirit play across ‹value› down ‹value› ‹value› [speed ‹value›]* (not called by the shipped scripts) | todo | `PlaySpiritAnim` logs "not implemented" |
| Whether an advisor has finished an animation: *good spirit/evil spirit played* (not called by the shipped scripts) | todo | `SpiritPlayed` logs "not implemented" |
| Whether the help system is on: *help system on* (not called by the shipped scripts) | todo | `HelpSystemOn` logs "not implemented" |
| Shows the shape of a gesture at a position: *animate gesture ‹value› at ‹value› radius ‹value›* (not called by the shipped scripts) | todo | `PlayGesture` logs "not implemented" |
| Shows text with a number in it for a moment: *say [single line] ‹format› with number ‹value› with interaction/without interaction* (not called by the shipped scripts) | todo | `TempTextWithNumber` logs "not implemented" |
| Turns the help system on or off: *enable/disable help system* (not called by the shipped scripts) | todo | `SetHelpSystem` logs "not implemented" |
| Makes an advisor point at a place on the screen: *make good spirit/evil spirit point across ‹value› down ‹value›* (not called by the shipped scripts) | todo | `SpiritScreenPoint` logs "not implemented" |
| Lets the hand demonstrations be stepped with keys: *enable/disable hand demonstration keys* (not called by the shipped scripts) | todo | `SetHandDemoKeys` logs "not implemented" |
