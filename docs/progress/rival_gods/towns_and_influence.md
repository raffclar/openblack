# How a computer god wins and loses towns

A computer god spreads its influence by winning towns the way a person does: impressing them, meeting their needs and
attacking its rivals' towns; it looks after its own towns and worshippers so they keep believing and praying. The story
lands add their own rules on top, holding some towns back until the right moment.

**Progress: 4/42 done, 1 partial — 11%**

See [../town/belief_and_conversion.md](../town/belief_and_conversion.md) for belief and conversion,
[../worship/influence.md](../worship/influence.md) for influence and [ai.md](ai.md) for how the god decides.

## What it starts with

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land scripts give each god its towns, by owner name | done | `CREATE_TOWN` (`FeatureScriptCommands::CreateTown`, `Town::owner`) |
| Land scripts give each god its temple | done | `CREATE_CITADEL` (`CitadelArchetype`) |
| Land scripts give each god worship sites by its temple, one per tribe | todo | `CREATE_WORSHIP_SITE` is ignored |
| Land scripts set how much each town believes in each god, and the most it can | partial | belief is written to a map the rules don't read; caps not implemented |
| A god's influence covers its temple and its towns | done | `InfluenceSystem` |
| Its influence ring is drawn in its colour | done | `InfluenceSystem`, `Player::k_Colours` |

## Choosing a town

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It finds the town it most wants to win, from every player's towns | todo | |
| It judges how much it wants to win or destroy each town | todo | |
| It finds which town of its own most needs defending, food, wood or people | todo | |
| It notices which of its towns has just been attacked | todo | `GET_TIME_SINCE_OBJECT_ATTACKED` is implemented for scripts; the god doesn't use it |
| It finds another town to take food or wood from | todo | |

## Winning towns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| By sending its creature to impress the town | todo | see [creatures.md](creatures.md) |
| By throwing things near the town to impress it | todo | |
| By casting impressive miracles at it | todo | see [magic.md](magic.md) |
| By meeting its food needs: food miracles, fields, fish farms or food carried from another town to its storehouse | todo | |
| By meeting its wood needs (never chosen when expanding, see [ai.md](ai.md)) | todo | |
| Belief it wins shows over the town in its colour, as the person's does | todo | the symbol rises only for the person's deeds (see [../town/belief_and_conversion.md](../town/belief_and_conversion.md)) |
| A town changes hands when its belief in the god passes what it needs | todo | |

## Destroying and defending

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It destroys a rival's town with miracles, with its creature, or by throwing things | todo | |
| It defends its own town with its creature or with miracles | todo | |
| It defends a town whose belief is under attack, with a miracle or by impressing it with its creature | todo | |
| It puts out its burning buildings with water | todo | |
| It sends its creature to help repair damaged buildings | todo | |
| It shields buildings left unprotected | todo | |

## Looking after its towns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It carries food and wood to the town that wants them most, or casts for them | todo | |
| It sends workers and wood to building sites | todo | |
| It places scaffolds from its workshop and combines small ones into bigger ones | todo | |
| It fills its workshop with wood | todo | |
| It makes breeder disciples to grow its towns | todo | |
| It makes disciples to meet a town's needs | todo | |
| It raises the town's totem (unconfirmed when) | todo | |

## Its worshippers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It takes villagers from its towns to the worship site when the site needs them | todo | see [../worship/worship_sites.md](../worship/worship_sites.md) |
| It feeds hungry worshippers: food on the site, or carried from fields, fish farms or other towns | todo | |
| It rests tired worshippers by healing them at the site or taking them home | todo | |

## Story rules

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| On the second land, a script checks each of eleven towns for a change of owner every tenth of a second and makes the gods react | todo | see [khazar.md](khazar.md) and [lethys.md](lethys.md) |
| On the second land, both gods' towns can hold at most 0.75 belief in either god | todo | |
| On the third land, the player can't win Lethys's last town until their creature is free, then only once his lead is under 0.4 | todo | see [lethys.md](lethys.md) |
| On the fifth land, Nemesis's home town can't be won while he holds any other town | todo | see [nemesis.md](nemesis.md) |
| On the fifth land, Nemesis's home town also counts as won if it shrinks to six or fewer (unconfirmed whether this script runs) | todo | |
| Scripts count each player's towns to decide when gods die and vortexes open | todo | `GET_PLAYER_TOWN_TOTAL` is a stub |
| Scripts compare each player's influence at the camera to make gods react to trespassing | todo | `GET_INFLUENCE` is a stub |
| The person gets "virtual influence" on the third and fifth lands | todo | `SET_VIRTUAL_INFLUENCE` is a stub; see [../worship/influence.md](../worship/influence.md) |
