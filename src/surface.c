#include "vista.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void surface_init(struct surface *s, int w, int h) {
    if (w <= 0 || h <= 0) {
        fprintf(stderr, "vista: surface size %dx%d is not usable\n", w, h);
        exit(1);
    }
    s->w = w;
    s->h = h;
    s->front = calloc((size_t)w * h, sizeof *s->front);
    s->back = calloc((size_t)w * h, sizeof *s->back);
    if (!s->front || !s->back) { perror("calloc"); exit(1); }
    /* force a full first flush */
    memset(s->front, 1, (size_t)w * h * sizeof *s->front);
}

void surface_free(struct surface *s) {
    free(s->front);
    free(s->back);
}

void surface_clear(struct surface *s, char ch, uint8_t fg, uint8_t bg) {
    for (int i = 0; i < s->w * s->h; i++)
        s->back[i] = (struct cell){ch, fg, bg};
}

void surface_put(struct surface *s, int x, int y, char ch, uint8_t fg, uint8_t bg) {
    if (x < 0 || y < 0 || x >= s->w || y >= s->h) return;
    s->back[y * s->w + x] = (struct cell){ch, fg, bg};
}

void surface_text(struct surface *s, int x, int y, const char *text, uint8_t fg, uint8_t bg) {
    for (int i = 0; text[i]; i++) surface_put(s, x + i, y, text[i], fg, bg);
}

void surface_hline(struct surface *s, int x, int y, int n, char ch, uint8_t fg, uint8_t bg) {
    for (int i = 0; i < n; i++) surface_put(s, x + i, y, ch, fg, bg);
}

void surface_box(struct surface *s, int x, int y, int w, int h, uint8_t fg, uint8_t bg) {
    for (int i = 0; i < w; i++) {
        surface_put(s, x + i, y, '-', fg, bg);
        surface_put(s, x + i, y + h - 1, '-', fg, bg);
    }
    for (int i = 0; i < h; i++) {
        surface_put(s, x, y + i, '|', fg, bg);
        surface_put(s, x + w - 1, y + i, '|', fg, bg);
    }
    surface_put(s, x, y, '+', fg, bg);
    surface_put(s, x + w - 1, y, '+', fg, bg);
    surface_put(s, x, y + h - 1, '+', fg, bg);
    surface_put(s, x + w - 1, y + h - 1, '+', fg, bg);
}

static int ansi_fg(uint8_t c) { return c == V_DEFAULT ? 39 : 30 + c; }
static int ansi_bg(uint8_t c) { return c == V_DEFAULT ? 49 : 40 + c; }

long surface_flush(struct surface *s, FILE *out) {
    long written = 0;
    int cur_fg = -1, cur_bg = -1;
    for (int y = 0; y < s->h; y++) {
        for (int x = 0; x < s->w; x++) {
            struct cell *f = &s->front[y * s->w + x];
            struct cell *b = &s->back[y * s->w + x];
            if (f->ch == b->ch && f->fg == b->fg && f->bg == b->bg) continue;
            *f = *b;
            if ((int)b->fg != cur_fg || (int)b->bg != cur_bg) {
                written += fprintf(out, "\033[%d;%dm", ansi_fg(b->fg), ansi_bg(b->bg));
                cur_fg = b->fg;
                cur_bg = b->bg;
            }
            written += fprintf(out, "\033[%d;%dH%c", y + 1, x + 1, b->ch);
        }
    }
    if (written)    /* reset only when we actually set attributes */
        written += fprintf(out, "\033[0m");
    return written;
}

void surface_dump(const struct surface *s, FILE *out) {
    for (int y = 0; y < s->h; y++) {
        for (int x = 0; x < s->w; x++)
            fputc(s->back[y * s->w + x].ch, out);
        fputc('\n', out);
    }
}
