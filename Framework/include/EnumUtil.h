#pragma once

template<typename T>
void NextEnumName(T& e) {
    int current = static_cast<int>(e);
    int enum_size = static_cast<int>(T::count);
    current = (current + 1) % enum_size;
    e = static_cast<T>(current);
}

