#include <iostream>
#include "pigeon.hh"

int main() {
    int i = 0;
    for (long pi: pigeon()){
        if (i % 1000 == 0) std::cout << pi << " " << i << "\n"<< std::flush;
        ++i;

    }
}
