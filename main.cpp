#include <unistd.h>
#include "miswork.h"

int main(void) {
    mis::MisWork misWork;
    if (misWork.Init() < 0) {
        return 0;
    }
    if (misWork.Run() < 0) {
        return 0;
    }
    pause();
    return 0;
}

