#ifndef VISTA_KEYS_H
#define VISTA_KEYS_H

/* Incremental terminal key decoder: feed bytes, get key codes back.
 * Pure state machine with no IO, so tests can drive it directly. */

enum {
    VK_NONE = -1,       /* more bytes needed */
    VK_ESC = 27,
    VK_TAB = 9,
    VK_ENTER = 13,
    VK_UP = 256, VK_DOWN, VK_LEFT, VK_RIGHT,
    VK_HOME, VK_END, VK_PGUP, VK_PGDN, VK_DEL,
    VK_BTAB             /* shift-tab */
};

struct keydec {
    char buf[8];
    int  len;
};

void keydec_reset(struct keydec *kd);
/* Feed one input byte. Returns VK_NONE while a sequence is incomplete,
 * a VK_* code or the byte itself when a key is complete. */
int  keydec_feed(struct keydec *kd, unsigned char c);
/* Call when input goes quiet with a pending sequence: a lone ESC is a
 * real Escape key, anything else partial is discarded as VK_ESC too. */
int  keydec_timeout(struct keydec *kd);

#endif
