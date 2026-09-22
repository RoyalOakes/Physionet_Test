/**
 * This header is for functions and such for dealing with WFDB headers and data
 * files.
 * 
 * This is a simplified version of the WFDB library. It assumes that there is
 * only one record open at a time. Signals within a record must have the same
 * file name, format, spf, skew, offset, and fs. There is a limit for how many
 * signals a record may have. Bsize must always be 1.
 * 
 * Multi-segment rectords are not supported.
 */

// Avoid double inclusion.
#ifndef BME463_WFDB_H /* BME463_WFDB_H */
#define BME463_WFDB_H

////////////////////////////////////////////////////////////////////////////////
// INCLUDE
////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"

#include "ff.h"
#include "term.h"

////////////////////////////////////////////////////////////////////////////////
// DEFINES
////////////////////////////////////////////////////////////////////////////////

#define WFDB_LINEL (256) // Max number of chars in a line of a WFDB file.
#define WFDB_NSIG  (16)  // Max number of signals in a record.
#define WFDB_FILEL (32)  // Max number of chars in a file name.
#define WFDB_RECL  (32)  // Max number of chars in a record name.
#define WFDB_DESCL (32)  // Max number of chars in a signal description.

////////////////////////////////////////////////////////////////////////////////
// ERROR HANDLING
////////////////////////////////////////////////////////////////////////////////

/** 
 * Contains error codes associated with WFDB files. Right now, it supports
 * error codes assocated with the readheader function. Errors associated with
 * opening files are handled by the FRESULT type defined in ff.h, so they are 
 * not included here.
 * 
 * HEA errors are errors assocated with parsing the header file.
 * DAT errors are errors associated with parsing the data file.
 */
typedef enum { 
    WFDB_OK = 0,            // No error.
    WFDB_ERR_HEA_CORRUPT,   // The header file is incorrectly formatted.
    WFDB_ERR_HEA_NO_NAME,   // No record name in the header file.
    WFDB_ERR_HEA_NO_NSIG,   // The number of signals is not present.
    WFDB_ERR_HEA_NO_FNAME,  // A signal is missing a file name.
    WFDB_ERR_HEA_NO_FMT,    // A signal is missing a format code.
    WFDB_ERR_HEA_MSR,       // The record is nested in another multi-seg record.
    WFDB_ERR_HEA_BAD_NAME,  // The record name is unsupported or incorrect.
    WFDB_ERR_HEA_BAD_FREQ,  // The sampling frequency is not supported.
    WFDB_ERR_HEA_BAD_NSIG,  // The number of signals is not supported.
    WFDB_ERR_HEA_BAD_FMT,   // The format is not supported.
    WFDB_ERR_HEA_BIG_NSIG,  // The number of signals is too large.
    WFDB_ERR_HEA_OBSOLETE,  // The record format is obsolete.
    WFDB_ERR_HEA_EOF,       // Unexpected EOF in the header file.
    WFDB_ERR_HEA_LONGDESC,  // A signal description is too long.
    WFDB_ERR_HEA_LONGNAME,  // A signal name is too long.
    WFDB_ERR_DAT_EOF,       // Unexpected EOF in the data file.
    WFDB_ERR_DAT_CORRUPT,   // The data file is incorrectly formatted.
} wfdb_err_t;

////////////////////////////////////////////////////////////////////////////////
// DATA TYPES
////////////////////////////////////////////////////////////////////////////////

/**
 * Contains all of the information necessary for a WFDB signal.
 */
typedef struct {
    char desc[WFDB_DESCL]; // Description of signal.
    int32_t samp;          // Most recent sample from record.
} wfdb_sig_t;

/**
 * Contains all of the information necessary for a WFDB record.
 */
typedef struct {
    char fname[WFDB_FILEL]; // Name of file that holds the data.
    char rname[WFDB_RECL];  // Name of record (no ext type).
    size_t fmt;   // Data format. No FLAC.
    size_t spf;   // Samples per frame (always 1).
    size_t skew;  // Should always be 0.
    size_t start; // Number of bytes before data begins.
    float fs;     // Sampling frequency.
    size_t nsig;  // Number of signals.
    size_t count;          // Count for packed data.
    int32_t data;          // Data for packed data.
    int32_t datb;          // Data for packed data.
    wfdb_sig_t sig[WFDB_NSIG]; // Array of signals.
} wfdb_rec_t;

/**
 * Contains information necessary for reading annotation files.
 */
typedef struct {
    int32_t type;    // Annotation type.
    size_t interval; // Number of samples before the next annotation.
    uint8_t ateof;   // 1 = At end of file, 0 = not at end.
} wfdb_ann_t;

////////////////////////////////////////////////////////////////////////////////
// EXTERN VARIABLES
////////////////////////////////////////////////////////////////////////////////

extern wfdb_rec_t rd;

////////////////////////////////////////////////////////////////////////////////
// PROTOTYPES
////////////////////////////////////////////////////////////////////////////////

void term_print_werr(char const msg[static 1], int const err_code);
wfdb_err_t wfdb_readheader(FIL* fp);
void wfdb_printrecord(void);
void wfdb_getframe(FIL* fp);

#endif /* BME463_WFDB_H */