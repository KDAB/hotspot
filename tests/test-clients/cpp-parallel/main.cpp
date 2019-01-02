/*
  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2017-2019 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Milian Wolff <milian.wolff@kdab.com>

  Licensees holding valid commercial KDAB Hotspot licenses may use this file in
  accordance with Hotspot Commercial License Agreement provided with the Software.

  Contact info@kdab.com if any conditions of this licensing are not clear to you.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <cmath>
#include <complex>
#include <future>
#include <iostream>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

using namespace std;

double worker()
{
    uniform_real_distribution<double> uniform(-1E5, 1E5);
    default_random_engine engine;
    double s = 0;
    for (int i = 0; i < 10000000; ++i) {
        s += norm(complex<double>(uniform(engine), uniform(engine)));
    }
    cout << s << endl;
    return s;
}

int main(int argc, char** argv)
{
    const int numTasks = argc > 1 ? stoi(argv[1]) : std::thread::hardware_concurrency();
    vector<std::future<double>> results;
    for (int i = 0; i < numTasks; ++i) {
        results.push_back(async(launch::async, worker));
    }
    return 0;
}
