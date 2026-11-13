#pragma once

#include "guest.h"

void Process_Argument(std::string* command, const char* argument);
std::string Process_Environment(char* const* entries, const char* temporary);
// Reserves guest memory, loads the images, and prepares the initial thread.
// gProcess.ExecutablePath must already be set.
void Process_Create(const Image* main, const Image* license);
