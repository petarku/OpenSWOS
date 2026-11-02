# Challenges

Implemented for Amiga and SDL. A challenge is a match position against the
computer, bundled with the game: load it to play on with the same teams, score,
clock, players, ball, bookings and substitutions remaining. This build
(offline-modes) has only the predefined collection below -- no saving a position
mid-match, no SAVED POSITIONS list, no generator.

## Challenge collection and progress

For the full feature guide, save format and maintenance notes, see
[CHALLENGE_COLLECTION.md](CHALLENGE_COLLECTION.md).

CHALLENGES opens the **CHALLENGE COLLECTION** list.
The collection is available on SDL and Amiga, each challenge rated 1-5 stars
(a tick on the list marks a completed one):

| Stars | Challenge | Human team first | Start |
| --- | --- | --- | --- |
| 1 | Finish the Job | AC Milan vs Juventus | 2-0 at 75, Juventus kickoff |
| 2 | Hold Your Nerve | Italy vs Malta | 1-0 at 80, Malta corner |
| 3 | Find the Winner | Liverpool vs Everton | 0-0 at 75, Liverpool corner |
| 3 | Turn It Around | Barcelona vs Real Madrid | 0-1 at 65, Barcelona kickoff |
| 4 | Against the Clock | Juventus vs AC Milan | 0-2 at 80, Juventus kickoff |
| 5 | Derby Miracle | Partizan vs Red Star | 0-3 at 67, Partizan corner |

All six are available immediately. Select a row for its details, then PLAY.
**Win the match** to complete it: a level score at 90 goes to extra time, then
penalties, and a shootout win counts; abandoning the match fails. The match goes
straight to the challenge result screen: RETRY (reloads the original position,
minute, score and players for a fresh attempt), NEXT CHALLENGE (after success),
CHALLENGE LIST or EXIT. A failed retry never removes an existing completion.
Finishing all six shows ALL CHALLENGES COMPLETED. Ordinary friendly replays
still start a new match.

Progress saves automatically at the final whistle and loads when entering the
collection after a restart. `CHPROG1.PRG` and `CHPROG2.PRG` live in the normal
save folder. These are alternating, versioned, checksummed records of stable
challenge IDs; the previous valid write survives an interrupted save. The UI
reports a failed save and offers RETRY SAVE. On FS-UAE, use the same persistent
mounted game drawer (`bin/` here); emulator snapshots are unnecessary. Keep the
save folder writable and preserve both progress files when moving installations.

The starting positions remain bundled assets, separate from progress and personal
saves: `data/CCS01.DAT` through `CCS06.DAT` for SDL, `data/CCA01.DAT` through
`CCA06.DAT` for Amiga. They are native `.CHL` payloads with an asset extension,
loaded directly without copying over user files. Collection play uses standard
Amiga gameplay/view and cards rules, restoring SDL preferences afterwards.

The specs the six positions were generated from are kept in
`docs/challenge_specs/collection/`; the generator itself is not in this build.

`python3 scripts/verify_challenge_collection.py` uses `chl_dump.py` to check both
collections' teams, scores, minutes, human sides, rules and setup sizes against
the specs. This is file verification, not play testing. The portable progress
regression test is `python3 scripts/test_challenge_progress.py` (hosted GCC).

Verification on 2026-10-28: both targets build, all twelve bundled files pass the
spec checks, and the SDL menu JSON matches all 52 compiled menus. The user played
and completed the first SDL challenge. In native FS-UAE testing, the first Amiga
challenge reached a winning final whistle, wrote `save/CHPROG1.PRG`, and still
showed DONE / 1 OF 6 COMPLETED after closing and cold-starting the emulator.
The remaining scenarios' difficulty has not been playtested.

## Implementation

A `.CHL` (`src/challenge.c` reads it) is an uncompressed 96-byte header, match setup and
`SimState_SavePortable(SIM_TIER_SIM)`. Current files total 18,752 bytes: 3,662
setup bytes and 14,994 simulation bytes, plus the header. Setup includes both
teams, six user tactics, controls, match length and rules. The simulation encoding
uses typed big-endian fields and pointer identities rather than process addresses.
Framebuffer pixels and circular scroll offsets are deliberately excluded.

| Code | Responsibility |
| --- | --- |
| `Challenge_ArmBundled` / `Challenge_LoadState` | Validate and load after match initialization |
| `src/challenge_collection.c` / `src/challenge_catalog.c` | Shared collection UI and six stable definitions |
| `src/challenge_progress.c` / `EndOfGame` | Final-whistle completion, portable progress and save recovery |
| `src/game_loop.c` | One-shot load and repaint |
| `src/sim_state.c` | Shared registry and target-aware pointer/tile encoding |

The Amiga load fix preserves the pre-load render camera, then uses the existing
replay repaint to advance both buffers to the restored camera. Previously, the
first buffer retained kick-off scroll offsets while the second received the
camera delta. Loaded camera fractions are preserved. On exit, both menu scroll
origins are reset before the normal menu redraw. SDL's top-level Esc shortcut now
also checks that no game, highlight playback or challenge is active.

## Verification and known limitations

Both targets compile. SDL `--challenge-selftest` passes setup and simulation
round trips. The SDL Esc fix was confirmed by the user.

An **SDL score-panel graphics issue on challenge startup** remains reported:
the team-name/results area can show incorrect graphics resembling red cards.
Its cause has not been established or fixed.

The header checksum is calculated from live state after serialization, with
separate calls for its two halves. Interrupt-driven timers can change between
operations, so a checksum warning alone does not establish a corrupt capture.
Diagnostic instrumentation also remains enabled; its output files are not tracked.

Read [AMIGA_DEBUGGING.md](AMIGA_DEBUGGING.md) for the fixes, debugger setup and
buffer measurements. [CHALLENGE_WIP.md](CHALLENGE_WIP.md) retains the original
handover and experiments; this page is the feature guide.
