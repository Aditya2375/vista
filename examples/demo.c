/* vista demo: a settings form - name input, checkbox, list, buttons. */
#include "demo.h"

#include <stdio.h>
#include <string.h>

static struct widget *status_label;

static void on_save(struct widget *w, void *user) {
    struct ui *ui = user;
    (void)w;
    /* find the input's content through the focus chain */
    const char *name = "";
    for (int i = 0; i < ui->nfocus; i++)
        if (ui->focus_chain[i]->kind == W_INPUT && ui->focus_chain[i]->len)
            name = ui->focus_chain[i]->buf;
    snprintf(status_label->text, sizeof status_label->text,
             "saved settings for %.80s", name[0] ? name : "(no name)");
}

static void on_quit(struct widget *w, void *user) {
    (void)w;
    ((struct ui *)user)->running = 0;
}

struct widget *build_form(struct ui *ui) {
    struct widget *root = vista_box(1, 1);
    widget_add(root, vista_label("Example settings"));
    struct widget *name = vista_input("your name");
    widget_add(root, name);
    widget_add(root, vista_checkbox("enable notifications", 1));
    struct widget *list = vista_list();
    vista_list_add(list, "small");
    vista_list_add(list, "medium");
    vista_list_add(list, "large");
    widget_add(root, list);
    struct widget *row = vista_box(0, 2);
    row->h = 1;
    widget_add(row, vista_button("save", on_save, ui));
    widget_add(row, vista_button("quit", on_quit, ui));
    widget_add(root, row);
    status_label = vista_label("tab moves focus, enter activates");
    widget_add(root, status_label);
    return root;
}

#ifndef DEMO_NO_MAIN
int main(void) {
    struct ui ui;
    struct widget *root = build_form(&ui);
    ui_init(&ui, root, 50, 16);
    int rc = ui_run(&ui);
    ui_free(&ui);
    widget_free(root);
    return rc;
}
#endif
