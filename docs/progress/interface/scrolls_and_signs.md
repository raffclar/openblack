# Scrolls and signs

The scrolls that float over the people and places of a land to mark a challenge or a tip, and the signposts the player
reads. Which challenges each land has, and what its gold and silver scrolls lead to, are in ../story/; the scrolls
inside the temple are in ../temple/.

**Progress: 1/17 done, 1 partial — 9%**

## Challenge scrolls

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A land script puts a scroll up at a place or over a character, tied to a challenge | todo | `CREATE_HIGHLIGHT` in `src/CHLApi.cpp` logs "not implemented" |
| Gold scrolls mark the story's challenges and silver ones the side challenges | todo | |
| A bronze scroll kind | todo | (unconfirmed where the game uses it) |
| Scrolls glint with sparkles in their colour | partial | the gold, silver and bronze glint effects exist as particle types (`src/Particles/ParticleTypes.cpp`) with nothing to emit them |
| All scrolls pulse together, smoothly, once every turn of a shared clock | todo | |
| Tapping a scroll starts its challenge, shown by its active sparkles | todo | the active effects exist as particle types only |
| A started scroll stays as a reminder: tapping it again replays the challenge's last message | todo | |
| The hand can only tap a scroll inside the player's influence (unless the scroll says otherwise) | todo | |
| Scrolls can't be picked up, burnt, crushed or knocked over, and the creature leaves them alone | todo | |
| A scroll can be set to draw at a height and turned on or off by scripts | todo | `SET_DRAW_HIGHLIGHT`, `HIGHLIGHT_PROPERTIES` log "not implemented" |
| The advisors point the scroll out when it comes up, or at random later | todo | see [help_system.md](help_system.md) |
| Scrolls are kept in saved games | todo | see ../engine/ |
| The temple's world room can hide or show every challenge's scroll | todo | see ../temple/ |

## Tips and signs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| "Did you know" scrolls in the world give a tip when tapped | todo | one on Land 2 tells of fireflies: [../nature/fireflies.md](../nature/fireflies.md#hints-in-the-game) |
| A scroll can be the way into the land's vortex to the next land | todo | see ../story/ |
| Signposts that show their words when the hand is over them | todo | (unconfirmed which lands have them and what they say) |
| The temple's room signs, lit for the door under the hand | done | `src/3D/TempleSigns.cpp`; see ../temple/ |
