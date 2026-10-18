#pragma once

#include <windows.h>

void service_start();
void service_stop();
void service_run(int argc, wchar_t** argv);
bool service_install();
bool service_remove();
bool service_is_installed();
