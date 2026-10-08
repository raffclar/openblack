# Sound banks and formats

How the game stores its sounds: banks of samples with headers saying how each plays, music banks of compressed chunks
streamed back to back, and loose wave files.

**Progress: 7/9 done, 1 partial — 83%**

## Banks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Sound banks are read with every sample's header | done | `components/` pack reader; test `test_pack_bank_info` |
| Samples are decoded from the banks' formats | done | `src/Audio/SoundDecoder.cpp`; test `test_sound_decoding` |
| Loose wave files | done | `src/Audio/WavAudioDecoder.cpp` |
| Music banks are compressed chunks, streamed and played back to back | done | `src/Audio/MusicPlayer.cpp`, `MpegAudioDecoder.cpp`; test `test_music_decoding` |
| A music bank's group and length | done | `AudioManager::GetMusicBankInfo` |
| Every bank in the audio folder is loaded at start | partial | `Game.cpp` loads them all at once; the game loads banks as lands and situations need them |
| Animation effect tables in the banks | done | `AnimEffectTable` |
| Atmosphere banks load and release with each land | done | `AtmosAudio` |
| Banks are released when no longer needed | todo | `ReadAudioHeaders` streaming todo (port notes) |
