/* Live resize and quit-by-signal on a pty. The child runs the demo form;
   the parent first waits for the child to go IDLE (blocked waiting for a
   key), then: SIGWINCH after a window shrink must redraw without any
   keypress, and SIGINT must restore the terminal and kill the child with
   SIGINT (honest status) rather than leave it running or the tty raw. */
#include "vista.h"
#include "demo.h"

#include <pty.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static char buf[65536];
static size_t total;

/* 1 = bytes read, 0 = quiet for quiet_ms, -1 = EOF */
static int read_some(int master, int quiet_ms) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(master, &rfds);
    struct timeval tv = {.tv_sec = quiet_ms / 1000, .tv_usec = (quiet_ms % 1000) * 1000};
    if (select(master + 1, &rfds, NULL, NULL, &tv) <= 0) return 0;
    ssize_t n = read(master, buf + total, sizeof buf - total - 1);
    if (n <= 0) return -1;
    total += (size_t)n;
    buf[total] = '\0';
    return 1;
}

static void wait_idle(int master) {   /* drain until 500ms of silence */
    while (read_some(master, 500) > 0) { }
}

static void fail(pid_t pid, const char *msg) {
    fprintf(stderr, "signal test: %s\n", msg);
    kill(pid, SIGKILL);
    waitpid(pid, NULL, 0);
    exit(1);
}

int main(void) {
    struct winsize ws = {.ws_col = 60, .ws_row = 20};
    int master;
    pid_t pid = forkpty(&master, NULL, NULL, &ws);
    if (pid < 0) { perror("forkpty"); return 1; }
    if (pid == 0) {
        struct ui ui;
        struct widget *root = build_form(&ui);
        ui_init(&ui, root, 50, 16);
        exit(ui_run(&ui));
    }

    /* first frame, then silence = child blocked waiting for a key */
    int r = read_some(master, 5000);
    if (r <= 0) fail(pid, "no first frame");
    wait_idle(master);   /* child idle now */

    /* shrink + SIGWINCH: a live resize redraws with no keypress */
    ws = (struct winsize){.ws_col = 40, .ws_row = 12};
    if (ioctl(master, TIOCSWINSZ, &ws) != 0) { perror("TIOCSWINSZ"); fail(pid, "ioctl"); }
    if (kill(pid, SIGWINCH) != 0) { perror("kill"); fail(pid, "kill SIGWINCH"); }
    r = read_some(master, 2000);
    if (r == 0) fail(pid, "no redraw after SIGWINCH (resize not live)");
    if (r < 0) fail(pid, "child died on SIGWINCH");
    wait_idle(master);

    /* SIGINT: terminal restored, death by the signal itself */
    if (kill(pid, SIGINT) != 0) { perror("kill"); fail(pid, "kill SIGINT"); }
    while (read_some(master, 500) > 0) { /* drain */ }

    int status = 0;
    for (int i = 0; i < 50; i++) {   /* up to 5s for an honest death */
        pid_t w = waitpid(pid, &status, WNOHANG);
        if (w == pid) break;
        usleep(100000);
    }
    if (!WIFSIGNALED(status) || WTERMSIG(status) != SIGINT)
        fail(pid, "expected death by SIGINT");
    if (!strstr(buf, "\033[?1049l"))
        fail(pid, "alternate screen never left after SIGINT");
    printf("SIGNAL TEST PASSED (live resize, restored, died by SIGINT)\n");
    return 0;
}
