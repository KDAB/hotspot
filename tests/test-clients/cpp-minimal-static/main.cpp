/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

double __attribute__((noinline)) asdf(double a, double b)
{
    return (a * a) / (b * b);
}

double __attribute__((noinline)) bar(unsigned long max)
{
    double d = 1;
    for (unsigned long i = 0; i < max; ++i) {
        d *= asdf(0.1234E-12 * i, 12.345E67 / i);
    }
    return d;
}

int __attribute__((noinline)) foo(unsigned long max)
{
    return bar(max);
}

int main()
{
    return foo(123456789) > 0;
}
