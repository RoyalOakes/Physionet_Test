/**
 * This file contains all functions related to processing user input.
 */

////////////////////////////////////////////////////////////////////////////////
// INCLUDE
////////////////////////////////////////////////////////////////////////////////

#include "user_cmd.h"
#include "wfdb.h"

////////////////////////////////////////////////////////////////////////////////
// EXTERNAL VARIABLES DEFINITIONS
////////////////////////////////////////////////////////////////////////////////

// Give some prototypes for the cmd_table.
void cmd_process_dir(void);
void cmd_process_mount(void);
void cmd_process_ls(void);
void cmd_process_help(void);
void cmd_process_siginfo(void);
void cmd_process_sigdat(void);

/**
 * This table contains all of the commands that the user can utilize.
 */
cmd_entry_t cmd_table[] = {
    {"dir", cmd_process_dir},
    {"mount", cmd_process_mount},
    {"ls", cmd_process_ls},
    {"help", cmd_process_help},
    {"siginfo", cmd_process_siginfo},
    {"sigdat", cmd_process_sigdat},
    {NULL, NULL} // End of table.
};

////////////////////////////////////////////////////////////////////////////////
// GLOBAL VARIABLES
////////////////////////////////////////////////////////////////////////////////

FATFS static   fs;      // The FATFS system for accessing files on an sd card.
FRESULT static fresult; // Error code returned by ff.h for certain operations.
FIL static     fil;     // File object structure.
FILINFO static finfo;   // File info structure.
DIR static     dir;     // Directory info structure.

wfdb_err_t wfdbresult; // Most recent WFDB result.
cmd_err_t cmdresult;   // Most recent command result.

char user_path[USER_PATH_LEN] = {"0:"};  // Working path for the user.
size_t user_path_null_idx = 2; // The index of the null terminator in user_path.

rec_info_t rec_info = {0};

void term_print_werr1(char const msg[static 1], int const err_code) {
    TERM_COLOR_ERR();
    printf("\r\nWFDB ERROR: %s (%d).", msg, err_code);
    TERM_COLOR_DFT();
}

////////////////////////////////////////////////////////////////////////////////
// FUNCTION DEFINITIONS
////////////////////////////////////////////////////////////////////////////////



/**
 * @brief Prints a warning message to the terminal.
 * @param msg The message to print.
 * @warning msg must be a null terminated string and must not be NULL.
 */
void term_print_warning(char const msg[static 1]) {
    TERM_COLOR_WAR();
    printf("%s", msg);
    TERM_COLOR_DFT();
}

/**
 * @brief Prints an error message to the terminal. An error code should be 
 * provided.
 * @param msg The message to print.
 * @param err_code The error code to print.
 * @warning msg must be a null terminated string and must not be NULL. 
 */
void term_print_ferr(char const msg[static 1], int const err_code) {
    TERM_COLOR_ERR();
    printf("\r\nFATFS ERROR: %s (%d).", msg, err_code);
    TERM_COLOR_DFT();
}

/**
 * @brief Moves the user's working path back by one directory.
 */
static void user_path_move_back (void) {
    if (user_path_null_idx <= 2) { // We are at root.
        strncpy(user_path, "0:", USER_PATH_LEN); // Just to be safe.
        user_path_null_idx = 2;
        TERM_BEEP();
        return;
    }

    // Move the null terminator index back to the previous '/'.
    while (user_path[--user_path_null_idx] != '/') tight_loop_contents();
    
    user_path[user_path_null_idx] = '\0';
    printf("\r\n%s", user_path);
    return;
}

/**
 * @brief This command processes the "dir" command. The argument from the user
 * should be a string which represents a directory to open, or ".." to move
 * the directory back by one. If the directory can be opened, the user_path
 * is updated to include the argument. Any errors are printed to the terminal.
 * 
 * @warning The user_path variable should always begin with "0:" and should
 * always be null terminated.
 */
void cmd_process_dir (void) {
    char buf[USR_QUEUE_ENTRY_LEN] = {"\0"};
    size_t len = 0;

    // We can be sure that the argument in buf is fewer than USR_QUEUE_ENTRY_LEN 
    // bytes because the user input buffer can not exceed that value.
    sscanf(uib, "dir %s ", buf);
    len = strlen(buf);

    if (len == 0) { // No arguments. Print the working directory.
        printf("\r\n%s", user_path);
        return;
    }

    if (buf[0] == '.' && buf[1] == '.' && buf[2] == '\0') {
        user_path_move_back();
        return;
    }

    // Don't let the user just put a period. It messes things up.
    if (buf[0] == '.' && buf[1] == '\0') {
        printf("\r\n%s", user_path);
        return;
    }

    // Make sure there is enough space for the updated path, with extra space
    // for any additional directories.
    if (user_path_null_idx + len >= USER_PATH_LEN - (USR_QUEUE_ENTRY_LEN + 1)) { 
        fresult = FR_INVALID_NAME;
        term_print_ferr("Potential path name too long", fresult);
        return;
    }

    // Because we used sscanf, the buf must contain a null terminated string.
    user_path[user_path_null_idx] = '/';
    user_path[user_path_null_idx + 1] = '\0'; // Set null terminator.
    strncat(&user_path[user_path_null_idx+1], 
        buf, 
        USER_PATH_LEN - user_path_null_idx - 2);

    // Try opening the directory.
    fresult = f_opendir(&dir, user_path);
    if (fresult != FR_OK) { // Could not open, cleanup.
        term_print_ferr("Could not open directory", fresult);
        user_path[user_path_null_idx] = '\0';
        f_closedir(&dir);
        return;
    }
    f_closedir(&dir);

    // Advance the index to the null terminator.
    user_path_null_idx += 1 + len;
    
    // Finally, print the updated path.
    printf("\r\n%s", user_path);
}

/**
 * @brief Attempts to mount a microSD card using ff.h. Prints any errors
 * to the terminal.
 */
void cmd_process_mount(void) {
    printf("\r\n");
    if (!sd_init_driver()) {
        fresult = FR_INT_ERR;
        term_print_ferr("Could not initialize SD driver", fresult);
        return;
    }

    // Mount drive
    fresult = f_mount(&fs, "0:", 1);
    if (fresult != FR_OK) {
        term_print_ferr("Could not mount file system", fresult);
        return;
    }
}

/**
 * @brief Helper function for cmd_process_ls. Prints all of the directories 
 * within the user_path.
 * @warning Assumes dir holds a valid object and hasn't been freed.
 */
void print_directories(void) {
    size_t i;
    for (i = 0; i < MAX_FILES_NUM; i++) {
        fresult = f_readdir(&dir,&finfo);

        // If we run out of files, stop.
        if (fresult != FR_OK) break;

        // If a file does not have a name, break.
        if (finfo.fname[0] == '\0') break;

        // If a file is hidden, system, or volume ID, skip it.
        if ((finfo.fattrib & 0b00001110)) continue;

        if (finfo.fattrib & 0b00010000) { // We have found a directory.
            printf("\r\n%s", finfo.fname);
            printf("/");
        }
    }

    if (i >= MAX_FILES_NUM) {
        term_print_warning("\r\nToo many files to display");
    }
}

/**
 * @brief Prints the contents of the file currently pointed to by fil. Only
 * prints MAX_CAT_LINE lines to the terminal. Skips any lines that start with
 * '#'.
 * @warning Does not close file.
 */
void cat_record(void) {
    char tc = 0;
    UINT btr = 1, br;
    size_t cr = 0; // Counter for the number of lines in the file.
    uint8_t _is_crlf = 0; // Assume UNIX LF to start.
    uint8_t _pound = 0;   // Skip line if found at start.

    printf("\r\n");
    while (fresult == FR_OK && !f_eof(&fil) && cr < MAX_CAT_LINE) {
        fresult = f_read(&fil, &tc, btr, &br);
        if (fresult != FR_OK) {
            term_print_ferr("Could not parse RECORDS", fresult);
            break;
        }

        if (tc == '#') {
            _pound = 1;
        }

        if (tc == '\n') { // UNIX LF line ending.
            cr++;
            if (_is_crlf == 0) {
                // If we see a LF, we need to print a CR for the terminal.
                if (_pound == 0) printf("\r\n");
            } else {
                // If we have CR already, we can print the LF.
                if (_pound == 0) printf("%c", tc);
            }
            _pound = 0;
        } else if (tc == '\r') { // WINDOWS CRLF line ending.
            _is_crlf = 1;
            if (_pound == 0) printf("%c", tc);
        } else {
            if (_pound == 0) printf("%c", tc);
        }
    }

    // Check that we did not overflow.
    if (cr >= MAX_CAT_LINE) {
        term_print_warning("\r\n~~~~~ TOO MANY LINES! ~~~~~\r\n");
        return;
    }
}

/**
 * @brief This function lists all of the records that can be accessed at the 
 * current user_path. It attempts to open the RECORDS file. If it can be opened,
 * then it prints the contents of the record file to the terminal.
 * @warning Assumes dir holds a valid object and hasn't been freed.
 */
void print_records(void) {
    size_t i;
    char temp_buf[USER_PATH_LEN];
    for (i = 0; i < MAX_FILES_NUM; i++) {
        fresult = f_readdir(&dir,&finfo);

        // If we run out of files, stop.
        if (fresult != FR_OK) break;

        // If a file does not have a name, break.
        if (finfo.fname[0] == '\0') break;

        // If a file is hidden, system, or volume ID, skip it.
        if ((finfo.fattrib & 0b00001110)) continue;

        if (strcmp(finfo.fname, "RECORDS") == 0) { // Found a records file.
            snprintf(temp_buf,sizeof(temp_buf),"%s/%s", user_path, finfo.fname);
            fresult = f_open(&fil, temp_buf, FA_READ | FA_OPEN_EXISTING);

            if (fresult != FR_OK) {
                term_print_ferr("Could not open RECORDS", fresult);
            }
            
            TERM_COLOR_REC();
            cat_record();
            TERM_COLOR_DFT();

            f_close(&fil);
        }
    }

    if (i >= MAX_FILES_NUM) {
        term_print_warning("\r\nToo many files to parse");
    }
}

/**
 * @brief This command uses a multi-sweep operation to print all of the
 * directories and records available at the user_path. First, all directories
 * are printed. Then, if an RECORDS files is present, prints all of the
 * available records.
 */
void cmd_process_ls (void) { // TODO
    // Check if user_path is actually valid.
    fresult = f_opendir(&dir, user_path);
    if (fresult != FR_OK) {
        term_print_ferr("Could not open directory.", fresult);
        return;
    }

    print_directories();
    f_closedir(&dir);

    fresult = f_opendir(&dir, user_path); // Go back to beginning.
    if (fresult != FR_OK) {
        term_print_ferr("Could not open directory", fresult);
        return;
    }

    print_records();
    f_closedir(&dir);
    return;
}

void cmd_process_help (void) { // TODO
    printf("\r\nTODO: Help.");
}


/** 
 * @brief Reads a header file and displays the information to the user.
 */
void cmd_process_siginfo (void) { // TODO
    char arg_buf[USR_QUEUE_ENTRY_LEN] = {"\0"};
    char path_buf[USER_PATH_LEN];
    size_t len = 0;

    // We can be sure that the argument in buf is fewer than USR_QUEUE_ENTRY_LEN 
    // bytes because the user input buffer can not exceed that value.
    sscanf(uib, "siginfo %s ", arg_buf);
    len = strlen(arg_buf);

    snprintf(path_buf, sizeof(path_buf), "%s/%s.hea", user_path, arg_buf);
    fresult = f_open(&fil, path_buf, FA_READ | FA_OPEN_EXISTING);
    printf("\r\n%s", path_buf);

    if (fresult != FR_OK) {
        term_print_ferr("Could not open .hea file", fresult);
        return;
    }

    wfdb_err_t temp = wfdb_readheader(&fil);
    if (temp == WFDB_OK)
        wfdb_printrecord();

    f_close(&fil);
}

/** 
 * @brief Reads a data file and prints the first 10 frames.
 */
void cmd_process_sigdat (void) { // TODO
    char arg_buf[USR_QUEUE_ENTRY_LEN] = {"\0"};
    char path_buf_h[USER_PATH_LEN];
    char path_buf_d[USER_PATH_LEN];
    size_t len = 0;

    // We can be sure that the argument in buf is fewer than USR_QUEUE_ENTRY_LEN 
    // bytes because the user input buffer can not exceed that value.
    sscanf(uib, "sigdat %s ", arg_buf);
    len = strlen(arg_buf);

    snprintf(path_buf_h, sizeof(path_buf_h), "%s/%s.hea", user_path, arg_buf);
    fresult = f_open(&fil, path_buf_h, FA_READ | FA_OPEN_EXISTING);
    printf("\r\n%s", path_buf_h);

    if (fresult != FR_OK) {
        term_print_ferr("Could not open .hea file", fresult);
        return;
    }

    wfdb_err_t temp = wfdb_readheader(&fil);
    if (temp != WFDB_OK) {
        term_print_werr("Could not parse header", temp);
        return;
    }

    f_close(&fil);

    // OK, so the header is fine.
    len = strlen(path_buf_h);
    for (size_t i = len; i >= 2; i--) {
        if (path_buf_h[i] == '/') {
            path_buf_h[i] = '\0';
            break;
        }
    }

    snprintf(path_buf_d, sizeof(path_buf_d), "%s/%s", path_buf_h, rd.fname);
    fresult = f_open(&fil, path_buf_d, FA_READ | FA_OPEN_EXISTING);
    printf("\r\n%s", path_buf_d);

    if (fresult != FR_OK) {
        term_print_ferr("Could not open data file", fresult);
        return;
    }

    rd.data = 0;
    rd.datb = 0;
    rd.count = 0;

    for (size_t j = 0; j < rd.nsig; j++) {
        rd.sig[j].samp = 0;
    }

    f_lseek(&fil, rd.start);

    for (size_t i = 0; i < 10; i++) {
        wfdb_getframe(&fil);
        printf("\r\n  ");
        for (size_t j = 0; j < rd.nsig; j++) {
            printf("%16d ", rd.sig[j].samp);
        }
    }

    f_close(&fil);

    return;
}

void cmd_process(void) {
    char buf[USR_QUEUE_ENTRY_LEN] = {0};
    size_t len = 0;

    sscanf(uib, "%s ", buf);
    len = strlen(buf);

    if (len == 0) {
        term_print_warning("\r\nNo command");
        return;
    }

    size_t i = 0;
    while (cmd_table[i].cmd != NULL) {
        if (strcmp(cmd_table[i].cmd, buf) == 0) {
            cmd_table[i].handler();
            return;
        }
        i++;
        
    }

    printf("\r\nUnrecognized command: [%s]\r\n", buf);
    return;
}

