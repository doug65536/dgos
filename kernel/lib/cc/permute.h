#include "vector.h"

__BEGIN_NAMESPACE_EXT

template<typename _T, typename _F>
void permute(std::vector<_T>& list, _F const& callback, size_t level = 0){
    if (level == list.size()){
        callback(list);
        return;
    }

    for (size_t i = level; i < list.size(); ++i) {
        std::swap(list[level], list[i]);
        permute(list, callback, level + 1);
        std::swap(list[level], list[i]);
    }
}

__END_NAMESPACE_EXT
