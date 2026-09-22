/**
 * This file contains functions for processing user commands. All commands 
 * supported for user inputs should be implemented as a cmd_entry_t and stored 
 * in the cmd_table. This is depended on the user input functions in 
 * user_input.c, espcecially the user input buf.
 */

// Avoid double inclusion.
#ifndef BME463_USER_CMD_H /* BME463_USER_CMD_H */
#define BME463_USER_CMD_H

////////////////////////////////////////////////////////////////////////////////
// INCLUDE
////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"

#include "term.h"
#include "user_input.h"
#include "wfdb.h"

#include "sd_card.h"
#include "ff.h"

////////////////////////////////////////////////////////////////////////////////
// DEFINES
////////////////////////////////////////////////////////////////////////////////

#define WFDB_LINE_LEN (256) // Max number of chars in a line from a WFDB header.
#define WFDB_MAX_NSIG (16)  // Max number of signals in a WFDB record.
#define WFDB_MAX_DESC_LEN (32) // Max number of chars in a signal description.
#define WFDB_MAX_NAME_LEN (32) // Max number of chars in a record name.

#define USER_PATH_LEN (256)  // Max length of the user path.

#define MAX_FILES_NUM (500) // Maximum number of files to print in a directory.
#define MAX_CAT_LINE (500) // Maximum number of lines to print with cat.
#define MAX_FILE_NAME_LEN (32) // Max number of chars in a file name.
#define MAX_SIG_NUM (16) // Max number of signals in a record.

////////////////////////////////////////////////////////////////////////////////
// ERROR CODE TYPES
////////////////////////////////////////////////////////////////////////////////

/**
 * Custom made error codes for this project. I don't expect to use these,
 * but they might be useful.
 */
typedef enum {
    CMD_OK = 0, // No error.
    CMD_ERR,    // Generic error code.
} cmd_err_t;

////////////////////////////////////////////////////////////////////////////////
// ADDITIONAL TYPES
////////////////////////////////////////////////////////////////////////////////

/* This structure defines an entry in the command table. */
typedef struct {
    char const* cmd;         // Name of the command.
    void (*handler) (void);  // Function pointer to the cmd handler.
} cmd_entry_t;

/* Signal information type */
typedef struct {
    char desc[WFDB_MAX_DESC_LEN]; // Signal description.
    int fmt;                      // Data format.
    size_t spf;                   // Samples per frame.
    size_t offset;                // Number of bytes before data.
    size_t skew;                  // Number of samples before sample 0.
    int data;                     // Most recent sample read for this signal.
} sig_info_t;

/* Record information type */
typedef struct {
    char name[WFDB_MAX_NAME_LEN]; // Record name.
    float fs;                     // Sample rate.
    size_t nsig;                  // Number of signals.
    sig_info_t sigs[MAX_SIG_NUM]; // Array of signals.
} rec_info_t;

////////////////////////////////////////////////////////////////////////////////
// EXTERNAL VARIABLES
////////////////////////////////////////////////////////////////////////////////

// All commands will be stored here. Defined in user_cmd.c.
extern cmd_entry_t cmd_table[];

////////////////////////////////////////////////////////////////////////////////
// FUNCTION PROTOTYPES  
////////////////////////////////////////////////////////////////////////////////

void cmd_process(void);

#endif /* BME463_USER_CMD_H */