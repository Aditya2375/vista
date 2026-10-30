#include "keys.h"

void keydec_reset(struct keydec *kd) { kd->len = 0; }

int keydec_feed(struct keydec *kd, unsigned char c) {
    if (kd->len == 0) {
        if (c == 27) { kd->buf[kd->len++] = (char)c; return VK_NONE; }
        return c;
    }
    if (kd->len >= (int)sizeof kd->buf - 1) {  /* absurd sequence: drop it */
        kd->len = 0;
        return VK_ESC;
    }
    kd->buf[kd->len++] = (char)c;
    if (kd->len == 2) {
        if (c != '[' && c != 'O') {            /* ESC + other: alt-key, we take ESC */
            kd->len = 0;
            return VK_ESC;
        }
        return VK_NONE;
    }
    /* ESC [ ... */
    char f = kd->buf[2];
    if (f >= 'A' && f <= 'Z') {                /* final letter */
        kd->len = 0;
        switch (f) {
        case 'A': return VK_UP;
        case 'B': return VK_DOWN;
        case 'C': return VK_RIGHT;
        case 'D': return VK_LEFT;
        case 'H': return VK_HOME;
        case 'F': return VK_END;
        case 'Z': return VK_BTAB;
        default:  return VK_ESC;
        }
    }
    if (f >= '0' && f <= '9') {
        if (c == '~' && kd->len == 4) {        /* single-digit forms only */
            int key = VK_ESC;
            switch (f) {
            case '1': case '7': key = VK_HOME; break;
            case '4': case '8': key = VK_END; break;
            case '3': key = VK_DEL; break;
            case '5': key = VK_PGUP; break;
            case '6': key = VK_PGDN; break;
            }
            kd->len = 0;
            return key;
        }
        if (c >= '0' && c <= '9') return VK_NONE;   /* more digits coming */
    }
    kd->len = 0;
    return VK_ESC;
}

int keydec_timeout(struct keydec *kd) {
    kd->len = 0;
    return VK_ESC;
}
