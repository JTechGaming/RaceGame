#include "igor.hpp"
#include <iostream>

int multiply(int number, int amount) {
    int result = 0;
    for (int i=0;i<5;i++) {
        std::cout << i << std::endl;
    }

    for (int j=0;j<amount;j++) {
        result = add(result, number);
    }
    return result;
}

int add(int a, int b) {
    return a + b;
}