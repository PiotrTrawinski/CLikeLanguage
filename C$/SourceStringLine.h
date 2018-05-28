#pragma once
#include <string>

struct SourceStringLine {
    SourceStringLine(std::string line, int number) :
        line(line),
        number(number)
    {}

    std::string line;
    int number;
};