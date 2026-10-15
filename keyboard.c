// keyboard.c
#include <stdint.h>
#include "keyboard.h"
#include "irq.h"
#include "io.h"
#include "term.h"

static const char sc_normal[58] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0, 'a','s','d','f','g','h','j','k','l',';','\'','`',
    0, '\\','z','x','c','v','b','n','m',',','.','/', 0,
    '*', 0, ' '
};

static const char sc_shifted[58] = {
    0, 27, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0, 'A','S','D','F','G','H','J','K','L',':','"','~',
    0, '|','Z','X','C','V','B','N','M','<','>','?', 0,
    '*', 0, ' '
};

static int shift = 0;

#define KB_BUF_SIZE 256
static volatile char kb_buf[KB_BUF_SIZE];
static volatile uint32_t kb_head = 0, kb_tail = 0;

static void kb_push(char c) {
    uint32_t next = (kb_head + 1) % KB_BUF_SIZE;
    if (next != kb_tail) {              // drop the key if the buffer is full
        kb_buf[kb_head] = c;
        kb_head = next;
    }
}

char keyboard_getchar(void) {
    for (;;) {
        __asm__ volatile ("cli");
        if (kb_tail != kb_head) {
            char c = kb_buf[kb_tail];
            kb_tail = (kb_tail + 1) % KB_BUF_SIZE;
            __asm__ volatile ("sti");
            return c;
        }
        __asm__ volatile ("sti; hlt");  // sti takes effect after the next instruction,
                                        // so no key can slip in between sti and hlt
    }
}

int keyboard_poll(char *out) {
    int got = 0;
    __asm__ volatile ("cli");
    if (kb_tail != kb_head) {
        *out = kb_buf[kb_tail];
        kb_tail = (kb_tail + 1) % KB_BUF_SIZE;
        got = 1;
    }
    __asm__ volatile ("sti");
    return got;
}

static void keyboard_callback(struct registers *r) {
    (void)r;
    uint8_t sc = inb(0x60);              // must read this or no more keys arrive

    if (sc & 0x80) {                     // key release
        uint8_t key = sc & 0x7F;
        if (key == 42 || key == 54) shift = 0;
        return;
    }
    if (sc == 42 || sc == 54) { shift = 1; return; }

    if (sc < 58) {
        char c = shift ? sc_shifted[sc] : sc_normal[sc];
        if (c) kb_push(c);
    }
}

void keyboard_init(void) {
    irq_install_handler(1, keyboard_callback);
}