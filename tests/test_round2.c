#include "harness.h"
#include "vista.h"
#include "keys.h"

#include <string.h>

int main(void) {
    /* key decoder: lone ESC, full sequences, partial-then-timeout */
    struct keydec kd;
    keydec_reset(&kd);
    CHECK(keydec_feed(&kd, 'a') == 'a', "plain byte passes through");
    CHECK(keydec_feed(&kd, 27) == VK_NONE, "ESC starts a pending sequence");
    CHECK(keydec_timeout(&kd) == VK_ESC, "quiet after ESC means Escape");
    CHECK(keydec_feed(&kd, 27) == VK_NONE, "pending again");
    CHECK(keydec_feed(&kd, '[') == VK_NONE, "CSI introducer pending");
    CHECK(keydec_feed(&kd, 'A') == VK_UP, "CSI A is Up");
    keydec_reset(&kd);
    keydec_feed(&kd, 27); keydec_feed(&kd, '[');
    CHECK(keydec_feed(&kd, '3') == VK_NONE, "tilde sequence pending on digit");
    CHECK(keydec_feed(&kd, '~') == VK_DEL, "CSI 3~ is Delete");
    keydec_reset(&kd);
    keydec_feed(&kd, 27); keydec_feed(&kd, '[');
    CHECK(keydec_feed(&kd, '5') == VK_NONE, "pgup pending");
    CHECK(keydec_feed(&kd, '~') == VK_PGUP, "CSI 5~ is PgUp");
    keydec_reset(&kd);
    keydec_feed(&kd, 27); keydec_feed(&kd, '[');
    CHECK(keydec_feed(&kd, 'Z') == VK_BTAB, "shift-tab decoded");
    keydec_reset(&kd);
    keydec_feed(&kd, 27); keydec_feed(&kd, '[');
    CHECK(keydec_timeout(&kd) == VK_ESC, "dangling CSI times out as ESC");
    CHECK(keydec_feed(&kd, 'q') == 'q', "decoder recovers after timeout");

    /* a list with 30 items: no cap, scroll follows the selection */
    struct widget *list = vista_list();
    for (int i = 0; i < 30; i++) {
        char item[32];
        snprintf(item, sizeof item, "item %d", i);
        vista_list_add(list, item);
    }
    CHECK(list->nitems == 30, "list holds 30 items");
    struct widget *root = vista_box(1, 0);
    widget_add(root, list);
    struct ui ui;
    ui_init(&ui, root, 30, 20);
    struct event down = { 'k', VK_DOWN };
    for (int i = 0; i < 29; i++) ui_event(&ui, &down);
    CHECK(list->selected == 29, "selection reaches the last item");
    ui_draw(&ui);
    CHECK(list->scroll > 0, "list scrolled to follow");
    /* the selected row must be inside the visible window */
    CHECK(list->selected - list->scroll < list->h - 2, "selection visible");
    struct event end = { 'k', VK_HOME };
    ui_event(&ui, &end);
    CHECK(list->selected == 0, "Home jumps to the top");
    struct event pgdn = { 'k', VK_PGDN };
    ui_event(&ui, &pgdn);
    CHECK(list->selected > 1, "PgDn moves a page");
    ui_free(&ui);
    widget_free(root);

    /* input: horizontal scroll keeps the cursor visible */
    struct widget *in = vista_input("x");
    struct widget *r2 = vista_box(1, 0);
    widget_add(r2, in);
    struct ui ui2;
    ui_init(&ui2, r2, 12, 5);   /* input width 12 */
    for (int i = 0; i < 30; i++) {
        struct event t = { 'k', 'a' + i % 26 };
        ui_event(&ui2, &t);
    }
    CHECK(in->len == 30, "input holds 30 chars");
    ui_draw(&ui2);
    CHECK(in->scroll > 0, "input scrolled horizontally");
    CHECK(in->cursor - in->scroll < in->w, "cursor inside the window");
    struct event home = { 'k', VK_HOME };
    ui_event(&ui2, &home);
    CHECK(in->cursor == 0, "Home to start of input");
    struct event del = { 'k', VK_DEL };
    ui_event(&ui2, &del);
    CHECK(in->len == 29 && in->buf[0] == 'b', "Delete removes forward");
    struct event endk = { 'k', VK_END };
    ui_event(&ui2, &endk);
    CHECK(in->cursor == in->len, "End to end of input");
    ui_free(&ui2);
    widget_free(r2);

    /* hbox: remainder columns are handed out, nobody starves */
    struct widget *row = vista_box(0, 1);
    struct widget *c1 = vista_label("aa");
    struct widget *c2 = vista_label("bb");
    struct widget *c3 = vista_label("cc");
    widget_add(row, c1); widget_add(row, c2); widget_add(row, c3);
    struct ui ui3;
    ui_init(&ui3, row, 10, 3);   /* avail 8 across 3: 3,3,2 */
    CHECK(c1->w == 3 && c2->w == 3 && c3->w == 2, "remainder distributed");
    ui_free(&ui3);
    widget_free(row);

    struct widget *row2 = vista_box(0, 0);
    for (int i = 0; i < 4; i++) widget_add(row2, vista_label("z"));
    struct ui ui4;
    ui_init(&ui4, row2, 3, 3);   /* narrower than the child count */
    for (int i = 0; i < 4; i++)
        CHECK(row2->children[i]->w >= 1, "no zero-width child");
    ui_free(&ui4);
    widget_free(row2);

    /* escape injection through widget text is neutralized */
    struct widget *evil = vista_label("safe\033[2J\033[31m");
    CHECK(strchr(evil->text, 27) == NULL, "no ESC survives in label text");
    CHECK(strchr(evil->text, '?') != NULL, "control chars shown as ?");
    widget_free(evil);

    /* resize re-lays out */
    struct widget *r5 = vista_box(1, 0);
    struct widget *lbl = vista_label("wide");
    widget_add(r5, lbl);
    struct ui ui5;
    ui_init(&ui5, r5, 40, 10);
    CHECK(lbl->w == 40, "initial width");
    ui_resize(&ui5, 60, 12);
    CHECK(lbl->w == 60, "resize re-lays out");
    ui_free(&ui5);
    widget_free(r5);

    /* flush after no change writes nothing; after a change writes little */
    struct surface s;
    surface_init(&s, 8, 3);
    surface_clear(&s, ' ', V_WHITE, V_BLACK);
    FILE *f = tmpfile();
    surface_flush(&s, f);
    long first = ftell(f);
    long second = surface_flush(&s, f) ;
    CHECK(first > 0, "first flush paints");
    CHECK(second == 0, "unchanged flush writes nothing");
    surface_put(&s, 3, 1, 'X', V_RED, V_BLACK);
    long third = surface_flush(&s, f);
    CHECK(third > 0 && third < first, "diff flush writes only the change");
    fclose(f);
    surface_free(&s);

    TEST_RESULTS();
}
