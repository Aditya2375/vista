#include "vista.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

/* interactive event pump: raw stdin -> events -> draw/flush */

static int read_key(void) {
    unsigned char c;
    ssize_t n = read(STDIN_FILENO, &c, 1);
    if (n <= 0) return VK_NONE;
    if (c == 27) {
        unsigned char seq[2];
        if (read(STDIN_FILENO, &seq[0], 1) <= 0) return 27;
        if (read(STDIN_FILENO, &seq[1], 1) <= 0) return 27;
        if (seq[0] == '[') {
            switch (seq[1]) {
            case 'A': return VK_UP;
            case 'B': return VK_DOWN;
            case 'C': return VK_RIGHT;
            case 'D': return VK_LEFT;
            case 'Z': return -2;  /* shift-tab: treat as tab */
            }
        }
        return 27;
    }
    return c;
}

int ui_run(struct ui *ui) {
    struct termios saved, raw;
    if (tcgetattr(STDIN_FILENO, &saved) != 0) {
        fprintf(stderr, "vista: stdin is not a terminal\n");
        return 1;
    }
    raw = saved;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    printf("\033[?25l\033[2J");
    fflush(stdout);

    ui_draw(ui);
    surface_flush(&ui->surface, stdout);
    fflush(stdout);

    while (ui->running) {
        int key = read_key();
        if (key == VK_NONE) continue;
        if (key == -2) key = VK_TAB;
        struct event ev = {.type = 'k', .key = key};
        ui_event(ui, &ev);
        ui_draw(ui);
        surface_flush(&ui->surface, stdout);
        fflush(stdout);
    }

    printf("\033[?25h\033[0m\033[2J");
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved);
    return 0;
}
