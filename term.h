/**
 * This file contains macros for controlling terminal colors and other useful 
 * commands. It helps other files look cleaner. It can also allow users to 
 * easily change the macros in case we need to move from ANSI to something else.
 */

// Avoid double inclusion.
#ifndef BME463_TERM_H /* BME463_TERM_H */
#define BME463_TERM_H

////////////////////////////////////////////////////////////////////////////////
// INCLUDE
////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>

////////////////////////////////////////////////////////////////////////////////
// MACROS
////////////////////////////////////////////////////////////////////////////////

// Macros for setting terminal colors.

#define TERM_COLOR_DFT() printf("\e[38;5;255m\e[48;5;0m") // Default
#define TERM_COLOR_HIS() printf("\e[38;5;4m\e[48;5;255m") // History
#define TERM_COLOR_REC() printf("\e[38;5;10m\e[48;5;0m")  // Records
#define TERM_COLOR_ANN() printf("\e[38;5;13m\e[48;5;0m")  // Annotations
#define TERM_COLOR_WAR() printf("\e[38;5;11m\e[48;5;0m")  // Warnings
#define TERM_COLOR_ERR() printf("\e[38;5;9m\e[48;5;0m")   // Errors
#define TERM_COLOR_DES() printf("\e[38;5;214m\e[48;5;0m") // Descriptions

// Macros for controlling the terminal.
#define TERM_CLEAR_LINE() printf("\33[2K\r") 
#define TERM_BEEP() printf("\a")
#define TERM_BACKSPACE() printf("\b \b")

#endif /* BME463_TERM_H */