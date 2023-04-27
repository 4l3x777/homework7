#include <iostream>
#include "bulk.h"

int main(int argc, char* argv[])
{
    if (argc == 2) Handler(std::stoi(argv[1]));
    return 0;
}