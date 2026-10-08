# The Miracle Stones

A cut stone-circle challenge: a mother kneels by her dead child beside a half circle of five singing stones and three
empty stone bases. Clicking two stones that sing the same note an octave apart makes a "pair"; the first pair heals
the child, the second drops food and wood, the third does nothing visible, and the fourth, which needs the three
missing stones found around the land and set in their bases, brings a chimp creature for the player to swap into. It
was never compiled into the game.

**Land:** 2 (never started) · **Giver:** an Indian mother at the stone circle · **Script:** MoreSingingStones · **Reward:** the child brought back to life, a large food and a large wood reward, and a chimp creature to swap for · **Repeatable:** no

**Progress: 0/0 done, 0 partial — 0%**

Sources: the script's source text and its single-quest test launcher, the script compiler's project and quest menu
lists, the land map scripts, the two shipped singing stone challenges, the shared notify, reminder, reward and creature
swap scripts, and the game's text table. Every row is n/a: the game never runs this quest. To restore it openblack would
need highlights, villagers, clicking objects, spells cast by script, special effects, rewards from the sky and the
creature swap, which are all stubs in `src/CHLApi.cpp` (its `Create` makes the stones and bases, being mobile statics,
but not the villagers or the chimp).

## Is it in the game?

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Not in the compiled challenge file: it is missing from the compiler's project list, and no land's control script or challenge list starts it | n/a | never started by the game |
| Only its single-quest test launcher starts it; it is not even in the developers' quest menu | n/a | never started by the game |
| The third pair starts a background script for a curse on the nearby town that exists in no source file, so this file can't be built as it stands | n/a | never started by the game |
| Its title "The Miracle Stones", its three reminders and every spoken line exist in the text table (under their own "singing stones 2" set) | n/a | never started by the game |
| Its spots fit Land 2: the mother's house is a real Indian house of Land 2's neutral Indian village (town 11), and the "cursed town" spot is that village's centre; the circle is on open ground about 140 from Land 2's shipped singing stone horseshoe | n/a | never started by the game: checked against every `Land<N>.txt` |
| The three missing stones are hidden about 545 east, 860 south-west and 630 west of the circle | n/a | never started by the game |
| Not reachable at all in the shipped game | n/a | never started by the game |

## How it relates to the shipped singing stones

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land 1's "The Singing Stones" (a hippy's ring of eight holders; find the five missing stones and set them so the ring plays a rising scale; reward a food miracle dispenser) shares the idea of fetching missing stones from around the land and locking them into bases | n/a | never started by the game: see [the_singing_stones.md](./the_singing_stones.md) |
| Land 2's "The Singing Stones" (a priest's horseshoe of nine stones; tap three hidden melodies for night, the dead raised or snow) shares the land, the area and the idea of playing the stones by clicking them to make magic happen, including raising the dead | n/a | never started by the game: see [the_singing_stones_land_2.md](./the_singing_stones_land_2.md) |
| This version looks like a dropped alternative for Land 2's circle: matching octave pairs instead of tunes, rewards per pair, and a chimp at the end | n/a | never started by the game: no source says which came first |
| It uses the same singing stone object as both shipped quests for all eight stones, and the same stone base for its empty holders | n/a | never started by the game |
| Its chimp would have been the second chimp offered in the game; the only one that ships is The Rejuvenator's, given when the creature is already an ape | n/a | never started by the game: see [the_rejuvenator.md](./the_rejuvenator.md) |

## The circle and the scroll

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Once the circle is out of view, five singing stones are made on one half of a circle of radius 17.5, each facing the centre, and they can't be moved or picked up | n/a | never started by the game |
| Their notes, round the half circle: G, A, G an octave up, B, C | n/a | never started by the game: from the comments; every stone actually plays the same sound (see Sounds) |
| Three empty stone bases fill the other half of the circle, the places of A, B and C an octave up | n/a | never started by the game |
| A silver scroll appears about 37 west of the circle's centre; while the camera is within 100 and the scroll is in view, the evil advisor nags "Whew! As if you ain't busy enough, here's something for you to do." every 30 seconds until the scroll or the circle is clicked | n/a | never started by the game: the shared challenge notify script |
| Only then are the three missing stones made at their far-off spots | n/a | never started by the game |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An Indian housewife stands 5 east of the centre; a dead boy of 9 lies near the centre, playing a dead pose | n/a | never started by the game |
| Cut scene: the camera glides in 5 seconds to a low view across the circle; the mother faces the camera, kneels and prays | n/a | never started by the game |
| The challenge record opens as "The Miracle Stones", success and alignment 0, reminder "We need to make two matching stone sing to heal the boy." | n/a | never started by the game |
| Mother: "Holy one, these five stones are meant to bring people back from the dead." She gets up and gossips | n/a | never started by the game |
| She turns to the child and mourns; the camera turns to the child over 2 seconds; mother: "Please make them work. My poor child, you see." | n/a | never started by the game |
| After 2 seconds the camera returns over 3 seconds | n/a | never started by the game |

## Finding the missing stones

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A missing stone dropped (not held, not flying) within 5 of its own base snaps onto it, faces the centre, and can no longer be moved or picked up | n/a | never started by the game |
| Dropped within 5 of another empty base, it is given a little hop towards a point 10 above that base and does not lock in | n/a | never started by the game: it keeps hopping each time it lands there |
| When all three are set, the good advisor: "All the stones are in place. Now we need to activate them." | n/a | never started by the game |

## Playing the stones

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Clicking a stone whose pair is not yet made (a missing one only once set in its base) picks it as the first note, plays its sound and puts a sparkle 10 above it | n/a | never started by the game |
| Clicking a different such stone picks the second note, plays its sound and puts a sparkle over it | n/a | never started by the game |
| The pairs are G with G (the two original stones), and A, B and C each with their missing octave stone; either order counts | n/a | never started by the game: so only the G pair can be made before stones are found |
| After any second note, right or wrong, both note sparkles and the choice are cleared (except after the fourth pair, which ends the puzzle) | n/a | never started by the game |
| A first note left without a second for about 10 seconds is cleared | n/a | never started by the game: counted in game turns, a comment assuming ten a second |
| Each made pair gets a lasting sparkle 15 above both its stones within about half a second | n/a | never started by the game |
| Rewards go by how many pairs are made, not by which pair | n/a | never started by the game |

## First pair: healing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Cut scene: camera to a raised view of the circle (3 seconds); good advisor: "Aha. The same note but an octave apart!" (a comment says to cut it if it makes the puzzle too obvious) | n/a | never started by the game |
| The camera swings round over 3 seconds while a strong heal miracle flies from the first G stone to the centre | n/a | never started by the game: always from that stone, whichever pair was made |
| If the child's body is within 10 of the centre, a heal miracle hits him and he comes back to life, looks impressed, and the player's alignment rises by 0.5; otherwise it falls by 0.2 | n/a | never started by the game |
| If the mother is dead and her body is within 10 of the centre, she too is healed, brought back to life and set beside the centre | n/a | never started by the game |
| The record moves to 0.25 (alignment 0), reminder "There are stones missing. Maybe we should find them." | n/a | never started by the game |
| After 2 seconds the camera returns to the opening view; if both live, the mother faces the camera, prays and says "Eternal thanks, godly spirit." | n/a | never started by the game |
| If only the child lives: "Where's my mother? I want my mother!" | n/a | never started by the game: the line is voiced by the woman's voice |
| If the child was outside the circle, the good advisor comes out for "Woah. Powerful stuff. Pity the kid's not in the circle." | n/a | never started by the game: a line voiced by the evil advisor |
| The camera returns to where it was (3 seconds) | n/a | never started by the game |
| The living members of the family walk home at speed 0.3 to their house in the Indian village, about 285 north | n/a | never started by the game |
| Good advisor: "Those two stones are permanently set now."; evil advisor: "I wonder what'll happen if we activate them all."; and, if any stone is still missing, good advisor: "We need the stones from around this land." | n/a | never started by the game |

## Second pair: food and wood

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Cut scene: camera to 15 above and 15 off the centre (3 seconds); five sparkles over the centre for 5 seconds | n/a | never started by the game |
| A large food reward falls from the sky 5 east of the centre and a large wood reward 5 west, each showing its help when clicked | n/a | never started by the game: the shared reward help |
| The camera drifts lower over 8 seconds; the record moves to 0.5 with the alignment so far, reminder "Maybe something'll happen if we match up more stones." | n/a | never started by the game |
| Both advisors come out: "Wow. An honest-to-god Miracle." then "Hey! We've been granted some food! Go stones!" | n/a | never started by the game: the first line is in the woman's voice although only the advisors are out |
| After 2 seconds the camera returns | n/a | never started by the game |

## Third pair: the curse

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Cut scene: camera to 15 above and 15 off the centre; magic beams run from the centre to each of the five stones and to each set missing stone, for 5 seconds | n/a | never started by the game |
| After 2 seconds a beam runs from the Indian village's centre to the circle (a note asks for it in a different colour) | n/a | never started by the game |
| The record moves to 0.75, same reminder | n/a | never started by the game |
| Evil advisor: "Eh? Nothing. Nothing at all."; good advisor: "Hmm. The pair is right, the stones are set, but no Miracle." | n/a | never started by the game |
| A comment says the nearby town was to become "all 12 years old"; the curse script it starts was never written | n/a | never started by the game |

## Fourth pair: the chimp

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| All the pair sparkles are removed | n/a | never started by the game |
| Cut scene: camera to 15 above and 15 off the centre; five sparkles for 3 seconds hide the chimp's appearance | n/a | never started by the game |
| After a second a chimp creature appears at the centre, turns to the camera and waves | n/a | never started by the game |
| The record closes as a success with the alignment so far, with the opening reminder | n/a | never started by the game |
| Good advisor, pointing at the chimp: "A Chimp. Let's have it. We must."; evil advisor: "How?"; good advisor: "Bring your Creature here and click the Chimp to swap bodies." | n/a | never started by the game |
| The camera pulls back to 25 above and 25 off over 3 seconds and the cut scene ends there | n/a | never started by the game |
| The chimp is then offered through the shared swap offer | n/a | never started by the game: see [creature_swaps.md](./creature_swaps.md) |

## The family

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| If the mother dies before reaching home: "Way to go. You killed her. Eee-vil!" and the player's alignment falls by 0.6 | n/a | never started by the game |
| If the healed child dies before reaching home: "You killed the little one. After healing her, too! Drat!" and alignment falls by 0.8 | n/a | never started by the game: the line says "her" for the boy |
| Each who reaches the house is released to village life and alignment rises by 0.1 | n/a | never started by the game: see Bugs |
| Before the first pair, if the child's body is more than 10 from the centre and not in the hand, the good advisor says once "Move the child back into the circle to heal him." | n/a | never started by the game |

## Sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every stone, when clicked, plays the same placeholder: a muffled child's cry | n/a | never started by the game: comments ask for each stone's real note |
| Each made pair plays a placeholder voice clip borrowed from another quest at the centre | n/a | never started by the game: a comment asks for a big chord |
| The circle doesn't sing when the hand passes over it; a comment considers it but warns of the cost | n/a | never started by the game |

## Bugs and quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Reaching the house is checked every pass with no guard, so the +0.1 alignment repeats for as long as the mother or child stands at the house | n/a | never started by the game |
| After the first pair the script waits for the mother's animation to finish; if she was left in her endless mourning (the child was not healed), it may never finish | n/a | never started by the game: whether an endless animation ever counts as finished can't be told from the source |
| The 0.25 record ignores the alignment so far | n/a | never started by the game |
| The last record goes back to the opening reminder about healing the boy | n/a | never started by the game |
