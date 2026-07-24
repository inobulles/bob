// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Aymeric Wibo

/*
 * Artifacts are build outputs that aren't meant to be installed anywhere on the system.
 *
 *      _.-""}
 *     / "" ;
 * .-"` __] ',               ___
 * I_ ""__.`-,;             |   |
 *   I_.,-"ii"{             !___!
 *   | ||  ||  |        ,    | |
 *   | ||  ||  |       .;    | |
 *   | ||  ||  |       | \   | |
 *   | ||  ||  |       |  |  | |
 *   | ||  ||  |       |  |  | |   __
 *   | ||  ||  |       |  |  | |  |  |
 *   | ||  ||  |   ;|  |  |  | |  |  |
 *   | ||  ||  |"\_/`,_|  |  | |  |  |  ___.--""`\
 *   | ||  ||  |       |  |\.| |=,|  |""          `,
 *   | ||  ||  |       |  |  | |  |  |____________.-+.__
 *  _:_!|_,'!__!       |  |  | |_,!  !         __,I   `"|
 * :     |      `-""`,.!__!-,!_!_ '--'`,_,--"""         |
 * |     ;___          `"-.-'    `,_.-'"            _..-'
 *  `-._ |   """--,,_     |`""-.--'|         __.--""
 *      `"--..__     ""--.|    |   |_,_  _.-'
 *              ""--.._   `-,__!_.-' _,""      fsc
 *                     ""--,____.--'"
 */

#pragma once

#include <flamingo/flamingo.h>

int setup_artifact_map(flamingo_t* flamingo);
