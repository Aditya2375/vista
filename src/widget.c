#include "vista.h"

#include <stdlib.h>
#include <string.h>

static struct widget *widget_new(enum widget_kind kind) {
    struct widget *w = calloc(1, sizeof *w);
    if (!w) { perror("calloc"); exit(1); }
    w->kind = kind;
    return w;
}

struct widget *vista_label(const char *text) {
    struct widget *w = widget_new(W_LABEL);
    snprintf(w->text, sizeof w->text, "%s", text);
    w->focusable = 0;
    w->h = 1;
    return w;
}

struct widget *vista_button(const char *text, widget_action_fn on_activate, void *user) {
    struct widget *w = widget_new(W_BUTTON);
    snprintf(w->text, sizeof w->text, "%s", text);
    w->focusable = 1;
    w->on_activate = on_activate;
    w->user = user;
    w->h = 1;
    return w;
}

struct widget *vista_input(const char *placeholder) {
    struct widget *w = widget_new(W_INPUT);
    snprintf(w->text, sizeof w->text, "%s", placeholder);
    w->focusable = 1;
    w->h = 1;
    return w;
}

struct widget *vista_checkbox(const char *text, int checked) {
    struct widget *w = widget_new(W_CHECKBOX);
    snprintf(w->text, sizeof w->text, "%s", text);
    w->checked = checked;
    w->focusable = 1;
    w->h = 1;
    return w;
}

struct widget *vista_list(void) {
    struct widget *w = widget_new(W_LIST);
    w->focusable = 1;
    w->h = 4;
    return w;
}

struct widget *vista_box(int vertical, int pad) {
    struct widget *w = widget_new(W_BOX);
    w->vertical = vertical;
    w->pad = pad;
    w->focusable = 0;
    return w;
}

void vista_list_add(struct widget *list, const char *item) {
    if (list->nitems >= 16) return;
    snprintf(list->items[list->nitems++], 64, "%s", item);
    if (list->h < list->nitems + 2) list->h = list->nitems + 2;  /* grow to fit */
}

void widget_add(struct widget *parent, struct widget *child) {
    if (parent->nchildren >= parent->children_cap) {
        parent->children_cap = parent->children_cap ? parent->children_cap * 2 : 8;
        parent->children = realloc(parent->children,
                                   (size_t)parent->children_cap * sizeof *parent->children);
        if (!parent->children) { perror("realloc"); exit(1); }
    }
    parent->children[parent->nchildren++] = child;
}

void widget_free(struct widget *w) {
    if (!w) return;
    for (int i = 0; i < w->nchildren; i++) widget_free(w->children[i]);
    free(w->children);
    free(w);
}

/* ---- layout: boxes split their rect among children ---- */
static void widget_layout(struct widget *w, int x, int y, int width) {
    w->x = x;
    w->y = y;
    if (w->kind == W_BOX) {
        int n = w->nchildren;
        if (!n) return;
        int pad_total = w->pad * (n - 1);
        if (w->vertical) {
            int cy = y;
            for (int i = 0; i < n; i++) {
                widget_layout(w->children[i], x, cy, width);
                cy += w->children[i]->h + w->pad;
            }
            w->h = cy - y - (n ? w->pad : 0);
            (void)pad_total;
        } else {
            int cw = (width - pad_total) / n;
            int cx = x;
            for (int i = 0; i < n; i++) {
                w->children[i]->h = w->h ? w->h : 1;
                widget_layout(w->children[i], cx, y, cw);
                cx += cw + w->pad;
            }
        }
        w->w = width;
    } else {
        w->w = width;
        if (w->kind == W_LIST && w->h < 2) w->h = 2;
    }
}

void ui_layout(struct ui *ui) {
    if (ui->root->kind == W_BOX) {
        if (ui->root->vertical) ui->root->h = ui->surface.h;
        widget_layout(ui->root, 0, 0, ui->surface.w);
    } else {
        widget_layout(ui->root, 0, 0, ui->surface.w);
    }
}

/* ---- drawing ---- */
static void widget_draw(struct widget *w, struct surface *s) {
    uint8_t fg = V_WHITE, bg = V_BLACK;
    switch (w->kind) {
    case W_LABEL:
        surface_text(s, w->x, w->y, w->text, V_CYAN, bg);
        break;
    case W_BUTTON: {
        char b[132];
        snprintf(b, sizeof b, "[ %s ]", w->text);
        if (w->focused) { fg = V_BLACK; bg = V_CYAN; }
        surface_text(s, w->x, w->y, b, fg, bg);
        break;
    }
    case W_INPUT: {
        if (w->focused) { fg = V_BLACK; bg = V_WHITE; }
        char shown[128];
        const char *content = w->len ? w->buf : w->text;
        snprintf(shown, sizeof shown, "%-*s", w->w > 0 && w->w < 120 ? w->w : 20,
                 w->len ? w->buf : content);
        surface_text(s, w->x, w->y, shown, w->len ? fg : V_DEFAULT, bg);
        if (w->focused && w->cursor <= w->len)
            surface_put(s, w->x + w->cursor, w->y, w->buf[w->cursor] ? w->buf[w->cursor] : ' ',
                        V_BLACK, V_GREEN);
        break;
    }
    case W_CHECKBOX: {
        char b[132];
        snprintf(b, sizeof b, "[%c] %s", w->checked ? 'x' : ' ', w->text);
        if (w->focused) { fg = V_YELLOW; }
        surface_text(s, w->x, w->y, b, fg, bg);
        break;
    }
    case W_LIST:
        surface_box(s, w->x, w->y, w->w, w->h, w->focused ? V_CYAN : V_WHITE, bg);
        for (int i = 0; i < w->nitems && i < w->h - 2; i++) {
            uint8_t ifg = V_WHITE, ibg = V_BLACK;
            if (w->focused && i == w->selected) { ifg = V_BLACK; ibg = V_CYAN; }
            char line[64];
            snprintf(line, sizeof line, "%-*s", w->w - 2, w->items[i]);
            surface_text(s, w->x + 1, w->y + 1 + i, line, ifg, ibg);
        }
        break;
    case W_BOX:
        for (int i = 0; i < w->nchildren; i++) widget_draw(w->children[i], s);
        break;
    }
}

void ui_draw(struct ui *ui) {
    surface_clear(&ui->surface, ' ', V_WHITE, V_BLACK);
    widget_draw(ui->root, &ui->surface);
}

/* ---- events ---- */
static int widget_event(struct widget *w, const struct event *ev) {
    switch (w->kind) {
    case W_BUTTON:
        if (ev->key == VK_ENTER || ev->key == ' ') {
            if (w->on_activate) w->on_activate(w, w->user);
            return 1;
        }
        return 0;
    case W_CHECKBOX:
        if (ev->key == VK_ENTER || ev->key == ' ') {
            w->checked = !w->checked;
            return 1;
        }
        return 0;
    case W_INPUT:
        if (ev->key >= 32 && ev->key < 127 && w->len < 120) {
            memmove(w->buf + w->cursor + 1, w->buf + w->cursor, (size_t)(w->len - w->cursor + 1));
            w->buf[w->cursor++] = (char)ev->key;
            w->len++;
            return 1;
        }
        if (ev->key == 127 || ev->key == 8) {
            if (w->cursor > 0) {
                memmove(w->buf + w->cursor - 1, w->buf + w->cursor, (size_t)(w->len - w->cursor + 1));
                w->cursor--;
                w->len--;
            }
            return 1;
        }
        if (ev->key == VK_LEFT && w->cursor > 0) { w->cursor--; return 1; }
        if (ev->key == VK_RIGHT && w->cursor < w->len) { w->cursor++; return 1; }
        return 0;
    case W_LIST:
        if (ev->key == VK_UP && w->selected > 0) { w->selected--; return 1; }
        if (ev->key == VK_DOWN && w->selected < w->nitems - 1) { w->selected++; return 1; }
        if (ev->key == VK_ENTER && w->on_activate) { w->on_activate(w, w->user); return 1; }
        return 0;
    default:
        return 0;
    }
}

static void collect_focusable(struct widget *w, struct ui *ui) {
    if (w->focusable) {
        ui->focus_chain = realloc(ui->focus_chain, (size_t)(ui->nfocus + 1) * sizeof *ui->focus_chain);
        if (!ui->focus_chain) { perror("realloc"); exit(1); }
        ui->focus_chain[ui->nfocus++] = w;
    }
    for (int i = 0; i < w->nchildren; i++) collect_focusable(w->children[i], ui);
}

static void focus_set(struct ui *ui, int idx) {
    for (int i = 0; i < ui->nfocus; i++) ui->focus_chain[i]->focused = 0;
    if (ui->nfocus) {
        ui->focus_idx = ((idx % ui->nfocus) + ui->nfocus) % ui->nfocus;
        ui->focus_chain[ui->focus_idx]->focused = 1;
    }
}

void ui_init(struct ui *ui, struct widget *root, int w, int h) {
    memset(ui, 0, sizeof *ui);
    ui->root = root;
    surface_init(&ui->surface, w, h);
    ui->running = 1;
    collect_focusable(root, ui);
    ui->focus_idx = -1;
    focus_set(ui, 0);
    ui_layout(ui);
}

void ui_free(struct ui *ui) {
    free(ui->focus_chain);
    surface_free(&ui->surface);
}

int ui_event(struct ui *ui, const struct event *ev) {
    if (ev->key == VK_TAB) {
        focus_set(ui, ui->focus_idx + 1);
        return 1;
    }
    if (ui->focus_idx >= 0)
        return widget_event(ui->focus_chain[ui->focus_idx], ev);
    return 0;
}

void ui_run_headless(struct ui *ui, const struct event *events, int nevents) {
    for (int i = 0; i < nevents && ui->running; i++)
        ui_event(ui, &events[i]);
    ui_draw(ui);
}
