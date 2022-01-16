/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <iostream>
#include <string>

unsigned long long __attribute__((noinline)) fibonacci(unsigned i)
{
    if (i == 0 || i == 1)
        return 1ull;
    return fibonacci(i - 1) + fibonacci(i - 2);
}

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "Usage: cpp-recursion N" << std::endl;
        return 1;
    }

    unsigned i = std::stoi(argv[1]);
    std::cout << "fib(" << i << ") = " << fibonacci(i) << std::endl;
    return 0;
}
