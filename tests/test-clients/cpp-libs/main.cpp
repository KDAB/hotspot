/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sharedlib.h"
#include "staticlib.h"

#include <iostream>
#include <random>

int main()
{
    std::uniform_int_distribution<unsigned long> uniform(100000000, 200000000);
    std::default_random_engine engine;

    StaticLib staticLib(uniform(engine));
    SharedLib sharedLib(uniform(engine));

    std::cout << staticLib.foo() << '\n' << sharedLib.foo() << '\n';

    return 0;
}
