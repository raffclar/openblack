# Challenge script language and functions

The story is written in the game's challenge script language and compiled into one file the game runs. Every script
command and native function, one row each, is in ../scripts/ (challenge_natives_*.md); the machine that runs the
scripts is in ../engine/script_vm.md. This file only says how far the story's needs are met.

**Progress: 2/12 done, 7 partial — 46%**

## What the story needs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game's own compiled challenge file is loaded and the story's top script started | done | `Game.cpp`; see ../engine/script_vm.md |
| The language compiles and decompiles exactly as the game's | done | `components/lhvmcompiler`, `components/lhvmdecompiler`; see ../debug/script_debugger.md |
| Script kinds: story scripts, help scripts, challenge help, temple help and specials, multiplayer help | partial | see ../engine/script_vm.md |
| Camera commands for cut scenes | partial | setting position and focus, widescreen and close clipping work; moving, following and paths are stubs (../scripts/) |
| Dialogue, text and the advisors | todo | all stubs (../scripts/) |
| Scrolls, challenge records and rewards | todo | all stubs (../scripts/) |
| Music and sound | partial | start and stop music and the alignment music work; sounds are stubs (../scripts/) |
| Towns, belief, influence, players and computer gods | todo | all stubs (../scripts/) |
| The creature and the leash | partial | the leash commands and some learning work (../scripts/) |
| Miracles, fire and weather | partial | casting, fire and the climate pause work (../scripts/) |
| Time and timers | partial | game time works; timers and countdowns are stubs (../scripts/) |
| Overall: about 62 of the 464 native functions do something, so no land's story runs past its first steps | partial | `src/CHLApi.cpp` |
