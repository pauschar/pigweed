// Copyright 2023 The Pigweed Authors
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

#include "pw_function/function.h"
#include "pw_transfer/internal/context.h"

namespace pw::transfer::internal {

class ClientContext final : public Context {
 public:
  constexpr ClientContext() : handle_id_(0), on_completion_(nullptr) {}

  void set_on_completion(Function<void(Status)>&& on_completion) {
    on_completion_ = std::move(on_completion);
  }

  constexpr uint32_t handle_id() const { return handle_id_; }
  constexpr void set_handle_id(uint32_t handle_id) { handle_id_ = handle_id; }

 private:
  Status FinalCleanup(Status status) override;

  // Transfer clients assign a unique handle_id to all active transfer sessions.
  // Unlike session or transfer IDs, this value is local to the client, not
  // requiring any coordination with the transfer server, allowing users of the
  // client to manage their ongoing transfers.
  uint32_t handle_id_;

  Function<void(Status)> on_completion_;
};

}  // namespace pw::transfer::internal
