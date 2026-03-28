#include"../include/ConcurrentAlloc.h"
#include<iostream>
#include<thread>
#include<vector>
void ConcurrentAllocTest1() {
    for (int i = 0;i < 1024;i++) {
        void* ptr = ConcurrentAlloc(5);
        if (i == 0 || i==1023) {
            std::cout << ptr << std::endl;
        }
        
    }
    std::cout << "------------------------" << std::endl;
    void* ptr = ConcurrentAlloc(3);
    std::cout << ptr << std::endl; 
}

int main() {

    ConcurrentAllocTest1();
    int a = 0;
    return 0;
}