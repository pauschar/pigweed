// Copyright 2021 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.
#pragma once

#include <cstdint>
#include <cstring>
#include <span>
#include <type_traits>
#include <utility>

#include "pw_assert/light.h"
#include "pw_checksum/crc16_ccitt.h"

namespace pw::persistent_ram {

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuninitialized"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

// A simple container for holding a value T with CRC16 integrity checking.
//
// A Persistent is simply a value T plus integrity checking for use in a
// persistent RAM section which is not initialized on boot.
//
// WARNING: Unlike a DoubleBufferedPersistent, a Persistent will be lost if a
// write/set operation is interrupted or otherwise not completed.
//
// TODO(pwbug/348): Consider a different integrity check implementation which
// does not use a 512B lookup table.
template <typename T>
class Persistent {
 public:
  // Constructor which does nothing, meaning it never sets the value.
  constexpr Persistent() {}

  Persistent(const Persistent&) = delete;  // Copy constructor is disabled.
  Persistent(Persistent&&) = delete;       // Move constructor is disabled.
  ~Persistent() {}                         // The destructor does nothing.

  // Construct the value in-place.
  template <class... Args>
  const T& emplace(Args&&... args) {
    new (const_cast<T*>(&contents_)) T(std::forward<Args>(args)...);
    crc_ = Crc(contents_);
    return const_cast<T&>(contents_);
  }

  // Assignment operator.
  template <typename U = T>
  Persistent& operator=(U&& value) {
    contents_ = std::move(value);
    crc_ = Crc(contents_);
    return *this;
  }

  // Destroys any contained value.
  void reset() {
    // The trivial destructor is skipped as it's trivial.
    std::memset(const_cast<T*>(&contents_), 0, sizeof(contents_));
    crc_ = 0;
  }

  // Returns true if a value is held by the Persistent.
  bool has_value() const {
    return crc_ == Crc(contents_);  // There's a value if its CRC matches.
  }

  // Access the value.
  //
  // Precondition: has_value() must be true.
  const T& value() const {
    PW_ASSERT(has_value());
    return const_cast<T&>(contents_);
  }

 private:
  static_assert(std::is_trivially_copy_constructible<T>::value,
                "If a Persistent persists across reboots, it is effectively "
                "loaded through a trivial copy constructor.");
  static_assert(std::is_trivially_destructible<T>::value,
                "A Persistent's destructor does not invoke the value's "
                "destructor, ergo only trivially destructible types are "
                "supported.");

  static uint16_t Crc(volatile const T& value) {
    return checksum::Crc16Ccitt::Calculate(
        std::as_bytes(std::span(const_cast<const T*>(&value), 1)));
  }

  // Use unions to denote that these members are never initialized by design and
  // on purpose. Volatile is used to ensure that the compiler cannot optimize
  // out operations where it seems like there is no further usage of a
  // Persistent as this may be on the next boot.
  union {
    volatile T contents_;
  };
  union {
    volatile uint16_t crc_;
  };
};

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

}  // namespace pw::persistent_ram
