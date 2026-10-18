#include "config.h"
#include <shlobj.h>

// All helpers are inline in config.h
// This TU exists to force linkage of shlobj / shell32 when needed
// and to provide a place for future non-inline config logic.

#pragma comment(lib, "shell32.lib")
