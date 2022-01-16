/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <chrono>
#include <cmath>
#include <complex>
#include <iostream>
#include <random>
#include <thread>

using namespace std;

void burn()
{
    uniform_real_distribution<double> uniform(-1E5, 1E5);
    default_random_engine engine;
    double s = 0;
    for (int i = 0; i < 1000000; ++i) {
        s += norm(complex<double>(uniform(engine), uniform(engine)));
    }
    cout << s << endl;
}

int main()
{
    for (int i = 0; i < 10; ++i) {
        burn();
        this_thread::sleep_for(chrono::milliseconds(100));
    }
    burn();
    return 0;
}
