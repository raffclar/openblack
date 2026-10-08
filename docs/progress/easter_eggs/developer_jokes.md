# Developer jokes

Jokes, puns, nods to Lionhead and its people, and developers' messages to each other, in the game's text and inside the
program. The game's text is in `Scripts/InfoScript2.txt` (UTF-16); the in-game credits are owned by
[../temple/library_room.md](../temple/library_room.md); the named villagers taken from the staff list by
[../villager/special_villagers.md](../villager/special_villagers.md).

**Progress: 0/7 done, 0 partial — 0%**

## In the game's text

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Asking the advisors about a lion creature can get "A Lion. With a nice head. Lionhead. Get it?"; about a horse, "Your Horse. I'm a little hoarse myself. Aha. Ha."; about the ogre, "The Ogre. He has his eye on you. That was a joke, Leader." | todo | the answers to the help query, see [../interface/help_system.md](../interface/help_system.md) |
| Asking about something the advisors find dull can get "Maybe this is something we'll put in the sequel." | todo | same |
| Villagers' grumbles include fourth-wall lines: "I wonder if I'll get a part in the sequel?", "I had to audition for this. Can you believe it?", "I could have been someone. I could have been in Dungeon Keeper.", "I know Peter Molyneux personally.", "Lord, won't you buy me a Mercedes Benz?", "It feels like someone's watching me." | todo | villager banter sets, men's and women's; when each set is shown is unconfirmed; see [../villager/looks_and_voices.md](../villager/looks_and_voices.md) |
| The advisors' own banter includes "Are we gonna figure in the sequel?", "Maybe we can tip over the monitor!" and "I got a CD of speed-thrash hymns in my car!" | todo | see [../story/advisors.md](../story/advisors.md); when each set is said is unconfirmed |
| The man who wants to be thrown says "I can see your mouse from here." | todo | see [../story/minigames.md](../story/minigames.md) |
| Puns in the challenges: "Gutted! Ha! Fishermen. Gutted. Get it?" (the drowning fishermen), "Wow. The saintly one made a joke." (the lost flock), "Hey! A Palm Pony. Oh dear. I'm not very good at jokes." | todo | see [../story/](../story/) |
| The phone box's "One of the Lionhead people." comes from a set of hidden-script lines written for a Lionhead person's birthday ("Hey Boss! Today is a special day.", "Yes. It's someone very important's birthday.", "So many happy returns to them!") and Christmas ("Happy Christmas!", "Bah, humbug!"); no challenge script says the birthday or "Happy Christmas!" lines, and "Bah, humbug!" is reused by the evil advisor at the Land 2 sacrifice when a small sacrifice is made | todo | the phone box is in [hidden_content.md](hidden_content.md); whether the program itself says the birthday lines on special days belongs to [../pc_integration/](../pc_integration/) |

## Inside the program

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| If the program's linked lists ever went wrong it would stop with "Peter: Damn Richard won the bet" (a node with no node before it), among other "Peter:" messages | n/a | only shown on an internal error, never in normal play; nothing to port |
| Other developer-signed error messages: "JONTY-When is a flock not a flock?", "JONTY- Something is seriously wrong with my code", "Jonty-Thing should be in script! OH CALL ME!!!!", "No thing - BAD MAN!", "Script offsets exceeded - Blimey!", "Jeremy: failed miserably save has - tell me.", "Richard: no subactions?", "Wierd - this bitstring (11b) is reserved." | n/a | internal errors only |
| The creature's actions include "kiss friend's arse", "tell creature to sod off", "watch telly" and "fart", and the challenge language has a command to get a creature's "arse position" (where its poo comes from) | n/a | names inside the program; the behaviours are in [secret_behaviours.md](secret_behaviours.md) and [unused_content.md](unused_content.md) |
| A key layout named "Daniel's Key Config mofo, dont mess" | n/a | see [hidden_keys_and_cheats.md](hidden_keys_and_cheats.md) |
| A message left in the program for testers: "This is a developer beta patch. There will be no support from EA regarding this patch for Black & White. ... please fill in the bugreport.txt file included with the patch and email it to bwbeta1@lionhead.com" | n/a | whether any build shows it is unconfirmed |
