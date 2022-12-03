/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "staticlib.h"

#include <complex>
#include <random>

namespace {
double asdf(double a, double b)
{
    return std::norm(std::complex<double>(a, b));
}
}

double StaticLib::bar(unsigned long max) const
{
    std::uniform_real_distribution<double> uniform(-1E5, 1E5);
    std::default_random_engine engine;
    double s = 0;
    for (unsigned long i = 0; i < max; ++i) {
        s += asdf(uniform(engine), uniform(engine));
    }
    return s;
}

double StaticLib::foo() const
{
    return bar(m_max);
}
