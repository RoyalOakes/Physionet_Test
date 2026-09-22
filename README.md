# PhysioNet Test

This repo contains the files for reading header and data files from PhysioNet databases. It is designed to work on a **Raspberry Pi Pico 2** with the **BME463 development board**. The code can parse `.dat` / `.hea` files from PhysioNet databases.

> **Note:** At the moment, the code is designed to only work on the sender Pico.

Use **PuTTY** or equivalent to communicate with the sender Pico at a baud rate of up to **921600**.

## Build

Use **VS Code** with the official Raspberry Pi Pico extension to build the project. You may need to edit the cmake files to make it specific to your machine.

## Files

| File | Description |
|---|---|
| `hw_config.c` | Hardware description for FATFS. Describes the GPIO and SPI pins for interfacing with the SD card. |
| `Physionet_test.c` | Main file. Contains the infinite loop. |
| `signal.c` | Taken from the WFDB repo. For reference only. |
| `user_cmd.c` | Code for executing commands received from the user. |
| `user_input.c` | Code for sending/receiving keystrokes from PuTTY. |
| `wfdb.c` | Code for parsing PhysioNet data. |

## Usage

When the connection with PuTTY is first made, a `>` character should appear, waiting for user input. Enter commands and submit them using **Enter/Return**.

### Commands

| Command | Description |
|---|---|
| `help` | Prints a string to the terminal. |
| `mount` | Mounts the file system to the SD card. Some commands will not work if the file system isn't mounted: `dir`, `ls`, `siginfo`, and `sigdat`. |
| `dir` | Prints the current directory. Use with an additional string separated by a space to move into a subdirectory, e.g. `dir MITBIH`. |
| `ls` | Lists all subdirectories and valid files (shown in green) in the current directory. |
| `siginfo` | Parses a given header file and prints its information, e.g. `siginfo 100`. |
| `sigdat` | Parses a given data file and prints the first ten data points from all channels, e.g. `sigdat 100`. |

## Hardware Config

Edit the `hw_config.c` file to match the board you are using.

New board: MISO = GP12, SCK = GP14, MOSI = GP15, SS = GP13.

Old board: MSIO = ?? ... 
