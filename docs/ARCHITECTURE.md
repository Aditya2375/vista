# vista architecture

## Layers

```text
app (widget tree + callbacks) -> ui (focus + event dispatch) -> widgets -> surface -> ANSI diff
                                 (widget.c)                                (surface.c)
```

## Widgets

Every widget is one struct with a kind tag and the union of per-kind state
(text, input buffer, checkbox flag, list items, box children). No vtables:
`widget_draw` and `widget_event` switch on kind. Containers are widgets
too (`W_BOX`), so the tree nests arbitrarily; layout is a top-down pass
where each box divides its rect among its children - vbox stacks natural
heights with padding, hbox splits equal columns.

## Focus and events

`ui_init` collects focusable widgets in tree order into a chain. Tab moves
the index (wrapping); every other event goes to the focused widget's
handler, which returns whether it consumed it. Buttons fire an
`on_activate(w, user)` callback; checkboxes toggle; inputs maintain a
buffer with a cursor (insert, backspace, left/right); lists move a clamped
selection. There is one event type - a key - and no timers or mouse, which
keeps the dispatch path tiny and fully deterministic.

## The surface

A double-buffered grid of `(char, fg, bg)` cells. All drawing is
immediate-mode into the back buffer each frame; `surface_flush` emits only
changed cells with cursor-addressing escapes, batching color changes.
`surface_dump` writes the back buffer as plain text - the exact same data
the terminal would show - which is the golden-test hook.

## Headless testing

`ui_run_headless` feeds scripted events through `ui_event` and draws once.
Because dispatch, state, and rendering are pure functions of the event
sequence, tests exercise the real code paths - the golden dump is a
byte-for-byte contract for what the user would see.

## The interactive loop

`ui_run` (run.c) sets raw mode (no canonical input, no echo, hidden
cursor), then loops: read a key (decoding CSI arrow sequences), dispatch,
draw, flush. On exit the terminal is restored exactly.
