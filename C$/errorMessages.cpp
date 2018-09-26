#include "errorMessages.h"

using namespace std;

void printErrorCodePosition(const CodePosition& codePosition) {
    printfColorErr(COLOR::MAGENTA, "%s", codePosition.fileInfo->name().c_str());
    printfColorErr(COLOR::GREY, " (%d:%d) ", codePosition.lineNumber, codePosition.charNumber);
}

void printErrorCodeLine(const CodePosition& codePosition) {
    printfColorErr(COLOR::WHITE, "%s\n", GVARS.sourceCode[codePosition.lineId].value.c_str());
    for (int i = 1; i < codePosition.charNumber; ++i) {
        printfColorErr(COLOR::GREY, "-");
    }
    printfColorErr(COLOR::GREY, "^\n");
}

void printInterpretError(const string& message, const CodePosition& codePosition) {
    if (codePosition.fileInfo) printErrorCodePosition(codePosition);
    printfColorErr(COLOR::RED, "Interpret Error");
    printfColorErr(COLOR::GREY, ": %s\n", message.c_str());
    if (codePosition.fileInfo) printErrorCodeLine(codePosition);
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
    if (codePosition.fileInfo) printErrorCodePosition(codePosition);
    printfColorErr(COLOR::RED, "Internal Error");
    printfColorErr(COLOR::GREY, ": %s\n", message.c_str());
    if (codePosition.fileInfo) printErrorCodeLine(codePosition);
    exit(1);
}

void warningMessage(const string& message, const CodePosition& codePosition) {
    if (codePosition.fileInfo) printErrorCodePosition(codePosition);
    printfColorErr(COLOR::YELLOW, "Warning");
    printfColorErr(COLOR::GREY, ": %s\n", message.c_str());
    if (codePosition.fileInfo) printErrorCodeLine(codePosition);
}