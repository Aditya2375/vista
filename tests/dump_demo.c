/* golden scenario: type a name, toggle the checkbox, move the list, save. */
#include "demo.h"

int main(void) {
    struct ui ui;
    struct widget *root = build_form(&ui);
    ui_init(&ui, root, 50, 16);

    struct event script[] = {
        /* focus starts on the input? no - first focusable is the input's
           predecessor... type into the input after one tab from save? The
           box order: label, input, checkbox, list, row(save/quit), status.
           First focusable is the input. */
        {'k', 'A'}, {'k', 'd'}, {'k', 'i'},
        {'k', VK_TAB},              /* checkbox */
        {'k', ' '},                 /* uncheck notifications */
        {'k', VK_TAB},              /* list */
        {'k', VK_DOWN},             /* medium */
        {'k', VK_TAB},              /* save button (hbox row: save first) */
        {'k', VK_ENTER},            /* save */
    };
    ui_run_headless(&ui, script, sizeof script / sizeof script[0]);
    surface_dump(&ui.surface, stdout);
    ui_free(&ui);
    widget_free(root);
    return 0;
}
