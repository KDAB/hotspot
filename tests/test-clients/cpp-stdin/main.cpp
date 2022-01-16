/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <iostream>
#include <string>

using namespace std;

int main()
{
    cout << "waiting for input..." << endl;

    string data;
    cin >> data;

    cout << "Received data:" << data << endl;
    return 0;
}
