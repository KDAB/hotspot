/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <chrono>
#include <pthread.h>
#include <string>
#include <thread>

using namespace std;

int main()
{
    for (int i = 0; i < 10; ++i) {
        thread t([i]() {
            pthread_setname_np(pthread_self(), ("threadname" + to_string(i)).c_str());
            this_thread::sleep_for(chrono::milliseconds(100));
        });
        t.join();
    }
    return 0;
}
