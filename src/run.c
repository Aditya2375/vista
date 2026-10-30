#include "vista.h"
#include "keys.h"

#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

/* interactive event pump: raw stdin -> events -> draw/flush */

static struct termios saved_termios;
static volatile sig_atomic_t got_winch = 0;
static volatile sig_atomic_t got_quit = 0;

static void term_restore(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_termios);
    /* leave the alternate screen, show the cursor, reset attrs */
    const char seq[] = "\033[?1049l\033[?25h\033[0m";
    ssize_t w = write(STDOUT_FILENO, seq, sizeof seq - 1);
    (void)w;
}

static void on_winch(int sig) { (void)sig; got_winch = 1; }

static void on_quit(int sig) {
    /* restore the terminal, then re-raise so the exit status is honest */
    term_restore();
    signal(sig, SIG_DFL);
    raise(sig);
}

/* Read one key, waiting at most timeout_ms when a sequence is pending. */
static int read_key(struct keydec *kd) {
    for (;;) {
        unsigned char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n <= 0) return VK_NONE;
        int k = keydec_feed(kd, c);
        if (k != VK_NONE) return k;
        /* a sequence is pending: give the rest of it 60 ms to arrive */
        struct pollfd pfd = { .fd = STDIN_FILENO, .events = POLLIN };
        if (poll(&pfd, 1, 60) <= 0)
            return keydec_timeout(kd);   /* a lone Escape is a real Escape */
    }
}

int ui_run(struct ui *ui) {
    if (tcgetattr(STDIN_FILENO, &saved_termios) != 0) {
        fprintf(stderr, "vista: stdin is not a terminal\n");
        return 1;
    }
    struct termios raw = saved_termios;
    raw.c_lflag &= (tcflag_t)~(ICANON | ECHO);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    atexit(term_restore);
    signal(SIGINT, on_quit);
    signal(SIGTERM, on_quit);
    signal(SIGWINCH, on_winch);
    {
        const char seq[] = "\033[?1049h\033[?25l";   /* alt screen, hide cursor */
        ssize_t w = write(STDOUT_FILENO, seq, sizeof seq - 1);
        (void)w;
    }

    struct keydec kd;
    keydec_reset(&kd);
    while (ui->running && !got_quit) {
        if (got_winch) {
            got_winch = 0;
            struct winsize ws;
            if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row && ws.ws_col)
                ui_resize(ui, ws.ws_col, ws.ws_row);
        }
        ui_draw(ui);
        surface_flush(&ui->surface, stdout);
        fflush(stdout);
        int key = read_key(&kd);
        if (key == VK_NONE) continue;
        struct event ev = { .type = 'k', .key = key };
        ui_event(ui, &ev);
    }
    term_restore();
    return 0;
}
