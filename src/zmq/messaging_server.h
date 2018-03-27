#ifndef MESSAGING_SERVER_H
#define MESSAGING_SERVER_H

#include "logger.h"
#include <zmq.hpp>
#include <string>
#include <iostream>
#ifndef _WIN32
#include <unistd.h>
#else
#include <windows.h>

#define sleep(n)    Sleep(n)
#endif


void messaging_test(int iterations);


#endif