/**
 * This file contains all functions related to recieveing user input from a
 * the terminal. The two main functions are blocking_get_user_input and 
 * timeout_get_user_input. There are helper functions for handling ANSI 
 * escape sequences, backspaces, and the user input queue.
 */

////////////////////////////////////////////////////////////////////////////////
// INCLUDE
////////////////////////////////////////////////////////////////////////////////

#include "user_input.h"

////////////////////////////////////////////////////////////////////////////////
// GLOBAL VARIABLES
////////////////////////////////////////////////////////////////////////////////

/* Index of oldest element in queue. */
size_t static oidx = 0;
/* Index of newest element in queue. */
size_t static nidx = 0;
/* Index of the location where the next element will be inserted. */
size_t static cidx = 0;
/* User offset. The value accessed by the user. 0 is when no element is 
   accessed. 1 is when first element is accessed. 2 is second, etc. */
size_t static uo = 0; 

/* Current position of the user input buffer. */
size_t static sidx = 0; 

////////////////////////////////////////////////////////////////////////////////
// DEFINES
////////////////////////////////////////////////////////////////////////////////

#define ESC_SEQ_TIMEOUT_US 10

////////////////////////////////////////////////////////////////////////////////
// FUNCTION DEFINITIONS
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Puts a copy of the user input buffer into the user input queue.
 * Adjusts indices as necessary. When the queue fills, the oldest element will
 * be silently overwritten.
 */
static void enqueue_user_input(void) {
    // if oidx == nidx == cidx, then queue is empty.
    if (cidx == nidx && cidx == oidx) {
        memccpy(&uiq[cidx][0], uib, '\0', USR_QUEUE_ENTRY_LEN); 
        uiq[cidx][USR_QUEUE_ENTRY_LEN-1] = '\0'; // Ensure null terminator.

        nidx = cidx; // Make sure nidx is at the newest element.
        cidx = (cidx + 1) % USR_QUEUE_ENTRY_DEPTH;

        return;
    }

    // if cidx == oidx, then we must advance both cidx and oidx. This will be
    // the most common case when the user has entered many commands.
    if (cidx == oidx) {
        memccpy(&uiq[cidx][0], uib, '\0', USR_QUEUE_ENTRY_LEN); 
        uiq[cidx][USR_QUEUE_ENTRY_LEN-1] = '\0'; // Ensure null terminator.

        oidx = (oidx + 1) % USR_QUEUE_ENTRY_DEPTH;
        nidx = cidx; // Make sure nidx is at the newest element.
        cidx = (cidx + 1) % USR_QUEUE_ENTRY_DEPTH;
        
        return;
    }

    // We are not overwriting any old elements.
    memccpy(&uiq[cidx][0], uib, '\0', USR_QUEUE_ENTRY_LEN); 
    uiq[cidx][USR_QUEUE_ENTRY_LEN-1] = '\0'; // Ensure null terminator.

    nidx = cidx; // Make sure nidx is at the newest element.
    cidx = (cidx + 1) % USR_QUEUE_ENTRY_DEPTH;

    return;
}

/**
 * @brief When a backspace is pressed in the terminal, this function will
 * decrement sidx and clear the character preceding the cursor position.
 */
static void handle_user_backspace(void) {
    if (sidx > 0) { // There is something to backspace over.
        sidx--;
        uib[sidx] = '\0';
        TERM_BACKSPACE();
    } else { // We are at the beginning of the line. Can't backspace anymore.
        uib[sidx] = '\0'; // Just to be safe.
        TERM_BEEP();
    }
}

/**
 * @brief When an up arrow is pressed, this function will put a copy of the
 * next oldest entry of the user input queue into the user input buffer. 
 * Adjusts the value of sidx to the end of the entry.
 */
static void handle_user_up_arrow(void) {
    if (nidx == cidx && oidx == cidx) { // Queue is empty.
        TERM_BEEP();
        return;
    }

    // Calculate how many valid entries exist in the queue
    size_t num_entries;
    if (nidx >= oidx) {
        num_entries = nidx - oidx + 1;
    } else {
        // Queue has wrapped around
        num_entries = USR_QUEUE_ENTRY_DEPTH - oidx + nidx + 1;
    }

    // Check if we can go further back in history
    if (uo >= num_entries) { // We are at the oldest entry.
        TERM_BEEP();
        return;
    }

    // Queue is not empty and we are not at the end of the queue.
    uo++;

    // Everything seems OK. Copy the entry from the queue in to the buffer.
    size_t temp = (nidx + USR_QUEUE_ENTRY_DEPTH - uo + 1)%USR_QUEUE_ENTRY_DEPTH;
    memccpy(uib, &uiq[temp][0], '\0', USR_QUEUE_ENTRY_LEN);
    uiq[temp][USR_QUEUE_ENTRY_LEN-1] = '\0'; // Ensure null terminator.

    // Advance sidx to the end of the entry.
    sidx = 0; // Reset sidx first
    while (uib[sidx] != '\0' && sidx < USR_QUEUE_ENTRY_LEN - 1) {
        sidx++;
    }

    return;
}

/**
 * @brief When a down arrow is pressed, this function will put a copy of the
 * next newest entry of the user input queue into the user input buffer. 
 * Adjusts the value of sidx to the end of the entry.
 */
static void handle_user_down_arrow(void) {
    if (uo == 0) { // We are at the beginning of the queue already.
        TERM_BEEP();
        return;
    }

    if (uo == 1) { // We are at the newest entry already.
        // Clear buffer and return to empty input.
        uo = 0;
        sidx = 0;
        uib[sidx] = '\0';
        return;
    }

    // We are not at the beginning and not at the newest entry.
    uo--;

    // Copy the entry from the queue in to the buffer.
    size_t temp = (nidx + USR_QUEUE_ENTRY_DEPTH - uo + 1)%USR_QUEUE_ENTRY_DEPTH;
    memccpy(uib, &uiq[temp][0], '\0', USR_QUEUE_ENTRY_LEN);
    uiq[temp][USR_QUEUE_ENTRY_LEN-1] = '\0'; // Ensure null terminator.

    // Advance sidx to the end of the entry.
    sidx = 0; // Reset sidx first
    while (uib[sidx] != '\0' && sidx < USR_QUEUE_ENTRY_LEN - 1) {
        sidx++;
    }

    return;
}

/**
 * @brief Processes an escape sequence. Assumes that an ESC has already been
 * read by getchar and that the next character is the beginning of the escape
 * sequence.
 * @warning Only supports ANSI escape sequences for up and down arrows.
 * @todo Implment something for when the user presses ESC alone.
 */
static void handle_user_escape_sequence(void) {
    int temp = getchar_timeout_us(ESC_SEQ_TIMEOUT_US);

    // If there are no characters immediately available, then ESC was pressed by
    // the user, and there is no escape squence.
    if (temp == PICO_ERROR_TIMEOUT) {
        return; // Do nothing (for now).
    }

    // If there are chars immediately available, then an escape sequence has 
    // been entered. Look for '[' to indicate an ANSI escape sequence.
    if (temp == '[') {
        temp = getchar_timeout_us(ESC_SEQ_TIMEOUT_US);

        switch (temp) {
            case 0x41: // Up arrow pressed.
                handle_user_up_arrow();
                break;
            case 0x42: // Down arrow pressed.
                handle_user_down_arrow();
                break;
            default: // Not supported. Burn characters.
                while (PICO_ERROR_TIMEOUT!=getchar_timeout_us(ESC_SEQ_TIMEOUT_US)) 
                    tight_loop_contents();
                break;
        }
        return;
    }

    // Unknown escape sequence. Burn through any remaining characters.
    while (PICO_ERROR_TIMEOUT != getchar_timeout_us(ESC_SEQ_TIMEOUT_US)) 
        tight_loop_contents();

    return;
}

/**
 * @brief Requests a string from the user. Stores the ongoing value in the user
 * input buffer and enqueues the result when enter/return is pressed. This
 * blocks until the user presses enter/return.
 * @param prm A string that will prompt the user.
 * @return The number of characters in the user input terminal (excluding null
 * terminator.)
 * @warning Silently ignores non-printable chars, except ESC, CR, LF, and DEL.
 */
size_t blocking_get_user_input(char const* prm) {
    char temp;
    uib[sidx] = '\0'; // Set null terminator.

    printf("\r\n%s", prm); // Echo prompt to terminal.

    while (1) { // Keep getting chars until user presses enter/return.
        temp = getchar(); // Blocking getchar().
        switch (temp) {
            case 0x00: // Nothing is pressed.
                break;
            case 0x0d: case 0x0a: // Enter or return is pressed.
                uib[sidx] = '\0'; // Set null terminator.
                sidx = 0; // Reset index for next user input.
                enqueue_user_input();
                uo = 0;
                TERM_CLEAR_LINE(); // Clear line.
                printf("%s", prm);
                TERM_COLOR_HIS();
                printf("%s", uib);
                TERM_COLOR_DFT();
                return sidx;
            case 0x7f: case 0x08: // Backspace or delete is pressed.
                handle_user_backspace();
                break;
            case 0x1b: // An escape sequence. 
                handle_user_escape_sequence();
                TERM_CLEAR_LINE();
                printf("%s%s", prm, uib);
                break;
            default:
                if (temp >= 32 && temp <= 126) {
                    uib[sidx++] = temp;
                    printf("%c", temp);
                }
                temp = 0;
                break;
        }

        if (sidx >= USR_QUEUE_ENTRY_LEN - 1) { // Must leave speace for '\0'.
            TERM_CLEAR_LINE();
            TERM_COLOR_WAR();
            printf("Overflow: limit of (%d) chars.", USR_QUEUE_ENTRY_LEN - 1);
            TERM_COLOR_DFT();
            TERM_BEEP(); 
            printf("\r\n%s", prm); // Reprint prompt after overflow.
            sidx = 0;
            uib[sidx] = '\0';
        }
    }
}

/**
 * @brief Requests a string from the user. Stores the ongoing value in the user
 * input buffer and enqueues the result when enter/return is pressed. This
 * blocks until the user presses enter/return.
 * @param prm A string that will prompt the user.
 * @return The character that was entered by the user. Returns 
 * PICO_ERROR_TIMEOUT if no input was entered within the timeout period 
 * specified by ESC_SEQ_TIMEOUT_US.
 * @warning Silently ignores non-printable chars, except ESC, CR, LF, and DEL.
 */
int timeout_get_user_input(char const* prm) {
    int temp;
    uib[sidx] = '\0'; // Set null terminator.

    printf("\r\n%s", prm); // Echo prompt to terminal.

    temp = getchar_timeout_us(ESC_SEQ_TIMEOUT_US); // Blocking getchar().
    switch (temp) {
        case PICO_ERROR_TIMEOUT: // Nothing is pressed.
            return PICO_ERROR_TIMEOUT;
        case 0x0d: case 0x0a: // Enter or return is pressed.
            uib[sidx] = '\0'; // Set null terminator.
            sidx = 0; // Reset index for next user input.
            enqueue_user_input();
            uo = 0;
            TERM_CLEAR_LINE(); // Clear line.
            printf("%s", prm);
            TERM_COLOR_HIS();
            printf("%s", uib);
            TERM_COLOR_DFT();
            return temp;
        case 0x7f: case 0x08: // Backspace or delete is pressed.
            handle_user_backspace();
            break;
        case 0x1b: // An escape sequence. 
            handle_user_escape_sequence();
            TERM_CLEAR_LINE();
            printf("%s%s", prm, uib);
            break;
        default:
            if (temp >= 32 && temp <= 126) {
                uib[sidx++] = temp;
                printf("%c", temp);
            }
            return temp;
    }

    if (sidx >= USR_QUEUE_ENTRY_LEN - 1) { // Must leave speace for '\0'.
        TERM_CLEAR_LINE();
        TERM_COLOR_WAR();
        printf("Overflow: limit of (%d) chars.", USR_QUEUE_ENTRY_LEN - 1);
        TERM_COLOR_DFT();
        TERM_BEEP(); 
        printf("\r\n%s", prm); // Reprint prompt after overflow.
        sidx = 0;
        uib[sidx] = '\0';
    }
}