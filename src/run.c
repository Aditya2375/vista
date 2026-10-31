#include "vista.h"
#include "keys.h"

#include <errno.h>
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
    /* async-signal-safe: only record the signal; the main loop restores
       the terminal and re-raises so the exit status stays honest */
    got_quit = sig;
}

/* Read one key. Waits in bounded poll slices so a signal that arrives
   between the main loop's flag check and the read still wakes us: with
   no SA_RESTART the poll fails with EINTR and the flags are re-checked. */
static int read_key(struct keydec *kd) {
    for (;;) {
        if (got_quit || got_winch) return VK_NONE;
        struct pollfd pfd = { .fd = STDIN_FILENO, .events = POLLIN };
        int pr = poll(&pfd, 1, 100);
        if (pr < 0) {
            if (errno == EINTR) continue;
            return VK_NONE;
        }
        if (pr == 0) continue;   /* idle slice; loop re-checks flags */
        unsigned char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n <= 0) return VK_NONE;
        int k = keydec_feed(kd, c);
        if (k != VK_NONE) return k;
        /* a sequence is pending: give the rest of it 60 ms to arrive */
        struct pollfd efd = { .fd = STDIN_FILENO, .events = POLLIN };
        if (poll(&efd, 1, 60) <= 0)
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
    /* no SA_RESTART: a signal interrupts read/poll with EINTR so the loop
       notices flags immediately (live resize, prompt quit) */
    struct sigaction sa = { .sa_handler = on_quit };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sa.sa_handler = on_winch;
    sigaction(SIGWINCH, &sa, NULL);
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
    if (got_quit) {
        signal(got_quit, SIG_DFL);
        raise(got_quit);
    }
    return 0;
}
