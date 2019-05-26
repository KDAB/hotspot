/*
  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2019 Erik Johansson <erik@ejohansson.se>

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

#include <deque>
#include <list>
#include <map>
#include <set>
#include <stack>
#include <string>
#include <vector>

#if __cplusplus >= 201103L
#include <array>
#include <forward_list>
#include <unordered_set>
#include <unordered_map>
#endif

template <typename T>
void doWork(T& t)
{
    typename T::value_type value;
    t.insert(t.end(), value);
}

#if __cplusplus >= 201103L
template <typename T>
void doWork(std::forward_list<T>& t)
{
    T value;
    t.push_front(value);
}
#endif

template <typename T>
T returnType()
{
    T t;
    for (size_t i = 0; i < 1000000; ++i) {
        doWork(t);
    }
    T copy = t;
    return t;
}

namespace mystd {
template <typename T>
struct Wrapper : public T {};
}

int main()
{
    returnType<std::string>();
    returnType<std::wstring>();
    returnType<std::vector<std::string> >();
    returnType<std::map<std::string, std::vector<std::map<int, float> > > >();
    returnType<mystd::Wrapper<std::list<std::string> > >();

#if __cplusplus >= 201103L
    returnType<std::forward_list<int> >();
#endif

    return 0;
}
