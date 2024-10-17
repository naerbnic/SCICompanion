#include "stdafx.h"

#include "ParserErrors.h"

char const errBinaryOp[] = "Expected second argument.";
char const errCaseArg[] = "Expected case value.";
char const errSwitchArg[] = "Expected switch argument.";
char const errSendObject[] = "Expected send object.";
char const errArgument[] = "Expected argument.";
char const errInteger[] = "Expected integer literal.";
char const errIntegerTooLarge[] = "Number too large. Largest 16-bit number is 65535.";
char const errIntegerTooSmall[] = "Number too small. Smallest 16-bit number is -32768.";
char const errThen[] = "Expected then clause.";
char const errVarName[] = "Expected variable name.";
char const errFileNameString[] = "Expected file name string.";
char const errElse[] = "Expected else clause.";
char const errNoKeywordOrSelector[] = "No keyword or selector permitted here.";