# The Big Whale

A cut first-land challenge, a "Jonah" story: a fisherman's son has been swallowed by a giant whale out at sea; once the
player's creature goes to the water the whale is thrown up onto the shore, and hitting it makes it spit the son out,
for a storm miracle seed from the sky. It was never compiled into the game, and none of its lines exist in the text
table. Its script once borrowed the title "The Beach Temple Puzzle", which the shipped game uses for Land 2's beach
temple ([the_beach_temple_puzzle.md](./the_beach_temple_puzzle.md)), but that line is commented out.

**Land:** 1 (an early version of it; never started) · **Giver:** a Norse fisherman by his house · **Script:** BigFish · **Reward:** a storm miracle seed dropped from the sky · **Repeatable:** no

**Progress: 0/0 done, 0 partial — 0%**

Sources: the script's source text, the uncompiled early Land 1 control script that starts it, the camera path names in
the script headers, the land map scripts and the game's text table. Every row is n/a: the game never runs this quest.

## Is it in the game?

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Not in the compiled challenge file; only an uncompiled early Land 1 control script (one that also starts the cut landslide and the cut sculptor's rock) and a single-quest test launcher start it | n/a | never started by the game |
| Its challenge is named for a whale, and its dialogue lines (the "big whale" lines and its reminder) are all missing from the text table; its advisor nag uses a line from Land 2's lifeguard quest that is missing too | n/a | never started by the game |
| It calls the scroll's nag helper with one argument too many for the shipped helper, so it would no longer compile against the shipped scripts | n/a | never started by the game |
| Its fisherman's house matches no house on the shipped Land 1 (the nearest house at that spot is on Land 3, about 38 away), so the land's layout changed after it was written | n/a | never started by the game: checked against every `Land<N>.txt` |
| The headers still name its camera path ("big whale"), but the shipped data has no whale model or path for it | n/a | never started by the game |
| Not reachable at all in the shipped game | n/a | never started by the game |

## Set-up and introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A whale is made at the start of its camera path, at double size, and a Norse fisherman at his house | n/a | never started by the game |
| A silver scroll appears over the house; the evil advisor nags within 100 until it is clicked | n/a | never started by the game |
| The challenge record lines (open and close) are commented out, so it would never show in the challenge log | n/a | never started by the game |
| Cut scene: the fisherman steps out of the house; the camera glides to him (6 seconds) and he speaks; he turns to the camera and speaks again while the camera swings out to sea (5 seconds); a "fish in the water" shot was still to do | n/a | never started by the game: two lines, text missing |
| Then both advisors come out for four lines | n/a | never started by the game: text missing |

## What the player does

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest waits until the player's creature is within 80 of a spot out at sea south of the house, then makes the creature idle | n/a | never started by the game |
| Cut scene: the camera cuts back to the shore; two lines; the whale swims its path; a giant (three times size) man is made on the beach to stand for the whale thrown up on the shore, with a 1 second camera shake within 100; another line | n/a | never started by the game: the real "whale on the shore, crowd gathers, voice from inside" was still to do |
| The swimming whale is removed; three more lines | n/a | never started by the game |
| The quest waits until the beached "whale" is hit by something (the plan: throw a rock at it so it spits out the son) | n/a | never started by the game |
| Cut scene with three closing lines; the creature is released | n/a | never started by the game |
| No timer, no failure and no way to give up | n/a | never started by the game |

## Reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A storm miracle seed falls from the sky beside the house, with the reward's own camera move (one of two random swoops, then following the seed down) | n/a | never started by the game: the shared reward-from-sky script |
| A planned ending where the whale swims away into the distance was never written | n/a | never started by the game |
