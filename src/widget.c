#include "vista.h"

#include <stdlib.h>
#include <string.h>

/* Control characters in widget text would let a label inject escape
 * sequences into the terminal; show them as '?' instead. */
static char *sanitize(const char *s) {
    size_t n = strlen(s);
    char *out = malloc(n + 1);
    if (!out) { perror("malloc"); exit(1); }
    for (size_t i = 0; i < n; i++)
        out[i] = (unsigned char)s[i] < 32 || s[i] == 127 ? '?' : s[i];
    out[n] = '\0';
    return out;
}

static struct widget *widget_new(enum widget_kind kind) {
    struct widget *w = calloc(1, sizeof *w);
    if (!w) { perror("calloc"); exit(1); }
    w->kind = kind;
    return w;
}

struct widget *vista_label(const char *text) {
    struct widget *w = widget_new(W_LABEL);
    w->text = sanitize(text);
    w->h = 1;
    return w;
}

struct widget *vista_button(const char *text, widget_action_fn on_activate, void *user) {
    struct widget *w = widget_new(W_BUTTON);
    w->text = sanitize(text);
    w->focusable = 1;
    w->on_activate = on_activate;
    w->user = user;
    w->h = 1;
    return w;
}

struct widget *vista_input(const char *placeholder) {
    struct widget *w = widget_new(W_INPUT);
    w->text = sanitize(placeholder);
    w->focusable = 1;
    w->h = 1;
    return w;
}

struct widget *vista_checkbox(const char *text, int checked) {
    struct widget *w = widget_new(W_CHECKBOX);
    w->text = sanitize(text);
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
    return w;
}

/* Replace a label's text (takes a sanitized copy). */
void vista_label_set(struct widget *w, const char *text) {
    char *fresh = sanitize(text);
    free(w->text);
    w->text = fresh;
}

void vista_list_add(struct widget *list, const char *item) {
    if (list->nitems >= list->items_cap) {
        list->items_cap = list->items_cap ? list->items_cap * 2 : 16;
        list->items = realloc(list->items, (size_t)list->items_cap * sizeof *list->items);
        if (!list->items) { perror("realloc"); exit(1); }
    }
    list->items[list->nitems++] = sanitize(item);
    if (list->h < list->nitems + 2 && list->h < 12)
        list->h = list->nitems + 2;   /* grow up to a sane window */
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
    for (int i = 0; i < w->nitems; i++) free(w->items[i]);
    free(w->items);
    free(w->children);
    free(w->text);
    free(w->buf);
    free(w);
}

/* ---- layout: boxes split their rect among children ---- */
static void widget_layout(struct widget *w, int x, int y, int width) {
    if (width < 1) width = 1;
    w->x = x;
    w->y = y;
    if (w->kind == W_BOX) {
        int n = w->nchildren;
        if (!n) { w->w = width; return; }
        int pad_total = w->pad * (n - 1);
        if (w->vertical) {
            int cy = y;
            for (int i = 0; i < n; i++) {
                widget_layout(w->children[i], x, cy, width);
                cy += w->children[i]->h + w->pad;
            }
            w->h = cy - y - w->pad;
        } else {
            /* share what the padding leaves; the remainder goes to the
             * first children, one column each, and nobody gets zero */
            int avail = width - pad_total;
            if (avail < n) avail = n;
            int cw = avail / n, rem = avail % n;
            int cx = x;
            for (int i = 0; i < n; i++) {
                int mine = cw + (i < rem ? 1 : 0);
                if (!w->children[i]->h) w->children[i]->h = 1;
                widget_layout(w->children[i], cx, y, mine);
                cx += mine + w->pad;
            }
        }
        w->w = width;
    } else {
        w->w = width;
        if (w->kind == W_LIST && w->h < 2) w->h = 2;
    }
}

void ui_layout(struct ui *ui) {
    if (ui->root->kind == W_BOX && ui->root->vertical)
        ui->root->h = ui->surface.h;
    widget_layout(ui->root, 0, 0, ui->surface.w);
    ui->needs_layout = 0;
}

/* ---- drawing ---- */
static void widget_draw(struct widget *w, struct surface *s) {
    uint8_t fg = V_WHITE, bg = V_BLACK;
    switch (w->kind) {
    case W_LABEL:
        surface_text(s, w->x, w->y, w->text, V_CYAN, bg);
        break;
    case W_BUTTON: {
        size_t n = strlen(w->text) + 5;
        char *b = malloc(n);
        if (!b) { perror("malloc"); exit(1); }
        snprintf(b, n, "[ %s ]", w->text);
        if (w->focused) { fg = V_BLACK; bg = V_CYAN; }
        surface_text(s, w->x, w->y, b, fg, bg);
        free(b);
        break;
    }
    case W_INPUT: {
        if (w->focused) { fg = V_BLACK; bg = V_WHITE; }
        int width = w->w > 0 ? w->w : 20;
        /* keep the cursor inside the visible window */
        if (w->cursor < w->scroll) w->scroll = w->cursor;
        if (w->cursor >= w->scroll + width) w->scroll = w->cursor - width + 1;
        const char *content = w->len ? w->buf : w->text;
        int clen = w->len ? w->len : (int)strlen(content);
        for (int i = 0; i < width; i++) {
            int idx = w->scroll + i;
            char c = idx < clen ? content[idx] : ' ';
            surface_put(s, w->x + i, w->y, c, w->len ? fg : V_DEFAULT, bg);
        }
        if (w->focused) {
            int cx = w->cursor - w->scroll;
            if (cx >= 0 && cx < width) {
                int idx = w->cursor;
                char c = idx < w->len ? w->buf[idx] : ' ';
                surface_put(s, w->x + cx, w->y, c, V_BLACK, V_GREEN);
            }
        }
        break;
    }
    case W_CHECKBOX: {
        size_t n = strlen(w->text) + 5;
        char *b = malloc(n);
        if (!b) { perror("malloc"); exit(1); }
        snprintf(b, n, "[%c] %s", w->checked ? 'x' : ' ', w->text);
        if (w->focused) fg = V_YELLOW;
        surface_text(s, w->x, w->y, b, fg, bg);
        free(b);
        break;
    }
    case W_LIST: {
        surface_box(s, w->x, w->y, w->w, w->h, w->focused ? V_CYAN : V_WHITE, bg);
        int view = w->h - 2;
        /* scroll so the selection is always visible */
        if (w->selected < w->scroll) w->scroll = w->selected;
        if (w->selected >= w->scroll + view) w->scroll = w->selected - view + 1;
        for (int i = 0; i < view && w->scroll + i < w->nitems; i++) {
            int idx = w->scroll + i;
            uint8_t ifg = V_WHITE, ibg = V_BLACK;
            if (w->focused && idx == w->selected) { ifg = V_BLACK; ibg = V_CYAN; }
            int inner = w->w - 2;
            for (int c = 0; c < inner; c++) {
                char ch = c < (int)strlen(w->items[idx]) ? w->items[idx][c] : ' ';
                surface_put(s, w->x + 1 + c, w->y + 1 + i, ch, ifg, ibg);
            }
        }
        /* scrollbar hint when items overflow the window */
        if (w->nitems > view && view > 0) {
            int thumb = w->y + 1 + w->scroll * view / w->nitems;
            surface_put(s, w->x + w->w - 1, thumb, '#', V_CYAN, bg);
        }
        break;
    }
    case W_BOX:
        for (int i = 0; i < w->nchildren; i++) widget_draw(w->children[i], s);
        break;
    }
}

void ui_draw(struct ui *ui) {
    if (ui->needs_layout) ui_layout(ui);
    surface_clear(&ui->surface, ' ', V_WHITE, V_BLACK);
    widget_draw(ui->root, &ui->surface);
}

/* ---- events ---- */
static int widget_event(struct widget *w, const struct event *ev) {
    if (ev->type != 'k') return 0;
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
        if (ev->key >= 32 && ev->key < 127) {
            if (w->len + 2 > w->cap) {   /* content plus the terminator */
                w->cap = w->cap ? w->cap * 2 : 64;
                w->buf = realloc(w->buf, (size_t)w->cap);
                if (!w->buf) { perror("realloc"); exit(1); }
            }
            memmove(w->buf + w->cursor + 1, w->buf + w->cursor, (size_t)(w->len - w->cursor));
            w->buf[w->cursor++] = (char)ev->key;
            w->len++;
            w->buf[w->len] = '\0';
            return 1;
        }
        if (ev->key == 127 || ev->key == 8) {
            if (w->cursor > 0) {
                memmove(w->buf + w->cursor - 1, w->buf + w->cursor, (size_t)(w->len - w->cursor));
                w->cursor--;
                w->len--;
                w->buf[w->len] = '\0';
            }
            return 1;
        }
        if (ev->key == VK_DEL && w->cursor < w->len) {
            memmove(w->buf + w->cursor, w->buf + w->cursor + 1, (size_t)(w->len - w->cursor - 1));
            w->len--;
            w->buf[w->len] = '\0';
            return 1;
        }
        if (ev->key == VK_LEFT && w->cursor > 0) { w->cursor--; return 1; }
        if (ev->key == VK_RIGHT && w->cursor < w->len) { w->cursor++; return 1; }
        if (ev->key == VK_HOME) { w->cursor = 0; return 1; }
        if (ev->key == VK_END) { w->cursor = w->len; return 1; }
        return 0;
    case W_LIST:
        if (ev->key == VK_UP && w->selected > 0) { w->selected--; return 1; }
        if (ev->key == VK_DOWN && w->selected < w->nitems - 1) { w->selected++; return 1; }
        if (ev->key == VK_PGUP && w->selected > 0) {
            w->selected -= w->h - 2;
            if (w->selected < 0) w->selected = 0;
            return 1;
        }
        if (ev->key == VK_PGDN && w->selected < w->nitems - 1) {
            w->selected += w->h - 2;
            if (w->selected > w->nitems - 1) w->selected = w->nitems - 1;
            return 1;
        }
        if (ev->key == VK_HOME) { w->selected = 0; return w->nitems > 0; }
        if (ev->key == VK_END && w->nitems) { w->selected = w->nitems - 1; return 1; }
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

void ui_resize(struct ui *ui, int w, int h) {
    surface_free(&ui->surface);
    surface_init(&ui->surface, w, h);
    ui->needs_layout = 1;
    ui_draw(ui);
}

int ui_event(struct ui *ui, const struct event *ev) {
    if (ev->type != 'k') return 0;
    if (ev->key == VK_TAB) {
        focus_set(ui, ui->focus_idx + 1);
        return 1;
    }
    if (ev->key == VK_BTAB) {
        focus_set(ui, ui->focus_idx - 1);
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
