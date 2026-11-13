#include <iostream>
#include "ThreadTeclado.h"
#include <mutex>

int threadTeclado(std::mutex* escrita) {

	escrita->lock();
	printf ("Thread Teclado iniciada\n");
	escrita->unlock();

	return 0;
};