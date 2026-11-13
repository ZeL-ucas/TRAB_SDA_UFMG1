#pragma once

#include <mutex>

// Thread que recebe sinais da entrada padrão
int threadTeclado(std::mutex* escrita);
