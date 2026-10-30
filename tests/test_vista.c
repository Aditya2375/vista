#include "harness.h"
#include "vista.h"

#include <string.h>


static int activations = 0;
static void count_activate(struct widget *w, void *user) { (void)w; (void)user; activations++; }

int main(void) {
    /* surface primitives */
    struct surface s;
    surface_init(&s, 10, 4);
    surface_clear(&s, '.', V_WHITE, V_BLACK);
    surface_put(&s, 2, 1, 'X', V_RED, V_BLACK);
    surface_put(&s, -5, 0, 'B', V_RED, V_BLACK);   /* clipped: no crash */
    surface_box(&s, 0, 0, 10, 4, V_WHITE, V_BLACK);
    CHECK(s.back[1 * 10 + 2].ch == 'X', "put lands");
    CHECK(s.back[0].ch == '+', "box corner");
    CHECK(s.back[10].ch == '|', "box side");
    surface_free(&s);

    /* focus chain: tab cycles through focusables only */
    struct widget *root = vista_box(1, 0);
    widget_add(root, vista_label("title"));
    struct widget *b1 = vista_button("one", count_activate, NULL);
    struct widget *i1 = vista_input("type");
    struct widget *c1 = vista_checkbox("flag", 0);
    widget_add(root, b1);
    widget_add(root, i1);
    widget_add(root, c1);

    struct ui ui;
    ui_init(&ui, root, 40, 10);
    CHECK(ui.nfocus == 3, "three focusables");
    CHECK(b1->focused == 1, "first focused by default");
    ui_event(&ui, &(struct event){'k', VK_TAB});
    CHECK(i1->focused == 1, "tab to input");
    ui_event(&ui, &(struct event){'k', VK_TAB});
    ui_event(&ui, &(struct event){'k', VK_TAB});
    CHECK(b1->focused == 1, "tab wraps");

    /* button activation */
    ui_event(&ui, &(struct event){'k', VK_ENTER});
    CHECK(activations == 1, "enter activates button");

    /* checkbox toggles */
    ui_event(&ui, &(struct event){'k', VK_TAB});  /* to input */
    ui_event(&ui, &(struct event){'k', VK_TAB});  /* to checkbox */
    ui_event(&ui, &(struct event){'k', ' '});
    CHECK(c1->checked == 1, "space checks");
    ui_event(&ui, &(struct event){'k', ' '});
    CHECK(c1->checked == 0, "space unchecks");

    /* input editing: type, move, backspace */
    struct widget *inp = vista_input("hint");
    struct widget *r2 = vista_box(1, 0);
    widget_add(r2, inp);
    struct ui ui2;
    ui_init(&ui2, r2, 40, 5);
    const char *typed = "hello";
    for (const char *p = typed; *p; p++) ui_event(&ui2, &(struct event){'k', *p});
    CHECK_STR(inp->buf, "hello", "typed text lands");
    CHECK(inp->cursor == 5, "cursor at end");
    ui_event(&ui2, &(struct event){'k', VK_LEFT});
    ui_event(&ui2, &(struct event){'k', 127});   /* backspace eats 'l' before cursor */
    CHECK_STR(inp->buf, "helo", "mid-buffer backspace");
    CHECK(inp->cursor == 3, "cursor moved with edit");
    ui_event(&ui2, &(struct event){'k', 'X'});
    CHECK_STR(inp->buf, "helXo", "mid-buffer insert");
    ui_free(&ui2);
    widget_free(r2);

    /* list navigation */
    struct widget *list = vista_list();
    vista_list_add(list, "a");
    vista_list_add(list, "b");
    vista_list_add(list, "c");
    struct widget *r3 = vista_box(1, 0);
    widget_add(r3, list);
    struct ui ui3;
    ui_init(&ui3, r3, 20, 8);
    ui_event(&ui3, &(struct event){'k', VK_DOWN});
    ui_event(&ui3, &(struct event){'k', VK_DOWN});
    CHECK(list->selected == 2, "down down selects third");
    ui_event(&ui3, &(struct event){'k', VK_DOWN});
    CHECK(list->selected == 2, "down clamps at the end");
    ui_event(&ui3, &(struct event){'k', VK_UP});
    CHECK(list->selected == 1, "up moves back");
    ui_free(&ui3);
    widget_free(r3);

    /* layout: vbox stacks rows, hbox splits columns */
    struct widget *vb = vista_box(1, 1);
    widget_add(vb, vista_label("row1"));
    widget_add(vb, vista_label("row2"));
    struct ui ui4;
    ui_init(&ui4, vb, 40, 10);
    CHECK(vb->children[0]->y == 0 && vb->children[1]->y == 2, "vbox rows with pad");
    struct widget *hb = vista_box(0, 2);
    hb->h = 1;
    widget_add(hb, vista_button("l", NULL, NULL));
    widget_add(hb, vista_button("r", NULL, NULL));
    struct widget *outer = vista_box(1, 0);
    widget_add(outer, hb);
    struct ui ui5;
    ui_init(&ui5, outer, 42, 5);
    CHECK(hb->children[0]->x == 0 && hb->children[1]->x == 22, "hbox splits with pad");
    CHECK(hb->children[0]->w == 20 && hb->children[1]->w == 20, "equal columns");

    ui_free(&ui5);
    widget_free(outer);
    ui_free(&ui4);
    widget_free(vb);
    ui_free(&ui);
    widget_free(root);

    TEST_RESULTS();
}
