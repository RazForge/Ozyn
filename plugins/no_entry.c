/*
 * no_entry.c — Shared library without the required entry symbol.
 *
 * This is a valid .so file but does not export ozayn_plugin_entry.
 * Plugin Manager must reject it during symbol resolution.
 *
 * Compile: gcc -shared -fPIC -o plugins/no_entry.so plugins/no_entry.c
 */

#include <stdio.h>

/* This library exports a function, but not the one Plugin Manager needs */
void some_random_function(void) {
    printf("This is not a plugin entry point.\n");
}
