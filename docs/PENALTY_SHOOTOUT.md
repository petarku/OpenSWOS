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

From there it is the friendly flow: the start screen (below), team selection,
then the ordinary PLAY MATCH screen. Any selection a friendly can start, a
shootout can: both sides shoot, so human v human and human v computer both make
sense and nothing extra is gated (contrast SHOOTING PRACTICE, which needs exactly
one computer side). The screens say so as you go: the headers read PENALTY
SHOOTOUT, the selection's confirm button PLAY SHOOTOUT, the start button START
SHOOTOUT, and the match is named PENALTY SHOOTOUT on the scoreboard and in a
saved highlight.

## The shooting order (2026-11-01)

The PLAY MATCH screen numbers the ten outfield rows of the eleven with their
place in the shooting order, left of each row, as SWOS 2020 does
(`DrawPlayerEntries`, `src/play_match_menu.c`):

- The last row is 1 and the row under the keeper is 10, because the engine
  takes the kicks from the bottom of the eleven up (`NextPenalty` walks
  `spritesTable[10..1]`). Past ten the queue cycles.
- The number belongs to the ROW. Swapping players is how the order changes,
  and a substitute swapped into the eleven becomes a taker.
- **The goalkeeper never shoots**: the engine's shooter pick has always left
  out slot 0, and his row has no number.

Until 2026-11-01 the mode replaced the PLAY MATCH screen with a SHOOTING ORDER
screen of its own (`_shootoutOrderMenu`: the ten takers as a queue, reordered
by clicking two of them). SWOS 2020's way replaced it, and that screen and its
queue were deleted.

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

### The start screen (2026-11-01)

PENALTY SHOOTOUT opens SWOS 2020's start screen, `_shootoutStartMenu`
(`src/penalty_shootout_menu.c`), where the friendly's own screen would be. The
`ShowFriendlyMenu` swap keeps all the friendly flow's bookkeeping around it.

- **CHOOSE TEAMS** (FREE SELECTION) is the friendly's own team selection.
- **RANDOM SELECTION** draws two teams of a group (`ShootoutRandomTeams`):
  - the first is the player's side (PLAYER-COACH), the second the computer's;
  - both are then played as CHOOSE TEAMS plays a pair, so the PLAY MATCH
    screen still comes first.
  - A club group draws a random country of the continent, then a random club of
    it. The continents are the team-selection screen's own country lists
    (`_europeTable` ... `_oceaniaTable`).
  - NATIONAL is the national files 80-85, and ALL RANDOM is every club and
    national file.
  - ORIGINAL is SWOS's ORIGINAL CUSTOM TEAMS (`TEAM.072`) and CUSTOM its EDITED
    CUSTOM TEAMS (`CUSTOMS.EDT`), the CHOOSE TEAMS screen's split. Both exist on
    both targets.
  - The draw has its own generator, seeded from the clock at each press, as the
    challenge cups' does.
- **GOALKEEPER CONTROL** sets `g_shootoutKeeperComputer`, latched at kick-off:
  - PLAYER: a human side's keeper guesses with the stick.
  - COMPUTER: the computer guesses for that keeper too, and no arrow is drawn.
  - It applies to this mode only; OPTIONS → GK CONTROL (below) is for every
    other penalty.

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

### The shootout screen (2026-11-01)

While the kicks are taken the drill shows SWOS 2020's screen
(`src/penalty_shootout_hud.c`) and none of the match's own HUD.

**What it shows:**
- **Scoreboard** along the foot: both teams, the shootout score, and the last
  five kicks each (✓ scored, ✗ missed, `*` being taken, `-` not yet; past five
  the columns scroll).
- **Takers list** on the left (SDL only): the shooting side's ten takers in
  order, the one at the spot highlighted.
- **Controller labels** (SDL only): CPU or PLAYER n beside the goal and the spot.
- **The taker's name** in the corner (Amiga only), through the game's own
  name display (`PlaceCurrentPlayerName`, `src/game_hud.c`), unless OPTIONS
  has that display off.

**What it hides** (only while `PenaltyShootout_HudActive`):
- the clock (`UpdateTime`) and the result board (`ShowResult` sends every
  request to its hide step), both in `src/game_state.c`;
- the name display, the numbers over the controlled players and the marker
  (`PenaltyShootout_HideMatchHud`, before `DrawSprites`).

The result after the match shows as ever.

**How it is drawn.** Each piece is a canvas: an 8-bit picture the file paints
itself, reading the font and face sprites pixel by pixel. It is repainted only
when what it shows changes, converted to sprite planes in chip RAM on the
Amiga, and drawn as one sprite a frame with `DrawSpriteClipped`, which queues
its undraw. Each canvas is sized to its content.

**The Amiga budget** (five FS-UAE rounds with the user, 2026-11-01). On the
8-plane AGA pitch every pixel of a bob's rectangle, see-through ones included,
is blitted plane by plane twice a frame: drawn, then undrawn.
- The first version drew grid tiles, bars and every glyph straight onto the
  pitch each frame: about a thousand blits a frame, far too slow.
- The canvases were better, but the takers list alone was more than half the
  area. The Amiga drops it, and the labels too, by the user's call.
- The Amiga shows the screen only while a kick is set up (`ST_PENALTIES`),
  and repaints only then. A repaint is CPU work that costs an A1200 several
  frames, and a result arrives mid-dive (the save) or in the celebration,
  which showed as a slow keeper.
- The shootout's first frame runs `MoveCamera`'s easing to the end
  (`PenaltyShootout_MatchTick`), so the camera starts at the penalty goal
  instead of scrolling there from the kick-off over all 22 players.

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
| the mode flag, the latch, the first-frame jump, the keeper input, the five places, the arrow | `src/penalty_shootout.c` |
| the PRACTICE submenu (data) | `bin/data_json/menus/practiceMenu.json`, `_practiceMenu` in `src/menu_data.c` |
| the start screen (data) | `bin/data_json/menus/shootoutStartMenu.json`, `_shootoutStartMenu` in `src/menu_data.c`; handlers in `src/penalty_shootout_menu.c` |
| the shooting-order numbers | `DrawPlayerEntries` (`src/play_match_menu.c`) |
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
