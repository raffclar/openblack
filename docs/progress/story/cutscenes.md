# Cut scenes and films

The films (Bink videos) and the scripted cut scenes of the story: the camera moving through shots under cinema bars,
fades, and the advisors and characters acting them out.

**Progress: 4/20 done, 2 partial — 25%**

## Films

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The start-up logos, the pre-intro, the intro film, the loading screen's tip pictures and the creature's fall | todo | see ../video/bink_videos.md and ../video/bink_playback.md |

## Scripted cut scenes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A cut scene takes the camera and brings the cinema bars in | partial | bars and fades work (`CinematicDirectorSystem`); taking the camera for a script is a stub |
| The camera is set to a place and focus at once | done | set camera position and focus (`src/CHLApi.cpp`) |
| The camera glides to a place and focus over a time | todo | move-camera commands are stubs; see ../camera/script_camera.md |
| The camera follows or faces an object | todo | see ../camera/follow_cameras.md |
| The camera runs a recorded camera path | todo | paths load and play in the debug window (`CameraPathSystem`), the script command is a stub; see ../camera/camera_paths.md |
| The picture fades to a colour and back between shots | done | see ../camera/cinematics.md |
| Characters walk to marks, turn and play animations for the scene | todo | the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| Special effects and sounds are started for the scene | todo | the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| Game time can be set, stopped and moved on for a scene | done | game time commands (`src/CHLApi.cpp`) |
| Music for the scene | partial | start and stop music work; see ../audio/music.md |
| The game's speed can be slowed for a scene | todo | the script commands it needs (dialogue, scrolls, advisors, rewards) are still stubs in `src/CHLApi.cpp` |
| Close camera clipping for close shots | done | see ../camera/cinematics.md |

## The story's set pieces

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The family and the drowning boy at the very start | todo | see land_1.md |
| The temple's completion | todo |  |
| The creatures in the glade | todo |  |
| Khazar's arrival and death | todo | see land_2.md |
| Lethys stealing the creature | todo |  |
| The vortex opening between lands | todo |  |
| Nemesis's curse, the big fight and the ending | todo | see ending.md |
