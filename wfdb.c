/**
 * This file contains the implementation on various WFDB related functions.
 */

#include "wfdb.h"

////////////////////////////////////////////////////////////////////////////////
// GLOBAL VARIABLES
////////////////////////////////////////////////////////////////////////////////

wfdb_rec_t rd = {0};

////////////////////////////////////////////////////////////////////////////////
// FUNCTION DEFINITIONS
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Reads a byte from a data file.
 * @param fp The file to read from.
 * @return The value of the byte read as an int32_t.
 */
static int32_t r8(FIL* fp) {
    char tc = 0;
    UINT btr = 1, br;
    f_read(fp, &tc, btr, &br);
    return 0x000000FF & tc;
}

/**
 * @brief Reads a 16 formatted sample from a file.
 * @param fp The file to read from.
 * @return The value of the 16-bit two's complement number read as an int32_t.
 */
static int32_t r16(FIL* fp) {
    int32_t l, h;

    l = r8(fp);
    h = r8(fp);
    return (((h<<8) | (l & 0xFF))<<16)>>16; 
}

/**
 * @brief Reads a 61 formatted sample from a file.
 * @param fp The file to read from.
 * @return The value of the reversed 16-bit two's complement number read as an 
 * int32_t.
 */
static int32_t r61(FIL* fp) {
    int32_t l, h;

    h = r8(fp);
    l = r8(fp);
    return (((h<<8) | (l & 0xFF))<<16)>>16; 
}

/**
 * @brief Reads a 24 formatted sample from a file.
 * @param fp The file to read from.
 * @return The value of the 24-bit two's complement sample.
 */
static int32_t r24(FIL* fp) {
    int32_t l, h;

    l = r16(fp);
    h = r8(fp);
    return (((h<<16) | (l & 0xFFFF))<<8)>>8; 
}

/**
 * @brief Reads a 32 formatted sample from a file.
 * @param fp The file to read from.
 * @return The value of the 32-bit two's complement sample.
 */
static int32_t r32(FIL* fp) {
    int32_t l, h;

    l = r16(fp);
    h = r16(fp);
    return ((h<<16) | (l & 0xFFFF)); 
}

/**
 * @brief Reads a 80 formatted sample from a file.
 * @param fp The file to read from.
 * @return The value of the 8-bit value in offset binary form.
 */
static int32_t r80(FIL* fp) {
    return r8(fp) - (1 << 7); 
}

/**
 * @brief Reads a 160 formatted sample from a file.
 * @param fp The file to read from.
 * @return The value of the 16-bit value in offset binary form.
 */
static int32_t r160(FIL* fp) {
    return r16(fp) - (1 << 15); 
}

/**
 * @brief Reads a 212 formatted sample from a file. Three 12-bit samples packed
 * tightly.
 * @param fp The file to read from.
 * @param nrst Active low reset. Send zero to reset the internal counter. Send
 * one otherwise.
 * @return The value of the 12-bit two's complement sample.
 */
static int32_t r212(FIL* fp) {
    int32_t v;

    switch (rd.count++) {
        case 0: 
            v = rd.data = r16(fp); 
            break;
        case 1:
        default:
            rd.count = 0;
            v = ((rd.data >> 4) & 0xF00) | (r8(fp) & 0xFF);
            break;
    }

    // Sign extiension from the 12th bit.
    if (v & 0x800) v |= ~(0xFFF);
    else v &= 0xFFF;
    return v;
}

/**
 * @brief Reads a 310 formatted sample from a file. Three 10-bit samples packed
 * into 4 bytes.
 * @param fp The file to read from.
 * @param nrst Active low reset. Send zero to reset the internal counter. Send
 * one otherwise.
 * @return The value of the 10-bit two's complement sample.
 */
static int32_t r310(FIL* fp) {
    int32_t v;

    switch (rd.count++) {
        case 0: v = (rd.data = r16(fp)) >> 1; break;
        case 1: v = (rd.datb = r16(fp)) >> 1; break;
        case 2:
        default:
            rd.count = 0;
            v = ((rd.data & 0xf800) >> 11) | ((rd.datb & 0xf800) >> 6);
            break;
    }

    // Sign extiension from the 10th bit.
    if (v & 0x200) v |= ~(0x3ff);
    else v &= 0x3ff;
    return (v);
}

/**
 * @brief Reads a 311 formatted sample from a file. Three 10-bit samples packed
 * into 4 bytes; yet somehow different from r310.
 * @param fp The file to read from.
 * @param nrst Active low reset. Send zero to reset the internal counter. Send
 * one otherwise.
 * @return The value of the 10-bit two's complement sample.
 */
static int32_t r311(FIL* fp) {
    int32_t v;

    switch (rd.count++) {
        case 0: v = (rd.data = r16(fp)); break;
        case 1: 
            rd.datb = (r8(fp) & 0xff);
            v = ((rd.data & 0xfc00) >> 10) | ((rd.datb & 0xf) << 6);
            break;
        case 2:
        default:
            rd.count = 0;
            rd.datb |= r8(fp) << 8;
            v = rd.datb >> 4;
            break;
    }

    // Sign extiension from the 10th bit.
    if (v & 0x200) v |= ~(0x3ff);
    else v &= 0x3ff;
    return (v);
}

/**
 * @brief Prints an error message to the terminal.
 */
void term_print_werr(char const msg[static 1], int const err_code) {
    TERM_COLOR_ERR();
    printf("\r\nWFDB ERROR: %s (%d).", msg, err_code);
    TERM_COLOR_DFT();
}

/**
 * @brief Checks if an integer is a valid format type for WFDB.
 * @param val The int to check.
 * @return 0 if not valid, 1 if valid.
 */
int wfdb_is_fmt(int const val) {
    switch (val) {
        case 8: case 16: case 24: case 32: case 61: case 80: case 160: case 212:
        case 310: case 311:
            return 1;
        default:
            return 0;
    }
    return 0;
}

/**
 * @brief Reads a line from a file and stores it in buf.
 * @param buf The buffer to write.
 * @param fp The pointer to the file
 * @return The number of chars in the line (excluding null terminator).
 */
size_t wfdb_getline (char buf[static WFDB_LINEL], FIL* fp) {
    char tc = 0;
    UINT btr = 1, br;
    size_t i = 0;

    FRESULT fr = f_read(fp, &tc, btr, &br);
    if (fr != FR_OK) {
        //term_print_ferr("Could not parse header", fresult);
        return 0;
    }

    // Keep getting chars until we get a LF. This works with LF or CRLF.
    do {
        buf[i++] = tc;
        (void)f_read(fp, &tc, btr, &br);
    } while (i < WFDB_LINEL - 2 && !f_eof(fp) && tc != '\n'); 

    // Set LF and terminator.
    buf[i] = '\n';
    buf[i+1] = '\0';

    return i;
}

/**
 * @brief Prints all of the information for rd.
 */
void wfdb_printrecord(void) {
    TERM_COLOR_DFT();
    printf("\r\n%s(%s) %d %1.1f %d", rd.rname, rd.fname, rd.nsig, rd.fs,
        rd.fmt);
    if (rd.start != 0) {
        printf("+%d", rd.start);
    }

    for (int i = 0; i < rd.nsig; i++) {
        printf("\r\n%4d ", i);
        TERM_COLOR_DES();
        printf("%s ", rd.sig[i].desc);
        TERM_COLOR_DFT();
    }
}

/**
 * @brief Reads a header file and stores the result in rd. Prints an error and
 * returns if anything goes wrong.
 * @param fp File pointer to the header file.
 * @warning The file pointer must be valid.
 */
wfdb_err_t wfdb_readheader(FIL* fp) {
    char *p, *q;
    rd.fs = 250.0f; // Default frequency.
    size_t nsig = 0; // Number of signals in the header.
    float fs = 0.0f;
    char static sep[] = " \t\n\r";

    char line[WFDB_LINEL];
    line[WFDB_LINEL-1] = '\0'; // Just to be safe.
    line[0] = '\0';

    size_t len = wfdb_getline(line, fp);
    
    if (len == 0) {
        term_print_werr("No data in hea", WFDB_ERR_HEA_EOF);
        return WFDB_ERR_HEA_EOF;
    }

    // Get the record name from the first non-empty, non-comment line.
    while ((p = strtok(line, sep)) == NULL || *p == '#') {
        if (wfdb_getline(line, fp) == 0) {
            term_print_werr("No name in hea", WFDB_ERR_HEA_NO_NAME);
            return WFDB_ERR_HEA_NO_NAME;
        }
    }

    if (strlen(p) >= WFDB_RECL) {
        term_print_werr("Record name too long", 
            WFDB_ERR_HEA_BAD_NAME);
        return WFDB_ERR_HEA_BAD_NAME;
    }

    strncpy(rd.rname, p, sizeof(rd.rname));
    rd.rname[sizeof(rd.rname)-1] = '\0'; // Just to be safe.

    for (q = p+1; *q && *q != '/'; q++); // IDK. Check for multi-seg?

    if (*q == '/') {
        term_print_werr("Multi-segment not supported", 
            WFDB_ERR_HEA_OBSOLETE);
        return WFDB_ERR_HEA_OBSOLETE;
    }

    strncpy(rd.fname, p, sizeof(rd.fname));
    rd.fname[sizeof(rd.fname) - 1] = '\0'; // Just to be safe.

    // Make sure we are using a new-style header with more than one token.
    if ((p = strtok((char *)NULL, sep)) == NULL) {
        term_print_werr("Obsolete header format", 
            WFDB_ERR_HEA_OBSOLETE);
        return WFDB_ERR_HEA_OBSOLETE;
    }

    // Number of signals is the second argument.
    sscanf(p, "%ld", &nsig);

    // Sampling frequency.
    if (p = strtok((char *)NULL, sep)) {
        sscanf(p, "%f", &fs);
        if (fs <= 0 || fs > 10000) {
            term_print_werr("Sampling frequency is incorrect", 
                WFDB_ERR_HEA_BAD_FREQ);
            return WFDB_ERR_HEA_BAD_FREQ;
        } else {
            rd.fs = fs;
        }
    }

    // Skip over counter frequency and base counter value.
    if (p) {
        for ( ; *p && *p != '/'; p++);
        if (*p == '/') {
            for ( ; *p && *p != '('; p++);
        }
    }

    // Skip over samples per signal.
    p = strtok((char *)NULL, sep);

    // Skip base time and date.
    p = strtok((char *)NULL,"\n\r");

    // Too many signals in hea. Give up.
    if (nsig > WFDB_NSIG) {
        term_print_werr("Too many signals in hea", 
            WFDB_ERR_HEA_BIG_NSIG);
        return WFDB_ERR_HEA_BIG_NSIG;
    }

    rd.nsig = nsig;

    // Get info for each signal.
    for (int s = 0; s < nsig; s++) {
        do {
            if (wfdb_getline(line,fp) == 0) {
                term_print_werr("Unexpected EOF in hea", 
                    WFDB_ERR_HEA_EOF);
                return WFDB_ERR_HEA_EOF;
            }
        } while ((p = strtok(line, sep)) == NULL || *p == '#');

        // We have found a non-empty, non-comment line. Get file name.
        if (s != 0) {
            if (strcmp(p, rd.fname) != 0) {
                term_print_werr("All signals must have the same file name",
                    WFDB_ERR_HEA_CORRUPT);
                return WFDB_ERR_HEA_CORRUPT;
            }
        } else {
            strncpy(rd.fname, p, sizeof(rd.fname));
            rd.fname[sizeof(rd.fname) - 1] = '\0'; // Just to be safe.
        }
        
        // Get format, spf, skew, and offset.
        if ((p = strtok((char *)NULL, sep)) == NULL) {
	        term_print_werr("No format for signal", 
                WFDB_ERR_HEA_NO_FMT);
            return WFDB_ERR_HEA_NO_FMT;
	    }

        int spf = 1;
        int skew = 0;
        int start = 0;
        int fmt = 0;

        sscanf(p, "%d", &fmt);

        while (*(++p)) {
            if (*p == 'x' && *(++p)) {
                sscanf(p, "%d", &spf);
            }
            if (*p == ':' && *(++p)) {
                sscanf(p, "%d", &skew);
            }
            if (*p == '+' && *(++p)) {
                sscanf(p, "%d", &start);
            }
        }

        if (s != 0) {
            if (spf != rd.spf) {
                term_print_werr("All signals must have the same spf",
                    WFDB_ERR_HEA_CORRUPT);
                return WFDB_ERR_HEA_CORRUPT;
            }
            if (skew != rd.skew) {
                term_print_werr("All signals must have the same skew",
                    WFDB_ERR_HEA_CORRUPT);
                return WFDB_ERR_HEA_CORRUPT;
            }
            if (start != rd.start) {
                term_print_werr("All signals must have the same start",
                    WFDB_ERR_HEA_CORRUPT);
                return WFDB_ERR_HEA_CORRUPT;
            }
            if (fmt != rd.fmt) {
                term_print_werr("All signals must have the same format",
                    WFDB_ERR_HEA_CORRUPT);
                return WFDB_ERR_HEA_CORRUPT;
            }
        } else {
            if (spf != 1) {
                term_print_werr("Unsupported spf.",
                    WFDB_ERR_HEA_CORRUPT);
                return WFDB_ERR_HEA_CORRUPT;
            }
            if (skew != 0) {
                term_print_werr("Unsupported skew.",
                    WFDB_ERR_HEA_CORRUPT);
                return WFDB_ERR_HEA_CORRUPT;
            }
            rd.spf =   (spf < 1) ? 1 : spf;
            rd.skew =  (skew < 0) ? 0 : skew;
            rd.start = (start < 0) ? 0 : start;
            rd.fmt = fmt;
        }

        if (!wfdb_is_fmt(rd.fmt)) {
            term_print_werr("Invaid signal format", 
                WFDB_ERR_HEA_BAD_FMT);
            return WFDB_ERR_HEA_BAD_FMT;
        }

        // Skip gain in ADC units.
        p = strtok((char *)NULL, sep);
        
        // Skip adc resolution.
        p = strtok((char *)NULL, sep);

        // Skip ADC zero.
        p = strtok((char *)NULL, sep);

        // Skip Init val.
        p = strtok((char *)NULL, sep);

        // Skip checksum
        p = strtok((char *)NULL, sep);

        // Skip blocksize
        p = strtok((char *)NULL, sep);

        // Get the description.
        if (p = strtok((char *)NULL, "\n\r")) {
            strncpy(rd.sig[s].desc, p, sizeof(rd.sig[s].desc));
            rd.sig[s].desc[sizeof(rd.sig[s].desc) - 1] = '\0';
        } else {
            snprintf(rd.sig[s].desc, 
                sizeof(rd.sig[s].desc),
                "sig_%d",
                s);
            rd.sig[s].desc[sizeof(rd.sig[s].desc) - 1] = '\0';
        }
    }

    return WFDB_OK;
}

/**
 * @brief Reads a single frame from a data file. Stores the frame in rd.
 * @param fp The file to read from.
 * @param nrst Active low reset. Set to 0 to reset internal counters. Otherwise
 * set to 1. Only matters for 212, 310, and 311.
 * @warning File pointer must be vaild.
 */
void wfdb_getframe(FIL* fp) {
    for (int c = 0; c < rd.nsig; c++) {
        switch (rd.fmt) {
            case 8: 
                rd.sig[c].samp += r8(fp);
                break;
            case 16: 
                rd.sig[c].samp = r16(fp);
                break;
            case 24: 
                rd.sig[c].samp = r24(fp);
                break;
            case 32: 
                rd.sig[c].samp = r32(fp);
                break;
            case 61: 
                rd.sig[c].samp = r61(fp);
                break;
            case 80: 
                rd.sig[c].samp = r80(fp);
                break;
            case 160: 
                rd.sig[c].samp = r160(fp);
                break;
            case 212:
                rd.sig[c].samp = r212(fp);
                break;
            case 310: 
                rd.sig[c].samp = r310(fp);
                break;
            case 311:
                rd.sig[c].samp = r311(fp);
                break;
            default:
                term_print_werr("Could not get frame", WFDB_ERR_DAT_CORRUPT);
                return;
        }
    }
}