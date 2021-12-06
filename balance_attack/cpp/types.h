//
// Created by yangzhe on 9/14/20.
//

#ifndef CPP_TYPES_H
#define CPP_TYPES_H

#include <cstdint>
#include <functional>

typedef int32_t block_id_t;
typedef int32_t time_ms_t;
typedef int32_t node_id_t;

// Utility class for enforcing explicit passing-by-ref.
template <class T>
class Ref {
  std::reference_wrapper<T> r;

 public:
  Ref(std::reference_wrapper<T> r) : r(r) {}
  Ref(T &&x) = delete;

  constexpr T &get() const { return this->r.get(); }

  constexpr operator std::reference_wrapper<T>() const { return this->r; }
};

#endif  // CPP_TYPES_H
