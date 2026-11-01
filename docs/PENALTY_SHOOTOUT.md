# PENALTY_SHOOTOUT.md — a friendly that is nothing but the shootout

A training mode: the match is the engine's own penalty shootout — the one a
drawn cup tie already goes to — from its first frame. Best of five, sudden
death after, then the result screen. The defending side's keeper is its
player's to throw (below), which is the point of the drill: practise guessing.

Both targets. SDL and Amiga both get the mode, the instant start and the
manual keeper — all of it is shared match code (`src/penalty_shootout.c` has
no SDL guards).

## Using it

Main menu → **PRACTICE** (the left column's second row, under EDIT) →
**PENALTY SHOOTOUT**. The PRACTICE submenu is what the SHOOTING PRACTICE
button became on 2026-10-26, when this mode joined it; both modes sit in it,
built like the EDIT submenu (docs/MENUS.md §12).

From there it is the friendly flow — the settings screen and team selection —
**with one screen swapped out: where a friendly would show the PLAY MATCH
screen, the drill shows the SHOOTING ORDER screen** (below, since 2026-10-26).
Tactics, squad swaps, VIEW OPPO and substitutes mean nothing to a match that
is only penalties, so none of that is offered; the one choice is who shoots
when. Any selection a friendly can start, a shootout can — both sides shoot,
so human v human and human v computer both make sense and nothing extra is
gated (contrast SHOOTING PRACTICE, which needs exactly one computer side).
The screens say so as you go: the headers read PENALTY SHOOTOUT, the
selection's confirm button PLAY SHOOTOUT, the start button START SHOOTOUT,
and the match is named PENALTY SHOOTOUT on the scoreboard and in a saved
highlight.

## The shooting order

The SHOOTING ORDER screen (`_shootoutOrderMenu`) lists the side's **ten
outfield players as the shooting queue**: positions 1–5, the best-of-five
takers, sit a gap above 6–10, who only shoot if it goes to sudden death (the
queue cycles past ten). **Click a player and then another and they trade
places**; clicking the same player twice puts him back down. The rows are the
squad list's own (number, fitness, name, position, skills, value — the
ViewSquad renderer), the picked-up row shown in its highlight style. START
SHOOTOUT starts the match, EXIT abandons it, and in human v human each player
orders their own queue in turn, exactly as each would have had a PLAY MATCH
screen.

The default queue is the engine's own order — `NextPenalty` walks
`spritesTable[10..1]`, the most attacking outfield slot first — so an
untouched screen changes nothing, and a CPU side (which never sees the
screen) shoots as it always has. **The goalkeeper never shoots**: the
engine's shooter pick has always excluded slot 0, and the drill keeps that
rather than teach the goalie update to take a kick.

The order is menu state (`g_shootoutOrder`), reset to the default each time
the mode is entered and **latched at kick-off beside the mode flag**
(`PenaltyShootout_BeginMatch`, validated as a permutation on the way in). In
the match, `NextPenalty` asks `PenaltyShootout_NextShooterSlot` before its
own countdown and writes the answer through the same sim variable
(`_teamXPenaltyShooterIndex`), so the sprite pick and everything downstream
are untouched; every match that is not the drill gets -1 and picks the stock
way, so cup shootouts, netplay and the rigs see nothing new.

**START SHOOTOUT goes straight to the first penalty.** No walk-on, no
kick-off: the mode's per-frame hook calls `StartPenalties` on the match's
first frame — the same jump a drawn cup tie makes at minute 90, made at
minute 0. (It first waited for the kickoff to be taken, the arcade timer's
entry point, which opened every shootout with a pointless whistle.)
`StartPenalties → NextPenalty` rebuilds the scene wholesale — `InitTeamsData`,
`StopAllPlayers`, its own game state and camera — and everything it reads
exists before the loop's first frame (`StartingMatch` has already run), so
the ceremony it replaces was never load-bearing.

## The manual keeper

OPTIONS -> GK CONTROL selects the penalty-save rule on Amiga and SDL:

- **DEFAULT** (initial setting): preserves the behaviour described below. A
  held dive direction takes control; otherwise normal matches use the AI and
  the training shootout keeper stands.
- **AI**: the computer handles penalty saves, including the training shootout.
- **MANUAL**: the human defender chooses the dive at every penalty. With no
  dive direction held, the keeper stands; there is no AI fallback.

This setting lasts for the current application session and resets to DEFAULT
on restart. CPU teams and open-play keeping are unaffected.

The decision can be checked with Cygwin GCC:
`gcc -include include/swos_platform.h -Iinclude tests/keeper_control_test.c src/penalty_shootout.c -o build/keeper_control_test && build/keeper_control_test`.

### In the drill a kick is a guess: five places (2026-11-01)

In PENALTY SHOOTOUT (the drill only, not a cup tie's shootout) physics plays
no part in a kick. Every kick is **on target**, going to one of **five places**,
and the keeper's guess decides it: **same place = save, any other = goal**.

| place | taker aims with / keeper guesses with |
|---|---|
| high left | UP + LEFT |
| left (low) | LEFT or DOWN + LEFT |
| the middle | nothing, UP or DOWN |
| right (low) | RIGHT or DOWN + RIGHT |
| high right | UP + RIGHT |

- **Takers.** A human taker aims with the stick at the moment he strikes. A
  computer taker picks one of the five at random. Both kick sites
  (`PlayerKickingBall`, `DoPass` in `src/update_players.c`) call
  `PenaltyShootout_ShotStruck`.
- **The ball** then flies a scripted straight line to the place in 16 frames
  (`PenaltyShootout_ScriptBall`, called from `UpdateBall`). On the last frame it
  goes back to physics just behind the line, so the engine's own goal check
  scores it.
- **The keeper** commits when the ball is within 20 px (`PenaltyShootout_Keeper`):
  - a human keeper takes the stick held at that moment;
  - a computer keeper picks a place at random.
  - The sides dive (high, or low with `GoalkeeperForceDiveAnim`); the middle
    stands.
  - While the ball is scripted, the goalie update's own catch, claim and
    deflect checks stand aside (`PenaltyShootout_BallScripted`).
- **The odds.** A random guess saves one kick in five, by design: the first
  version had eight places (three heights per side) and play-testing found it
  nearly unsaveable.
- **The arrow.** A human keeper facing a computer taker sees an arrow on the
  goal pointing the held place, until the ball is struck
  (`PenaltyShootout_DrawKeeperArrow`, after `DrawSprites` in `src/game_loop.c`).
  - It is red with an orange edge, built at start-up as 13x13 sprites from two
    drawings, and drawn with `DrawSpriteClipped`.
  - On the Amiga the planes and masks are `SWOS_CHIP_BSS`, and the blit queue
    undraws it.
  - Nothing is drawn for the middle.
- **Replays and rollback.** None of this is in the replay or rollback state:
  the drill is offline training and keeps no recording.

### Outside the drill: the manual keeper

At every other penalty a human defends, the keeper is yours (below). Since
2026-11-01 a held dive starts only once the ball is close
(`ManualKeeperDiveDue`, `src/goalkeeper.c`: from 20 px out to 10 px past him),
where before it started at the strike and was always too early or too late.

| held | he does |
|---|---|
| LEFT | full-stretch jump left |
| LEFT + DOWN | low dive left |
| RIGHT / RIGHT + DOWN | mirrored |
| nothing | the AI keeper (DEFAULT), or stands (MANUAL) |

The reward for guessing the side is that the dive is **at full dive speed**.
The AI keeper during penalties rolls dice on side, speed *and* timing, and
deliberately dives the wrong way about one attempt in four. The risk is that
the height is yours too: a low dive under a rising shot, or a jump over a
daisy-cutter, reaches nothing.

A CPU-run side keeps the AI keeper. In human v human the defending player gets
the keeper each round.

**Since 2026-10-26 the manual keeper is not the mode's alone: every penalty a
human DEFENDS works this way**, in any match on either target — an in-match
penalty kick (from the strike until the next touch, the engine's own
`_penalty` window) and a drawn cup tie's shootout, offline or online (the
held direction is ordinary synced input, so netplay and the replay/rollback
rigs see nothing new). The mode remains where the mechanic is practised, with
**one deliberate difference outside the drill: a neutral stick hands the
keeper back to the AI.** In the drill, nothing held means the keeper stands —
no guess, no dive is the exercise — but in a real match that would punish
every player who never learned the mechanic, so there the manual dive is
strictly opt-in: touch nothing and the keeper plays the game he always has,
hold a dive direction and it overrides the dice. (The first live build had
the drill's stand-on-neutral everywhere; changed same day, on the first
play-test.) Open play is untouched: shots in the run of play are saved by the
AI keeper as ever, since the defender's pad is busy steering an outfield
player.

## How it works

| piece | where |
|---|---|
| the mode flag, the latch, the first-frame jump, the keeper input, the shooting order + its screen's handlers | `src/penalty_shootout.c` |
| the PRACTICE submenu (data) | `bin/data_json/menus/practiceMenu.json`, `_practiceMenu` in `src/menu_data.c` |
| the SHOOTING ORDER screen (data) | `bin/data_json/menus/shootoutOrderMenu.json`, `_shootoutOrderMenu` in `src/menu_data.c` (the ViewSquad shape, docs/MENUS.md §12) |
| where it replaces the PLAY MATCH screen | `ShowPlayMatchMenu` (`src/play_match_menu.c`) |
| where the match reads the order | the shooter pick in `NextPenalty` (`src/goalkeeper.c`) |
| the submenu's opener | `PracticeMenuSelected` (`src/menus.c`) |
| the flow it borrows | `FriendlyFlow` → `Friendly` → `ShowFriendlyMenu` (`src/menus.c`) |
| the words SHOOTOUT replaces FRIENDLY with | `FriendlyMenuInit`, `ChooseTeamsMenu` (`src/menus.c`), `SetGameStartButtonText` (`src/play_match_menu.c`), `InitAndPlayGame` (`src/game_init.c`) |
| latch set / per-frame jump | `InitializeInGameTeamsAndStartGame` (`src/game_init.c`) / the tick call in `GameLoop` (`src/game_loop.c`) |
| the engine's shootout (untouched) | `StartPenalties` (`src/match_setup.c`), `NextPenalty` (`src/goalkeeper.c`) |
| manual keeper: the ask | the `shot_at_goal` gate in the goalie update (`src/update_players.c`) |
| manual keeper: the dive | `GoalkeeperJumping` + `GoalkeeperForceDiveAnim` (`src/goalkeeper.c`) |
| the drill's five places: kick, flight, keeper, arrow | `PenaltyShootout_ShotStruck` / `_ScriptBall` / `_Keeper` / `_DrawKeeperArrow` (`src/penalty_shootout.c`); hooks in `src/update_players.c`, `UpdateBall` (`src/ball_physics.c`), `GameLoop` (`src/game_loop.c`) |

**The mode reuses the friendly flow whole**, for the reasons argued in
docs/SHOOTING_PRACTICE.md (RNG seed, gameType save/restore, the pristine
team-cache reload) — but through **`FriendlyFlow`, not `FriendlyMenu`**.
`FriendlyMenu` ends in `CommonMenuExit`, the *main menu's* fixup, and both
mode handlers now run under the 4-entry PRACTICE submenu, where that call
writes a competition name through a stale pointer (the EDIT submenu's
documented trap, MENUS.md §12). `FriendlyFlow` is the same flow without the
fixup; `PracticeMenuSelected` makes the call once, after the submenu closes.

**The mode is latched at kick-off** (`PenaltyShootout_BeginMatch`), like
shooting practice and for the same reason: what a match plays by is decided
once, never read live from a menu global.

**The manual keeper is asked for before any AI path.** The goalie update's
save gate (`shot_at_goal`, `src/update_players.c`) asks
`ManualKeeper_DiveInput` above both the finisher-skill auto-react
roll and `ShouldGoalkeeperDive`, so in manual mode no dice ever move him. The
input is the engine's own 0..7 compass read from the defending side's
`TeamGeneralInfo.direction` — W/SW/E/SE are literally the pad's left,
left+down, right, right+down (`src/input.c`). The shooting side's keeper runs
the same goalie update, idle at the far end, and is explicitly excluded
(`_penaltyShooterSprite->teamNumber` during a shootout; at an in-match kick
the kicker's side is `_lastTeamPlayed`, set beside `_penalty` at the strike),
as is any CPU-run side (`playerNumber == 0`). The gate is the engine's own
penalty-save window, `_playingPenalties || _penalty` — the same pair
`ShouldGoalkeeperDive`'s penalty branch tests — which is what makes the
mechanic hold in normal matches, not just this mode.

## The bug log

**The REPLAY after a shootout played as a plain friendly** (found in testing,
fixed same day). The post-friendly REPLAY button restarts the match through
`InitGameRestoreTeams` + `GameLoop` directly (`game_loop.c`
`end_of_match_flow`), never re-entering `InitializeInGameTeamsAndStartGame` —
so the latch survived but the first-frame jump's one-shot stayed spent, and
the replayed match never went to penalties. That path now calls
`PenaltyShootout_MatchRestarted`, which re-arms the one-shot and leaves the
latch alone, so a replayed friendly stays a friendly. The general lesson:
**the REPLAY path skips the match-begin hooks** — anything latched in
`InitializeInGameTeamsAndStartGame` that must also reset per match needs its
own re-arm there. (Shooting practice never noticed, because its strip runs on
every walk-on rather than once.)

## Planned

- Shooter-side control refinements (aiming help, power), if the drill wants
  them — the shooter is the stock penalty taker today.
