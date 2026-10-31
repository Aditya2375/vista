/* Browser demo glue: host the real demo form and feed it key events from
   the page; the surface buffer is drawn to a canvas. The raw-terminal
   input loop and ANSI flush stay native-only - see docs/. */
#include <emscripten.h>
#include <stdlib.h>

#include "../src/vista.h"
#include "../examples/demo.h"

struct host {
    struct ui ui;
    struct widget *root;
};

EMSCRIPTEN_KEEPALIVE struct host *vista_new(int w, int h) {
    struct host *host = malloc(sizeof *host);
    if (!host) return NULL;
    host->root = build_form(&host->ui);
    ui_init(&host->ui, host->root, w, h);
    ui_draw(&host->ui);
    return host;
}

EMSCRIPTEN_KEEPALIVE void vista_free(struct host *host) {
    if (!host) return;
    ui_free(&host->ui);
    widget_free(host->root);
    free(host);
}

/* Feed one key, redraw, report whether the UI is still running. */
EMSCRIPTEN_KEEPALIVE int vista_key(struct host *host, int key) {
    struct event ev = { 'k', key };
    ui_event(&host->ui, &ev);
    ui_draw(&host->ui);
    return host->ui.running;
}

EMSCRIPTEN_KEEPALIVE int vista_w(struct host *host) { return host->ui.surface.w; }
EMSCRIPTEN_KEEPALIVE int vista_h(struct host *host) { return host->ui.surface.h; }

/* Pack cell i as (ch << 16) | (fg << 8) | bg for cheap JS reads. */
EMSCRIPTEN_KEEPALIVE int vista_cell(struct host *host, int i) {
    struct cell c = host->ui.surface.back[i];
    return ((unsigned char)c.ch << 16) | (c.fg << 8) | c.bg;
}
