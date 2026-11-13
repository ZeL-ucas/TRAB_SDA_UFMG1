#include <iostream>
#include "ThreadTeclado.h"
#include <mutex>
#include <conio.h>     
#include <windows.h>    
#include <stdio.h>
int threadTeclado(memoria* memoria) {
    while (!memoria->encerrar) {
        int tecla = _getch();
        if (tecla == 's' || tecla == 'S') {
            memoria->pedirSetPoints = true;
        }
        else if (tecla == 27 || tecla == 'q' || tecla == 'Q') {
            memoria->encerrar = true;
            break;
        }
        Sleep(50);
    }
    return 0;
}