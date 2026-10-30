#ifndef VISTA_H
#define VISTA_H

#include <stdint.h>
#include <stdio.h>

/* vista - a text-mode UI toolkit: widget tree, layout, focus, events,
   double-buffered rendering to ANSI terminals or a headless buffer. */

enum {
    V_BLACK, V_RED, V_GREEN, V_YELLOW, V_BLUE, V_MAGENTA, V_CYAN, V_WHITE, V_DEFAULT
};

struct cell { char ch; uint8_t fg, bg; };

struct surface {
    int w, h;
    struct cell *front, *back;
};

/* w and h must both be positive; invalid sizes are fatal (exit 1). */
void surface_init(struct surface *s, int w, int h);
void surface_free(struct surface *s);
void surface_clear(struct surface *s, char ch, uint8_t fg, uint8_t bg);
void surface_put(struct surface *s, int x, int y, char ch, uint8_t fg, uint8_t bg);
void surface_text(struct surface *s, int x, int y, const char *text, uint8_t fg, uint8_t bg);
void surface_hline(struct surface *s, int x, int y, int n, char ch, uint8_t fg, uint8_t bg);
void surface_box(struct surface *s, int x, int y, int w, int h, uint8_t fg, uint8_t bg);
long surface_flush(struct surface *s, FILE *out);   /* ANSI diff; bytes written */
void surface_dump(const struct surface *s, FILE *out); /* plain text */

/* ---- events ---- */
#include "keys.h"

struct event {
    int type;           /* 'k' = key, 'r' = terminal resized */
    int key;
};

/* ---- widgets ---- */
enum widget_kind { W_LABEL, W_BUTTON, W_INPUT, W_CHECKBOX, W_LIST, W_BOX };

struct widget;
typedef void (*widget_action_fn)(struct widget *w, void *user);

struct widget {
    enum widget_kind kind;
    int x, y, w, h;             /* computed by layout */
    int focusable;
    int focused;
    char *text;                 /* label/button/checkbox text, input placeholder */
    /* input */
    char *buf;
    int len, cursor, cap;
    int scroll;                 /* W_INPUT: first visible column;
                                   W_LIST: first visible item */
    /* checkbox */
    int checked;
    /* list */
    char **items;
    int nitems, items_cap, selected;
    /* box layout */
    int vertical;               /* W_BOX: 1 = vbox, 0 = hbox */
    int pad;
    struct widget **children;
    int nchildren, children_cap;
    /* button and list activation */
    widget_action_fn on_activate;
    void *user;
};

struct widget *vista_label(const char *text);
struct widget *vista_button(const char *text, widget_action_fn on_activate, void *user);
struct widget *vista_input(const char *placeholder);
struct widget *vista_checkbox(const char *text, int checked);
struct widget *vista_list(void);
struct widget *vista_box(int vertical, int pad);
void vista_label_set(struct widget *label, const char *text);
/* Any number of items; the list scrolls to keep the selection visible. */
void vista_list_add(struct widget *list, const char *item);
void widget_add(struct widget *parent, struct widget *child);
/* Frees the widget and, recursively, every child added to it. */
void widget_free(struct widget *w);

/* ---- the UI root ---- */
struct ui {
    struct widget *root;        /* owned by the caller, not by ui_free */
    struct surface surface;
    struct widget **focus_chain;
    int nfocus;
    int focus_idx;
    int running;
    int needs_layout;
};

void ui_init(struct ui *ui, struct widget *root, int w, int h);
/* Frees vista-side state (surface, focus chain). The widget tree belongs
 * to the caller: release it with widget_free. */
void ui_free(struct ui *ui);
void ui_layout(struct ui *ui);
/* New terminal size: reallocates the surface and re-lays out. */
void ui_resize(struct ui *ui, int w, int h);
/* feed one event; returns 1 if a widget consumed it */
int ui_event(struct ui *ui, const struct event *ev);
void ui_draw(struct ui *ui);
/* headless: run n events then draw; for tests */
void ui_run_headless(struct ui *ui, const struct event *events, int nevents);
/* interactive on the terminal */
int ui_run(struct ui *ui);

#endif
