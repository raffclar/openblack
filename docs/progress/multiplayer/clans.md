# Clans

Clans of players who play internet games together: a team can play for a clan, which then scores instead of its
players, brings its own creature from game to game, and shows its logo as the team's symbol. The game asks the online
servers for all of it; the clans themselves were run on the game's web site.

**Codebase: bwgame-service** (`C:\projects\bwgame-service`), the stand-in for the game's online servers and web site,
not openblack. Every "done" below is done there; paths in the notes are relative to that repository. What the game
itself does with clans in a match is in [multiplayer_rules.md](multiplayer_rules.md) (openblack).

**Progress: 40/40 done, 0 partial — 100%**

## What the game asks the servers for

Checked against the original game's requests and how it reads the replies.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game lists the clans a player belongs to | done | bwgame-service: `online/services.py` (clan list); `tests/test_clans.py::test_game_clan_list_puts_the_first_joined_clan_first` |
| Each clan in that list carries a yes/no flag | done | the game stores it but never reads it (checked in the Windows and Mac versions), so it changes nothing; bwgame-service sends "yes" for the clan's leader. Clan numbers are never 0, which the Mac version would skip |
| A player's details show their clan | done | bwgame-service: the clan they joined first (the game shows one) |
| The clan details: name, logo, and the lines "Clan Description:", "Clan Leader:", "Clan URL:" and "Credits" | done | bwgame-service: the motto, the leader, the web site and the clan's points, in the places the game labels them (the labels come from the game's own text files); two unused places are left empty |
| The clan logo is a picture file the game loads, keeping a 64 by 64 shape in 16 shades | done | bwgame-service: `online/logo.py` makes exactly that from any uploaded image; `docs/research/clan-logo.md`; `tests/test_clans.py::test_logo_is_the_bmp_the_game_reads` |
| A team picking a clan gets the clan's creature | done | bwgame-service: the stored creature, sent back exactly as the game uploaded it |
| A clan with no creature yet tells the game to create one of the chosen type and name | done | bwgame-service: the type is the row of the game's creature table, from the Giant Ape (0) to the Gorilla (16); the game doesn't check it, so nothing else is ever sent. Any of the 17 can be chosen, unlocked in single-player or not; `tests/test_clans.py::test_clan_creature_numbers_are_the_games_table_rows` |
| At the end of a clan game, the team leader's game uploads the clan creature | done | bwgame-service: kept per clan; only from a member who gives their password |
| At the end of an internet game, the host reports the results and gets everyone's new points | done | bwgame-service: `online/services.py` (results) |
| A team playing for a clan scores for the clan instead of its players | done | bwgame-service: the ranking formula itself is the server's own (the original's is unknown), an Elo-style update from 1000 points |
| Clan names and texts reach the game in its character set | done | bwgame-service: names, mottos, web sites and creature names are limited to characters the game can show |
| The clan logo becomes the symbol of a team playing for the clan, over its towns and on its hand | n/a | done by the game itself; bwgame-service only supplies the logo. openblack: see multiplayer_rules.md |
| The clan logo shows in the multiplayer screens' clan box | n/a | done by the game itself, as above |

## Protection of clan scoring

Not in the original as far as is known; the server's own rules, so that results can't be made up.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Results count only when sent with the password of an account that played in that game | done | bwgame-service: `tests/test_abuse.py::test_results_need_a_valid_password`, `test_results_only_from_a_player_of_that_game` |
| A team scores for a clan only when all its players are members of it | done | bwgame-service: otherwise its players score for themselves; `tests/test_abuse.py::test_a_team_scores_for_a_clan_only_when_all_are_members` |
| Oversized result reports are refused | done | bwgame-service: 64 KB at most |

## Running a clan on the web site

The original site's clan pages are lost; how they worked is unknown. This is bwgame-service's own design, built so the
game's side above works. Tests in `tests/test_clans.py`.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A signed-in player founds a clan and leads it | done | bwgame-service: `online/clans.py`, `web/views.py` |
| A clan has one leader, officers and members | done | bwgame-service: `online/models.py` (memberships with an officer mark) |
| Each clan chooses who can join: anyone, by request, or by invitation only | done | bwgame-service |
| Players ask to join; the leader or an officer approves or declines; the player can withdraw | done | bwgame-service |
| The leader or an officer invites a player by name; the player accepts or declines; the invitation can be withdrawn | done | bwgame-service: open invitations show on the player's account page |
| An invitation lets a player in whatever the clan's joining rule | done | bwgame-service |
| The leader or an officer removes members; officers can't remove officers or the leader | done | bwgame-service |
| Removed players can be banned from rejoining, asking or being invited, and the ban lifted | done | bwgame-service |
| The leader makes members officers and back | done | bwgame-service |
| The leader hands the leadership to a member, becoming an officer | done | bwgame-service |
| A leader who leaves is followed by the longest-serving officer, else the longest-serving member | done | bwgame-service |
| A clan whose last member leaves is disbanded | done | bwgame-service |
| The leader disbands the clan, typing its name to confirm | done | bwgame-service: members, requests, invitations and the creature go with it; past results stay |
| The leader edits the name, motto, web site, joining rule, and the creature's type and name | done | bwgame-service |
| The leader uploads a logo, shown on the site as the game will draw it | done | bwgame-service: the image's transparency (or brightness, optionally inverted) becomes the shape; previewed in a player colour |
| The leader removes the logo | done | bwgame-service |
| The leader resets the clan creature, so the next clan game starts a new one | done | bwgame-service |
| A player belongs to at most a few clans, and a clan has at most a set number of members | done | bwgame-service: 3 and 100 by default |
| Founding clans, joining requests and clan changes are rate-limited | done | bwgame-service: `accounts/throttle.py` |
| Clan names, mottos, web sites and creature names are checked for blocked words, reserved names and hidden characters | done | bwgame-service: `moderation/`; web sites must be http or https addresses |
| The clan list shows each clan's logo, leader, members, joining rule and points | done | bwgame-service |
| The ranking lists clans by points | done | bwgame-service |
| Clan tags shown beside players' names | n/a | the game has no clan tags; a player could only type one into their own name |

## Moderation

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every new logo is put on the admins' review list (pictures aren't checked automatically) | done | bwgame-service: Moderation → Flags |
| Admins see and change a clan's members, requests, invitations and bans | done | bwgame-service: the clan's admin page |
| Admins remove a clan's logo, or clear its motto, web site and creature name | done | bwgame-service: actions on the admin's clan list |
| Clan text that matches a blocked word added later is hidden on the pages | done | bwgame-service |
