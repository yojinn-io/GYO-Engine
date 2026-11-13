#pragma once


// TypeKey: the in-process lookup key of a C++ type (target/kernel DI-KS 4: only
// for in-process lookup; never an id, never on the wire or in a digest; ids
// come from registration order). The key is the address of one object per
// type. The object is deliberately not const: a linker that folds identical
// read-only data (MSVC /OPT:ICF, lld --icf=all) could otherwise give two types
// the same address (spec B.6 risk 5). A collision is still detected at
// registration as R-E5 (Registry.cpp).

namespace GYO::Kernel {

struct TypeKey {
    const void* address = nullptr;

    friend constexpr bool operator==(const TypeKey&, const TypeKey&) = default;
};

namespace Detail {

// The only object behind a TypeKey. tests/common/kernel/kernel_guard.py (K4)
// and kernel_symbols.py allow exactly this static variable and tlCurrent.
template <class T>
struct TypeKeyOf {
    static inline char typeKeyAnchor = 0;
};

} // namespace Detail

template <class T>
[[nodiscard]] constexpr TypeKey KeyOf() noexcept {
    return TypeKey{&Detail::TypeKeyOf<T>::typeKeyAnchor};
}

} // namespace GYO::Kernel
