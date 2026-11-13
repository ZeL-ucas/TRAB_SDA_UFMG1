#pragma once

#include <winsock2.h>
#include <stdio.h>
#include <iostream>
#include <mutex>

int threadClient(std::mutex *escrita);
