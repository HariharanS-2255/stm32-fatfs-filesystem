#include "uart_cli.h"
#include "ff.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>

#define CLI_BUFFER_SIZE 128
static char cli_rx_buffer[CLI_BUFFER_SIZE];
static uint8_t cli_rx_index = 0;
static UART_HandleTypeDef *cli_uart = NULL;
static FATFS fs;
static volatile bool command_ready = false;

static void UART_CLI_ShowHelp(void);

void UART_CLI_Init(UART_HandleTypeDef *huart) {
    cli_uart = huart;
    cli_rx_index = 0;
    memset(cli_rx_buffer, 0, CLI_BUFFER_SIZE);

    if (f_mount(&fs, "", 1) == FR_OK) {
        UART_CLI_Printf("Mount OK\r\n");
    } else {
        UART_CLI_Printf("Mount FAIL\r\n");
    }

    UART_CLI_Printf("\r\nCLI Ready\r\nType 'help' for list of commands\r\n> ");
    HAL_UART_Receive_IT(cli_uart, (uint8_t *)&cli_rx_buffer[cli_rx_index], 1);
}

void UART_CLI_Process(void) {
    if (command_ready) {
        if (cli_rx_index > 0) {
            cli_rx_buffer[cli_rx_index] = '\0';

            char *cmd = cli_rx_buffer;
            char *args = NULL;
            char *space = strchr(cmd, ' ');
            if (space != NULL) {
                *space = '\0';
                args = space + 1;
                while (*args == ' ') args++;
            }

            /*** HELP COMMAND ***/
            if (strcmp(cmd, "help") == 0) {
                UART_CLI_ShowHelp();
            }

            /*** WRITE COMMAND ***/
            else if (strcmp(cmd, "write") == 0) {
                char *filename = args;
                char *content = NULL;
                if (filename) {
                    space = strchr(filename, ' ');
                    if (space != NULL) {
                        *space = '\0';
                        content = space + 1;
                    }
                }

                if (filename && content) {
                    FIL file;
                    if (f_open(&file, filename, FA_WRITE | FA_CREATE_ALWAYS) == FR_OK) {
                        UINT bw;
                        f_write(&file, content, strlen(content), &bw);
                        f_close(&file);
                        UART_CLI_Printf("OK\r\n");
                    } else {
                        UART_CLI_Printf("Write failed\r\n");
                    }
                } else {
                    UART_CLI_Printf("Usage: write <filename> <content>\r\n");
                }
            }

            /*** APPEND COMMAND ***/
            else if (strcmp(cmd, "append") == 0) {
                char *filename = args;
                char *content = NULL;
                if (filename) {
                    space = strchr(filename, ' ');
                    if (space != NULL) {
                        *space = '\0';
                        content = space + 1;
                    }
                }

                if (filename && content) {
                    FIL file;
                    if (f_open(&file, filename, FA_WRITE | FA_OPEN_APPEND) == FR_OK) {
                        UINT bw;
                        f_write(&file, content, strlen(content), &bw);
                        f_close(&file);
                        UART_CLI_Printf("Appended OK\r\n");
                    } else {
                        UART_CLI_Printf("Append failed\r\n");
                    }
                } else {
                    UART_CLI_Printf("Usage: append <filename> <content>\r\n");
                }
            }

            /*** READ COMMAND ***/
            else if (strcmp(cmd, "read") == 0) {
                if (args) {
                    FIL file;
                    char buffer[128];
                    if (f_open(&file, args, FA_READ) == FR_OK) {
                        while (f_gets((TCHAR*)buffer, sizeof(buffer), &file)) {
                            UART_CLI_Printf("%s", buffer);
                        }
                        f_close(&file);
                        UART_CLI_Printf("\r\n");
                    } else {
                        UART_CLI_Printf("Read failed\r\n");
                    }
                } else {
                    UART_CLI_Printf("Usage: read <filename>\r\n");
                }
            }

            /*** LS COMMAND ***/
            else if (strcmp(cmd, "ls") == 0) {
                DIR dir;
                FILINFO fno;
                if (f_opendir(&dir, "") == FR_OK) {
                    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
                        UART_CLI_Printf("%s\r\n", fno.fname);
                    }
                    f_closedir(&dir);
                } else {
                    UART_CLI_Printf("Failed to open directory\r\n");
                }
            }

            /*** MAKEFILE COMMAND ***/
            else if (strcmp(cmd, "makefile") == 0) {
                if (args) {
                    FIL file;
                    if (f_open(&file, args, FA_CREATE_ALWAYS | FA_WRITE) == FR_OK) {
                        f_close(&file);
                        UART_CLI_Printf("File created: %s\r\n", args);
                    } else {
                        UART_CLI_Printf("Failed to create file\r\n");
                    }
                } else {
                    UART_CLI_Printf("Usage: makefile <filename>\r\n");
                }
            }

            /*** DELETEFILE COMMAND ***/
            else if (strcmp(cmd, "deletefile") == 0) {
                if (args) {
                    if (f_unlink(args) == FR_OK) {
                        UART_CLI_Printf("File deleted: %s\r\n", args);
                    } else {
                        UART_CLI_Printf("Failed to delete file\r\n");
                    }
                } else {
                    UART_CLI_Printf("Usage: deletefile <filename>\r\n");
                }
            }

            /*** MKDIR COMMAND ***/
            else if (strcmp(cmd, "mkdir") == 0) {
                if (args) {
                    if (f_mkdir(args) == FR_OK) {
                        UART_CLI_Printf("Directory created: %s\r\n", args);
                    } else {
                        UART_CLI_Printf("Failed to create directory\r\n");
                    }
                } else {
                    UART_CLI_Printf("Usage: mkdir <dirname>\r\n");
                }
            }

            /*** RMDIR COMMAND ***/
            else if (strcmp(cmd, "rmdir") == 0) {
                if (args) {
                    if (f_unlink(args) == FR_OK) {
                        UART_CLI_Printf("Directory removed: %s\r\n", args);
                    } else {
                        UART_CLI_Printf("Failed to remove directory\r\n");
                    }
                } else {
                    UART_CLI_Printf("Usage: rmdir <dirname>\r\n");
                }
            }

            /*** CD COMMAND ***/
            else if (strcmp(cmd, "cd") == 0) {
                if (args) {
                    if (f_chdir(args) == FR_OK) {
                        UART_CLI_Printf("Changed directory to: %s\r\n", args);
                    } else {
                        UART_CLI_Printf("Failed to change directory\r\n");
                    }
                } else {
                    UART_CLI_Printf("Usage: cd <path>\r\n");
                }
            }

            /*** DF COMMAND ***/
            else if (strcmp(cmd, "df") == 0) {
                       FATFS* fs_ptr;
                       DWORD free_clusters, free_sectors, total_sectors;
                       FRESULT res = f_getfree("", &free_clusters, &fs_ptr);

                       if (res == FR_OK) {
                           total_sectors = (fs_ptr->n_fatent - 2) * fs_ptr->csize;
                           free_sectors  = free_clusters * fs_ptr->csize;

                           uint64_t total_bytes = (uint64_t)total_sectors * 512;
                           uint64_t free_bytes  = (uint64_t)free_sectors  * 512;

                           uint32_t total_gb_int = total_bytes / (1024ULL * 1024ULL * 1024ULL);
                           uint32_t total_gb_frac = (total_bytes % (1024ULL * 1024ULL * 1024ULL)) * 100 / (1024ULL * 1024ULL * 1024ULL);

                           uint32_t free_gb_int = free_bytes / (1024ULL * 1024ULL * 1024ULL);
                           uint32_t free_gb_frac = (free_bytes % (1024ULL * 1024ULL * 1024ULL)) * 100 / (1024ULL * 1024ULL * 1024ULL);

                           UART_CLI_Printf("Total size: %lu.%02lu GB\r\n", total_gb_int, total_gb_frac);
                           UART_CLI_Printf("Free size : %lu.%02lu GB\r\n", free_gb_int, free_gb_frac);
                       } else {
                           UART_CLI_Printf("f_getfree() failed: %d\r\n", res);
                       }
                   }

            /*** UNKNOWN COMMAND ***/
            else {
                UART_CLI_Printf("Unknown command: '%s'\r\n", cmd);
            }
        }

        // Reset for next command
        cli_rx_index = 0;
        memset(cli_rx_buffer, 0, CLI_BUFFER_SIZE);
        command_ready = false;
        UART_CLI_Printf("> ");
        HAL_UART_Receive_IT(cli_uart, (uint8_t *)&cli_rx_buffer[cli_rx_index], 1);
    }
}

static void UART_CLI_ShowHelp(void) {
    UART_CLI_Printf("Available commands:\r\n");
    UART_CLI_Printf(" help                - Show this help message\r\n");
    UART_CLI_Printf(" write <f> <text>    - Create/overwrite file with text\r\n");
    UART_CLI_Printf(" append <f> <text>   - Append text to file\r\n");
    UART_CLI_Printf(" read <f>            - Read file contents\r\n");
    UART_CLI_Printf(" ls                  - List files in directory\r\n");
    UART_CLI_Printf(" makefile <f>        - Create empty file\r\n");
    UART_CLI_Printf(" deletefile <f>      - Delete a file\r\n");
    UART_CLI_Printf(" mkdir <d>           - Create a directory\r\n");
    UART_CLI_Printf(" rmdir <d>           - Remove a directory\r\n");
    UART_CLI_Printf(" cd <path>           - Change directory\r\n");
    UART_CLI_Printf(" df                  - Show disk usage\r\n");
}

void UART_CLI_Printf(const char *format, ...) {
    char buffer[128];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    HAL_UART_Transmit(cli_uart, (uint8_t *)buffer, strlen(buffer), HAL_MAX_DELAY);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == cli_uart->Instance && !command_ready) {
        char received_char = cli_rx_buffer[cli_rx_index];

        if (received_char == '\r' || received_char == '\n') {
            HAL_UART_Transmit(huart, (uint8_t*)"\r\n", 2, 100);
            if (cli_rx_index > 0) {
                cli_rx_buffer[cli_rx_index] = '\0';
                command_ready = true;
                return;
            } else {
                UART_CLI_Printf("> ");
            }
            cli_rx_index = 0;
            memset(cli_rx_buffer, 0, CLI_BUFFER_SIZE);
        }
        else if (received_char == '\b' || received_char == 127) {
            if (cli_rx_index > 0) {
                cli_rx_index--;
                HAL_UART_Transmit(huart, (uint8_t*)"\b \b", 3, 100);
            }
        }
        else if (cli_rx_index < CLI_BUFFER_SIZE - 1 && received_char >= 32 && received_char <= 126) {
            cli_rx_index++;
            HAL_UART_Transmit(huart, (uint8_t*)&received_char, 1, 100); // Echo
        }

        HAL_UART_Receive_IT(cli_uart, (uint8_t *)&cli_rx_buffer[cli_rx_index], 1);
    }
}
