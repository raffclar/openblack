---
name: openblack-code-comments
description: Use whenever you write or change a code comment in any openblack checkout or worktree. All code comments must follow strict ASD-STE100 Simplified Technical English (STE), describe behaviour in plain words, and never cite vanilla addresses, Mac symbols, assembly or vanilla internal names.
---

# openblack code comments: ASD-STE100 Simplified Technical English

Every code comment in openblack follows the ASD-STE100 Simplified Technical English (STE) specification strictly.
This applies to all code comments: `//` and `/* */` comments, `///` documentation comments, comments in shaders,
CMake files, scripts and tests, in every openblack checkout and worktree.

It applies to comments you write or change. When you edit a block of code, also make the comments in that block obey
these rules. Do not rewrite comments in code that you do not otherwise touch.

## What a comment says

- Describe what the code does and why, in terms of the game's behaviour: "The villager walks at the speed of the
  state that it goes to", not where the behaviour is in the original binary.
- Do not write vanilla addresses (`0x...`), Mac symbols, assembly, or vanilla internal function names (`fn_...`).
  Keep that evidence in the bw1-decomp notes and the research documents.
- Write in English only.

## Core rules (the project checklist)

Every STE text (a comment, a commit message) must obey all of these:

1. Keep procedural sentences to 20 words or fewer; descriptive sentences to 25 words or fewer.
2. One instruction per sentence.
3. Use the active voice.
4. Use the present tense for procedures.
5. Start an instruction with the command verb ("Remove the bolt.").
6. Use articles ("the file", not "file").
7. Do not use a noun as a verb.
8. Use only approved words, with one meaning per word (see `DICTIONARY.md` at the root of the repository: openblack and bw1-decomp have the same file).
9. Write one topic per paragraph; lead with the topic sentence.

The rules below give more detail.

## STE rules

- **Words:** use only STE approved words, with their approved meaning and part of speech. You can use technical
  names as they are: names of code (`PickingSystem`, `CreatureMind`, `k_Names`), names of files, and terms of the game
  (villager, creature, temple, miracle, leash, worship site, scroll).
- **One meaning for one word.** Use the same word for the same thing every time.
- **Sentence length:** a procedural sentence (an instruction) has a maximum of 20 words. A descriptive sentence has a
  maximum of 25 words.
- **One topic in one sentence.** Divide a long sentence into two or more sentences.
- **Voice:** use the active voice. Do not use the passive voice: write "The hand drops the globe", not "The globe is
  dropped".
- **Instructions:** use the imperative ("Clear the list before the next turn").
- **Tenses:** use only the simple present, the simple past and the simple future.
- **No -ing forms** as nouns or adjectives: write "the code that picks", not "the picking code", unless it is a
  technical name.
- **No phrasal verbs:** write "start", "do", "continue", "remove", not "set up", "carry out", "go on", "take away".
- **Articles and demonstratives:** use "the", "a", "this", "these" where it is possible. Do not write in a telegraphic
  style.
- **No contractions, slang or idioms:** write "do not", "it is".
- **Noun clusters:** do not join more than three nouns. Divide a longer cluster ("the speed of the villager's walk",
  not "villager walk speed value").
- **Paragraphs:** one topic in one paragraph; a descriptive paragraph has a maximum of six sentences.
- **Lists:** use a list for a sequence of steps or a set of conditions, not a long sentence.

## Check before you commit

Read every comment that you wrote or changed against the rules above. Count the words in long sentences. Change the
passive voice and the -ing forms. The commit message rules are in the `openblack-commit-check` skill (section 2).
