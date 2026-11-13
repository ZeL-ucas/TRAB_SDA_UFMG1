
#pragma once
#include <mutex>
#include <atomic>
#include <condition_variable>

struct memoria {
    std::mutex mtxSaida;
    std::mutex mtxDados;

    float tempCamara = 0.0f;
    float pressaoCamara = 0.0f;
    float pressaoArgonio = 0.0f;
    float tempPanela = 0.0f;


    int spPressaoCamara = 0;
    float spTempCamara = 0.0f;
    double spPressaoArgonio = 0.0;


    std::atomic<bool> pedirSetPoints{ false };
    std::atomic<bool> novosSetPoints{ false };
    std::atomic<bool> encerrar{ false };
};