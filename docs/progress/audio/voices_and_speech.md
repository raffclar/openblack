# Voices and speech

Spoken lines: the advisors and story characters reading their dialogue, villagers talking and singing, the creature's
voice, and the whispering voices that say the player's name.

**Progress: 2/15 done, 1 partial — 17%**

## Dialogue

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each line of dialogue has a recording, played as the line shows | todo | the dialogue banks (`Audio/Dialogue`) load but nothing plays them; the dialogue commands are stubs |
| The advisors' lines from their own banks (guidance and help sprites) | todo | see ../story/advisors.md |
| The advisors' lips move with their voice | todo |  |
| A line can be skipped and the voice stops | todo |  |
| Speech volume and subtitles settings | todo | see ../interface/options.md |
| Miracle lines spoken when a miracle is cast | todo | (unconfirmed when the spell dialogue bank is used) |
| The missionaries' three sung verses, with the words to follow | todo | see [the_explorers.md](../story/silver_scrolls/the_explorers.md) |

## Villagers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers talk in their tribe's babble | todo | see ../villager/looks_and_voices.md |
| Villagers praise or fear the player aloud | partial | see ../villager/looks_and_voices.md |
| Villagers sing while they dance and worship | todo | see ../worship/ |

## The creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature's voice from its species' bank, by size and alignment | done | `creature_audio::VoiceBank`, `CreatureAudioSystem`; test `test_creature_audio` |
| Its voice is heard only when close enough | done | `creature_audio::IsHeard` |
| Scripts change the creature's sounds | todo | stub in `src/CHLApi.cpp` |

## Other voices

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Spooky whispering voices now and then say the player's name, matched by how it sounds | todo |  |
| The hidden phone box's recorded messages | todo | see ../story/land_1.md |
