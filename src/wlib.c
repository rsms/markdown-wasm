#include "common.h"
#include "wlib.h"

// globals of last error
static u32         errcode = 0;
static const char* errmsg = "";
static bool        errmsg_free = false;


export void* wrealloc(void* ptr, size_t size) {
  return realloc(ptr, size);
}

export void wfree(void* ptr) {
  free(ptr);
}


export u32 WErrGetCode() {
  return errcode;
}

export const char* WErrGetMsg() {
  return errmsg;
}


export void WErrClear() {
  errcode = 0;
  if (errmsg_free) {
    free((void*)errmsg);
    errmsg_free = false;
  }
  errmsg = NULL;
}


bool WErrSet(u32 code, const char* msg) {
  WErrClear();
  if (code != 0) {
    errcode = code;
    errmsg = msg;
    return true;
  }
  return false;
}
