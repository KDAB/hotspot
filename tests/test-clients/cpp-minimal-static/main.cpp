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
