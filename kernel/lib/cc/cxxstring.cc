#include "cxxstring.h"

// Explicit instantiations
#pragma GCC visibility push(default)
template class std::basic_string<char>;
template class std::basic_string<wchar_t>;
template class std::basic_string<char16_t>;
template class std::basic_string<char32_t>;
#pragma GCC visibility pop
