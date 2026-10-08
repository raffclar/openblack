# The gathering box and the ping server

While a game world runs, the game keeps in touch with Lionhead's "ping server" (`bwping.bwgame.com`, UDP port 2611):
it says it is playing, asks which of its friends are online, and takes messages for its gathering box, the in-game box
of players, friends and others. Lionhead's staff used it to speak to players under ids the game draws in yellow.

**Codebase: bwgame-service** (`C:\projects\bwgame-service`), the stand-in for the game's online servers, not openblack.
Every "done" below is done there; paths in the notes are relative to that repository. The full protocol is in its
`docs/research/bwping.md`. What the gathering box itself does in the game is openblack's, listed as n/a here.

**Progress: 13/13 done, 0 partial — 100%**

## What the game sends and the server answers

Checked against the original game's code.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every 20 seconds of play the game pings the server with its player's id; it expects no answer | done | bwgame-service: `bwgame/bwping.py`; a ping counts once the account passes the chat lobby's check (an active account whose game login came from that address); `tests/test_bwping.py` |
| It sends the ids of its player's friends (at most 25); the server answers with those who are online and where | done | bwgame-service: friends who are playing are listed at the server's own address, and their chat through the gathering box is relayed, so players never see each other's address (`BW_PING_FRIENDS`: relay, direct or off) |
| A friend listed online keeps the online mark for 7.5 minutes | done | bwgame-service answers every request, every 20 seconds |
| Chat between players in the gathering box goes to the address the game holds for that player | done | bwgame-service: relayed to the friend's game under the sender's account name; blocked words are not passed on |
| The host of an internet game reports its teams and the map's conditions every 20 seconds and when the game is decided | done | bwgame-service: kept per host, shown in the admin (Online → Host reports); one of its numbers and its flag are not understood (unconfirmed) |
| The game takes these messages from anyone, in single packets with no handshake | done | bwgame-service: each address is rate-limited; broken packets are dropped as the game would |

## Staff in the game

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Players with ids 9999999 to 10000014 are drawn in yellow in the gathering box | done | bwgame-service: staff accounts get one of these ids in the admin; no player's id can be one (the game's own ids are far larger) |
| A message from id 9999999, the admin, opens every player's gathering box | done | bwgame-service: the admin account has 9999999 |
| A staff member's chat line shows as "name: text" in the gathering box, and in the in-game chat and its log | done | bwgame-service: Online → Server messages, to everyone playing or to one player (who gets it when their game next pings, within an hour) |
| A message starting with a secret word opens a Yes/No question, or a box to type a line into; the answer goes back to the sender | done | bwgame-service: the answers are kept with the question; a player can turn these dialogs into plain chat with a setting |
| What a player types to a staff entry in the gathering box reaches the server | done | bwgame-service: Online → Player replies |
| Lionhead's announcements: a text shown as up to 15 yellow names in the "Others" list | done | bwgame-service: the "announcement" message kind; they stay until the game is closed |
| Who is playing right now | done | bwgame-service: Accounts → Players → In game: Now |
| The gathering box's lists, friends list and chat window themselves | n/a | done by the game itself (openblack: not started) |
