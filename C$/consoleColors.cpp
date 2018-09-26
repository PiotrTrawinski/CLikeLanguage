#include "consoleColors.h"
#include <Windows.h>
#include <stdio.h>
#include <stdarg.h>

void setColor(int color) {
    SetConsoleTextAttribute(GetStdHandle(STD_ERROR_HANDLE), color);
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

void printfColorErr(int color, const char* format, ...) {
    va_list args;
    va_start(args, format);
    setColor(color);
    vfprintf(stderr, format, args);
    va_end(args);
    setColor(COLOR::WHITE);
}
void printfColor(int color, const char* format, ...) {
    va_list args;
    va_start(args, format);
    setColor(color);
    vprintf(format, args);
    va_end(args);
    setColor(COLOR::WHITE);
}