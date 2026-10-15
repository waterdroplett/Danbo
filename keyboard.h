#ifndef KEYBOARD_H
#define KEYBOARD_H

void keyboard_init(void);
char keyboard_getchar(void);   // blocks (sleeping the CPU) until a key arrives

int keyboard_poll(char *out);  // returns 1 and fills *out if a key is waiting

#endif