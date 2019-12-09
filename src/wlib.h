#pragma once

// WErrOK == no error
#define WErrOK 0

// WErrSet sets the code and message. Returns false if code == 0, true otherwise.
bool WErrSet(u32 code, const char* msg);

// clear error state
// sets code to WErrOK and message to ""
void WErrClear();

// read code
u32 WErrGetCode();

// read message
const char* WErrGetMsg();
