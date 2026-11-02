/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
/* shooting_practice.h — SHOOTING PRACTICE, a mode of its own.
 *
 * A friendly, human vs computer, where the computer side is stripped down to
 * its goalkeeper (TeamGeneralInfo.spritesTable[0]) so the human's 11 can
 * practise finishing into an open defence.
 *
 * It is the main menu's SHOOTING PRACTICE button (entry 2, under EDIT) and
 * nothing else: press it and you are in the friendly's own flow -- its settings
 * screen, its team selection, its PLAY MATCH screen, with EDIT TACTICS, VIEW
 * OPPO, squad swaps and substitutes all working as they always do. The ONE
 * differences are the untimed match, chosen attacking direction and computer
 * side fielding only its keeper (plus any enabled lines).
 *
 * Until 2026-10-22 it was instead a PRACTICE toggle on the PLAY MATCH screen
 * (entry 69). That button is gone: it had to clear itself whenever it was off
 * screen, or a stale ON leaked into an unrelated match, and that same clearing
 * is what made a standalone mode impossible to build on top of it. The mode now
 * owns g_shootingPracticeMode below and nothing else writes it.
 *
 * BOTH targets. Nothing here depends on SDL: the strip and the kickoff hook are
 * shared match code, and this object links into the m68k binary too, so the
 * Amiga gets the same drill. (Contrast UNIFORM, which is SDL-only because it
 * opens a SWOS 2020 JSON menu the Amiga does not run.)
 */
#pragma once
#include "game_types.h"

/* Non-zero while ShootingPracticeMenu runs the practice flow. Owned by it
 * alone -- no menu redraw clears it. While it is set ShowFriendlyMenu shows the
 * mode's start screen instead of the friendly's, and the screens after it say
 * PRACTICE instead of FRIENDLY and refuse to start until exactly one side is
 * the computer. */
extern int32_t g_shootingPracticeMode;

/* The start screen's DEFENSE / MIDFIELD / ATTACK switches, one column per
 * side (src/shooting_practice_menu.c): 1 = the side keeps its players of that
 * line (RB/LB/D, M/LW/RW, A), 0 = they are taken off. uint16_t because each
 * button draws through a MENU_CMD_STRING_TABLE whose index read is 2 bytes.
 * Defaults: the player all ON, the computer all OFF. Menu state: the match
 * latches them at kick-off (ShootingPractice_BeginMatch). */
extern uint16_t g_practicePlayerDefense, g_practicePlayerMidfield, g_practicePlayerAttack;
extern uint16_t g_practiceCpuDefense, g_practiceCpuMidfield, g_practiceCpuAttack;
/* ATTACK GOAL: the player's side attacks 0 = UP, 1 = DOWN. Latched at kick-off. */
extern uint16_t g_shootingPracticeAttackDown;
/* CPU SKILL: 0 RANDOM (default, any strength), 1 BEGINNER .. 4 WORLD CLASS -- how strong a team RANDOM TEAM
 * draws for the computer (shooting_practice_menu.c). CHOOSE TEAM ignores it. */
extern uint16_t g_shootingPracticeCpuSkill;

/* The PRACTICE submenu's SHOOTING PRACTICE button: set the mode, run the
 * friendly flow (whose first screen is the start screen), clear it again. */
void ShootingPracticeMenu(void);

/* 1 when the teams picked so far could start a drill -- exactly one computer
 * side and exactly one human one. The three screens that show the PLAY button
 * ask this before showing it (CheckNumTeamsAllowed, CheckNumberOfTeams,
 * SetChooseChangeTeamsEntries), so a selection the drill cannot apply to can
 * never be started rather than silently playing as an ordinary friendly. */
int16_t ShootingPractice_SelectionOk(void);

/* Latch whether THIS match is a shooting-practice one, and which side is the
 * computer, for the match's lifetime. Called once the in-game teams know who is
 * who (InitializeInGameTeamsAndStartGame). Everything below reads that latch
 * rather than the mode flag, so that a match already under way cannot be
 * changed by anything the menus do afterwards. */
void ShootingPractice_BeginMatch(void);

/* Latched mode: no match clock or timed half/full-time in shooting practice. */
int16_t ShootingPractice_IsActive(void);

/* 0 = ordinary match; otherwise the team occupying the top half (attacking
 * down). Apply before InitTeamsData binds teams to pitch ends. */
int16_t ShootingPractice_ForcedTopTeamNum(void);

/* 0 = no change; else the team number (1 or 2) that must kick off instead of
 * whatever the coin toss picked: the player's side, unless the player took all
 * their lines off -- then the computer's, because a lone goalkeeper cannot take a
 * kickoff. Applies to EVERY kickoff: match start,
 * half time, extra time, and the restart after a goal. */
int16_t ShootingPractice_ForcedKickoffTeamNum(void);

/* Takes off the lines each side does not keep: marks those outfield players
 * sentAway (the same flag a red card sets — src/tackle_foul.c SendPlayerAway)
 * and parks them off-pitch, without the disciplinary side effects (no card, no
 * booking stat, no SENT OFF banner) since nothing happened in the match. The
 * keepers always stay. A no-op unless the mode is on and exactly one side is the
 * computer. Called every time both teams walk out (match start, second half) so
 * the strip survives half-time. */
void ShootingPractice_StripOutfieldPlayers(void);
