/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
/* penalty_shootout.h — PENALTY SHOOTOUT, a mode of its own.
 *
 * A friendly whose match is nothing but the engine's own penalty shootout —
 * the one a drawn cup tie already goes to (StartPenalties/NextPenalty). The
 * teams walk out, the kickoff is taken, and the first live frame hands the
 * match to the shootout, which runs and ends itself exactly as it does after
 * a drawn cup final (EndOfGame once a side cannot be caught).
 *
 * It is the PRACTICE submenu's PENALTY SHOOTOUT button (the main menu's
 * PRACTICE row, beside SHOOTING PRACTICE) and nothing else: press it and you
 * are in the friendly's own flow -- its own start screen (below), team
 * selection, the PLAY MATCH screen with the shooting order on it -- with the
 * match itself being the shootout.
 * Any selection a friendly can start, a shootout can: both sides shoot, so
 * human v human and human v computer both make sense and nothing extra is
 * gated (contrast SHOOTING PRACTICE, which needs exactly one computer side).
 *
 * BOTH targets, like shooting practice: the latch and the tick are shared
 * match code and this object links into the m68k binary too.
 */
#pragma once
#include "game_types.h"

/* Non-zero from the moment PENALTY SHOOTOUT is pressed on the PRACTICE submenu
 * until its flow returns there. Owned by PenaltyShootoutMenu alone. While it is
 * set the friendly's screens say PENALTY SHOOTOUT instead of FRIENDLY. */
extern int32_t g_penaltyShootoutMode;

/* The PRACTICE submenu's PENALTY SHOOTOUT button: set the mode, run the
 * friendly flow, clear it again. Runs FriendlyFlow, not FriendlyMenu: a
 * submenu handler must not reach CommonMenuExit (docs/MENUS.md §12). */
void PenaltyShootoutMenu(void);

/* Latch whether THIS match is a shootout, once, as the in-game teams are set
 * up (InitializeInGameTeamsAndStartGame) — the same shape as
 * ShootingPractice_BeginMatch and for the same reason: what a match plays by
 * is decided at kick-off, never read live from a menu global. */
void PenaltyShootout_BeginMatch(void);

/* Per-frame: on the first frame of the match, hand it to the shootout — the
 * same StartPenalties jump a drawn cup tie makes at minute 90, made at minute
 * 0: no walk-on, no kickoff. A no-op unless the latch is set, and fires once
 * per match. */
void PenaltyShootout_MatchTick(void);

/* The friendly REPLAY path (game_loop.c end_of_match_flow) restarts the match
 * without re-entering InitializeInGameTeamsAndStartGame: call this there so
 * the one-shot is re-armed and a replayed shootout is a shootout again. */
void PenaltyShootout_MatchRestarted(void);

/* OPTIONS -> GK CONTROL: DEFAULT preserves the contextual behaviour below;
 * AI always delegates to the computer; MANUAL stands on neutral at all penalties.
 * Session setting, shared by the menu's string table and the match. */
enum { KEEPER_CONTROL_DEFAULT, KEEPER_CONTROL_AI, KEEPER_CONTROL_MANUAL };
extern uint16_t g_keeperControl;

/* MANUAL KEEPER. At ANY penalty — the training shootout, a drawn cup tie's
 * shootout, an in-match penalty kick — the DEFENDING side's keeper belongs to
 * its player: hold LEFT to jump left, LEFT+DOWN to dive left low (RIGHT
 * mirrored). The reward for guessing is that the dive is immediate and at
 * full dive speed, where the AI keeper rolls dice on side, speed and timing;
 * the risk is the height is yours too. A NEUTRAL stick differs by context:
 * in the training drill the keeper stands (no guess, no dive — the drill's
 * point; he still catches a shot hit straight at him), while in a real match
 * it hands him back to the AI, so holding a direction is strictly opt-in.
 * (Born as this mode's drill, opened to normal matches 2026-10-26 — hence
 * the home here.)
 *
 * Asked by the goalie update at its save-attempt gate (update_players.c
 * shot_at_goal). Returns -1 when the AI should decide as ever (no penalty in
 * play, a CPU side, this keeper's own side shooting, or a neutral stick in a
 * real match); 0 for "stand" (the drill's neutral); else the held dive
 * direction (2=E, 3=SE, 5=SW, 6=W). */
int16_t ManualKeeper_DiveInput(TeamGeneralInfo *team);

/* FIVE PLACES — how a kick is decided IN THE DRILL
 * (penalty_shootout.c, docs/PENALTY_SHOOTOUT.md). Every kick goes, always on
 * target, to one of five places: high left, left, the middle, right, high
 * right. A computer taker picks one; a human taker picks the side with the
 * stick (anything with LEFT / RIGHT in it, else the middle) and the height
 * with the press, as for a pass and a shot: held high, a tap low. The keeper
 * commits with the stick alone when the ball reaches him (UP+LEFT, LEFT or
 * DOWN+LEFT, nothing / UP / DOWN, RIGHT or DOWN+RIGHT, UP+RIGHT); the same
 * place is a save, any other a goal. Physics plays no part.
 *
 *   ShotStruck   — the kick sites (update_players.c), after the engine's own
 *                  kick: picks the place and takes the ball over.
 *   ScriptBall   — UpdateBall: flies the ball there; 1 = it moved the ball.
 *   Keeper       — the goalie update: the defending keeper while it flies;
 *                  1 = handled this frame.
 *   BallScripted — non-zero while it flies: the keeper's own catch, claim
 *                  and deflect checks stand aside (the guess decides). */
void    PenaltyShootout_ShotStruck(Sprite *shooter, TeamGeneralInfo *team,
                                  int16_t strong);   /* 1 held fire (shot), 0 a tap (pass) */
int16_t PenaltyShootout_ScriptBall(Sprite *ball);
int16_t PenaltyShootout_Keeper(Sprite *keeper, Sprite *ball, TeamGeneralInfo *team);
int16_t PenaltyShootout_BallScripted(void);
void    PenaltyShootout_ShotReset(void);   /* BeginMatch / MatchRestarted */

/* THE START SCREEN (_shootoutStartMenu, src/penalty_shootout_menu.c), shown
 * by ShowFriendlyMenu in place of the friendly's own while the mode is on:
 * CHOOSE TEAMS, or a RANDOM SELECTION of two teams of a group (the player's
 * side first, the computer's second), and GOALKEEPER CONTROL. That last is
 * g_shootoutKeeperComputer: 0 = PLAYER (a human side's keeper guesses with the
 * stick), 1 = COMPUTER (the computer guesses for him; no arrow). Latched at
 * kick-off by PenaltyShootout_BeginMatch; the index of
 * _shootoutKeeperStrTable (swos_amiga_ptr_imports.c). */
extern uint16_t g_shootoutKeeperComputer;
struct MenuEntry;
void ShootoutStartMenuInit(void);
void ShootoutRandomTeams(struct MenuEntry *entry);
void ShootoutKeeperControl(struct MenuEntry *entry);

/* The arrow on the goal showing a human keeper's held place while he waits
 * for a computer taker (gone once the ball is struck). Called after
 * DrawSprites each frame (game_loop.c); a no-op outside the drill. */
void    PenaltyShootout_DrawKeeperArrow(void);

/* THE SHOOTOUT SCREEN (src/penalty_shootout_hud.c), SWOS 2020's: the shooting
 * side's ten takers on the left (current one highlighted), the kick-by-kick
 * board along the bottom (the last five kicks; a tick scored, a cross
 * missed, * to come, - not yet), and who controls each side. Drawn after the
 * sprites each frame (game_loop.c) on darkened grid tiles, whose own undraw
 * clears it on the Amiga; a no-op outside the drill.
 * TakeKickSaved: 1 once after the drill's keeper saved (shotSaved). */
void    PenaltyShootout_DrawHud(void);
int16_t PenaltyShootout_TakeKickSaved(void);
int16_t PenaltyShootout_HudActive(void);

/* THE SHOOTING ORDER is the engine's own: NextPenalty walks spritesTable
 * 10..1, so the eleven's last row shoots first and the keeper (index 0) never
 * does. The PLAY MATCH screen numbers the rows that way in this mode
 * (play_match_menu.c DrawPlayerEntries); swapping players changes it. */
