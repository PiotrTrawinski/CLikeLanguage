#pragma once

namespace COLOR {
    enum COLOR {
        BLACK = 0,
        DARK_BLUE,
        DARK_GREEN,
        DARK_CYAN,
        DARK_RED,
        DARK_MAGENTA,
        DARK_YELLOW,
        GREY,
        DARK_GREY,
        BLUE,
        GREEN,
        CYAN,
        RED,
        MAGENTA,
        YELLOW,
        WHITE,
    };
}

void setColor(int color);
void printfColorErr(int color, const char* format, ...);
void printfColor(int color, const char* format, ...);