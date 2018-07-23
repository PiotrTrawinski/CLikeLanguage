#pragma once

#include <memory>
#include <vector>

template<typename T> bool cmpPtr(const T* lhs, const T* rhs) {
    if (lhs == nullptr && rhs == nullptr) {
        return true;
    }
    if (lhs == nullptr) {
        return false;
    }
    if (rhs == nullptr) {
        return false;
    }
    return *lhs == *rhs;
    /*if ((lhs && !rhs) || (!lhs && rhs)) {
        return false;
    }
    return (!lhs && !rhs) || (*lhs == *rhs);*/
}
template<typename T> bool operator==(const std::unique_ptr<T>& lhs, const std::unique_ptr<T>& rhs) {
    if ((lhs && !rhs) || (!lhs && rhs)) {
        return false;
    }
    return (!lhs && !rhs) || (*lhs == *rhs);
}
template<typename T> bool operator==(const std::vector<T>& lhs, const std::vector<T>& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (int i = 0; i < lhs.size(); ++i) {
        if (!(lhs[i] == rhs[i])) {
            return false;
        }
    }
    return true;
}
template<typename T> bool operator==(const std::vector<T*>& lhs, const std::vector<T*>& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (int i = 0; i < lhs.size(); ++i) {
        if (!cmpPtr(lhs[i], rhs[i])) {
            return false;
        }
    }
    return true;
}