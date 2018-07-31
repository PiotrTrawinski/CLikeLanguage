#include "errorMessages.h"

using namespace std;

void printErrorCodePosition(const CodePosition& codePosition) {
    cerr << codePosition.fileInfo->name << " (" << codePosition.lineNumber << ":" << codePosition.charNumber << ") ";
    /*cerr << "in file: " << codePosition.fileInfo->name << "\n"
         << "at line: " << codePosition.lineNumber << "\n"
         << "at char: " << codePosition.charNumber << "\n";*/
}

void printInterpretError(const string& message, const CodePosition& codePosition) {
    if (codePosition.fileInfo) {
        printErrorCodePosition(codePosition);
    }
    cerr << "Interpret Error: " << message << "\n";
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
    if (codePosition.fileInfo) {
        printErrorCodePosition(codePosition);
    }
    cerr << "Internal Error: " << message << "\n";
    exit(1);
}

void warningMessage(const string& message, const CodePosition& codePosition) {
    if (codePosition.fileInfo) {
        printErrorCodePosition(codePosition);
    }
    cerr << "Warning: " << message << "\n";
}