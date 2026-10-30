# vista

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
+--------------------------------------------------+
|small                                             |
|medium                                            |
|large                                             |
+--------------------------------------------------+
[ save ]                  [ quit ]
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

struct widget *root = vista_box(1, 1);              /* vertical, pad 1 */
widget_add(root, vista_label("Example settings"));
widget_add(root, vista_input("your name"));
widget_add(root, vista_checkbox("enable notifications", 1));
struct widget *list = vista_list();
vista_list_add(list, "small");
widget_add(root, list);
widget_add(root, vista_button("save", on_save, &ui));

struct ui ui;
ui_init(&ui, root, 50, 16);
ui_run(&ui);                                        /* interactive */
```

- **Widgets** are plain structs with a kind and per-kind state; containers
  (`vista_box`) lay children out in rows or columns with padding.
- **Focus** is a chain of the focusable widgets in tree order; `tab`
  cycles, `enter`/`space` activates, inputs take printable keys, arrows,
  and backspace, lists take up/down/enter.
- **Rendering** is immediate-mode onto a cell grid; `surface_flush` diffs
  front and back buffers and emits only the changes.
- **Headless mode**: `ui_run_headless(ui, events, n)` feeds a scripted
  event sequence through the real dispatch path - tests assert both widget
  state and the rendered text grid.

## Tested / not tested

Tested here: 21 unit checks (surface primitives and clipping, focus chain
and tab wrap, button activation, checkbox toggling, input editing with
mid-buffer insert/backspace, list navigation with clamping, vbox/hbox
layout math) plus a golden dump of a scripted form session: type "Adi",
uncheck notifications, move the list, press save, verify the status line.

Not tested: interactive use on a real terminal by a human (event decode
and rendering are verified headless and as text, not by eye on every
terminal emulator), window resize, mouse input, and scrolling lists
(16 items max, all visible).

## Architecture and tests

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md). `./tests/run-tests.sh`
runs the unit suite and the golden dump; CI runs the same on every push.
