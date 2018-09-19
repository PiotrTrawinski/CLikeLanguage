#include "errorMessages.h"
#include <Windows.h>

using namespace std;

void setColor(WORD color) {
    SetConsoleTextAttribute(GetStdHandle(STD_ERROR_HANDLE), color);
}
void setWhiteColor() {
    setColor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
}
void setGrayColor() {
    setColor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}
void setRedColor() {
    setColor(FOREGROUND_RED | FOREGROUND_INTENSITY);
}
void setYellowColor() {
    setColor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
}
void setDarkGrayColor() {
    setColor(FOREGROUND_INTENSITY);
}

void printErrorCodePosition(const CodePosition& codePosition) {
    setDarkGrayColor();
    cerr << codePosition.fileInfo->name();
    setGrayColor();
    cerr << " (" << codePosition.lineNumber << ":" << codePosition.charNumber << ") ";
    setWhiteColor();
}

void printErrorCodeLine(const CodePosition& codePosition) {
    setWhiteColor();
    cerr << GVARS.sourceCode[codePosition.lineId].value << '\n';
    setGrayColor();
    for (int i = 1; i < codePosition.charNumber; ++i) {
        cerr << '-';
    }
    cerr << "^\n";
    setWhiteColor();
}

void printInterpretError(const string& message, const CodePosition& codePosition) {
    setGrayColor();
    if (codePosition.fileInfo) printErrorCodePosition(codePosition);
    setRedColor();
    cerr << "Interpret Error";
    setGrayColor();
    cerr << ": " << message << "\n";
    if (codePosition.fileInfo) printErrorCodeLine(codePosition);
    setWhiteColor();
}
bool errorMessageBool(const string& message, const CodePosition& codePosition) {
    printInterpretError(message, codePosition);
    return false;
}
nullopt_t errorMessageOpt(const string& message, const CodePosition& codePosition) {
    printInterpretError(message, codePosition);
    return nullopt;
}
nullptr_t errorMessageNull(const string& message, const CodePosition& codePosition) {
    printInterpretError(message, codePosition);
    return nullptr;
}

void internalError(const string& message, const CodePosition& codePosition) {
    setGrayColor();
    if (codePosition.fileInfo) printErrorCodePosition(codePosition);
    setRedColor();
    cerr << "Internal Error";
    setGrayColor();
    cerr << ": " << message << "\n";
    if (codePosition.fileInfo) printErrorCodeLine(codePosition);
    setWhiteColor();
    exit(1);
}

void warningMessage(const string& message, const CodePosition& codePosition) {
    setGrayColor();
    if (codePosition.fileInfo) printErrorCodePosition(codePosition);
    setYellowColor();
    cerr << "Warning";
    setGrayColor();
    cerr << ": " << message << "\n";
    if (codePosition.fileInfo) printErrorCodeLine(codePosition);
    setWhiteColor();
}