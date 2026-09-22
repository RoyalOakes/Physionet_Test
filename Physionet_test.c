#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/rand.h"

#include "sd_card.h"
#include "ff.h"

#include "term.h"
#include "user_input.h"
#include "user_cmd.h"
#include "wfdb.h"

char uib[USR_QUEUE_ENTRY_LEN] = {0}; // User input buffer.
char uiq[USR_QUEUE_ENTRY_DEPTH][USR_QUEUE_ENTRY_LEN] = {0}; // User input queue.           

int main() {
    stdio_init_all();

    // Do nothing until a USB connection is made thru a terminal.
    while(!stdio_usb_connected()){ 
        tight_loop_contents();
    }

    // White text on a black background
    TERM_COLOR_DFT();

    while (true) {
        blocking_get_user_input("> ");
        cmd_process();
    }
}
