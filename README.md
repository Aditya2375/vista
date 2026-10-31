# vista

**[Try the demo form in your browser](https://aditya2375.github.io/vista/)** - the real widget tree, layout and event loop compiled to WebAssembly.

A text-mode UI toolkit in C - in the Turbo Vision tradition, with no
ncurses or other dependencies. A widget tree (labels, buttons, inputs,
checkboxes, lists, layout boxes), a focus chain with tab navigation,
event dispatch, and a double-buffered surface that flushes only changed
cells as ANSI escapes. The same code runs interactively on a terminal or
headless under a scripted event feed, which is how the test suite plays
a whole form session for real.

```text
Example settings

Adi

[ ] enable notifications

+------------------------------------------------+
|small                                           |
|medium                                          |
|large                                           |
+------------------------------------------------+

[ save ]                    [ quit ]

saved settings for Adi
```

## Quick start

```bash
make
./build/demo         # tab moves focus, enter/space activates, type to edit
```

## Using it

```c
#include "vista.h"

static void on_save(struct widget *w, void *user) {
    (void)w; (void)user;                            /* do the thing */
}

int main(void) {
    struct widget *root = vista_box(1, 1);          /* vertical, pad 1 */
    widget_add(root, vista_label("Example settings"));
    widget_add(root, vista_input("your name"));
    widget_add(root, vista_checkbox("enable notifications", 1));
    struct widget *list = vista_list();
    vista_list_add(list, "small");
    widget_add(root, list);

    struct ui ui;
    widget_add(root, vista_button("save", on_save, &ui));
    ui_init(&ui, root, 50, 16);
    int rc = ui_run(&ui);                           /* interactive */
    ui_free(&ui);
    widget_free(root);
    return rc;
}
```

- **Widgets** are plain structs with a kind and per-kind state; containers
  (`vista_box`) lay children out in rows or columns with padding.
- **Focus** is a chain of the focusable widgets in tree order; `tab`
  and `shift-tab` cycle, `enter`/`space` activates. Inputs take printable
  keys, arrows, home/end, delete, and backspace, and scroll horizontally
  to keep the cursor visible. Lists take up/down/home/end/page keys and
  scroll any number of items through their window.
- **Rendering** is immediate-mode onto a cell grid; `surface_flush` diffs
  front and back buffers and emits only the changes.
- **Headless mode**: `ui_run_headless(ui, events, n)` feeds a scripted
  event sequence through the real dispatch path - tests assert both widget
  state and the rendered text grid.

## Tested / not tested

Tested here: 58 unit checks across two suites - surface primitives and
clipping, the flush diff (no change, no bytes), focus chain and tab wrap,
button activation, checkbox toggling, input editing (mid-buffer insert,
backspace, delete, home/end, horizontal scrolling), list navigation with
30 items scrolling through the window, hbox remainder distribution and
zero-width clamping, escape-sequence sanitization in widget text, the
terminal key decoder (lone Escape with a timeout, full CSI sequences,
dangling sequences), and live resize - plus a golden dump of a scripted
form session: type "Adi", uncheck notifications, move the list, press
save, verify the status line. CI also runs the suite under ASan/UBSan.

Not tested: interactive use on a real terminal by a human (event decode
and rendering are verified headless and as text, not by eye on every
terminal emulator) and mouse input. The interactive runner restores the
terminal on Ctrl-C/SIGTERM and re-lays out on SIGWINCH; both are wired
through signal handlers but exercised manually, not in CI.

## Architecture and tests

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md). `./tests/run-tests.sh`
runs the unit suite and the golden dump; CI runs the same on every push.
