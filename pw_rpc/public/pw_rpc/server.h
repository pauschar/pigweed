// Copyright 2020 The Pigweed Authors
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

#include <cstddef>

#include "pw_containers/intrusive_list.h"
#include "pw_rpc/channel.h"
#include "pw_rpc/internal/base_server_writer.h"
#include "pw_rpc/internal/channel.h"
#include "pw_rpc/internal/service.h"

namespace pw::rpc {

class Server {
 public:
  constexpr Server(span<Channel> channels)
      : channels_(static_cast<internal::Channel*>(channels.data()),
                  channels.size()) {}

  // Registers a service with the server. This should not be called directly
  // with an internal::Service; instead, use a generated class which inherits
  // from it.
  void RegisterService(internal::Service& service) {
    services_.push_front(service);
  }

  void ProcessPacket(span<const std::byte> packet, ChannelOutput& interface);

  constexpr size_t channel_count() const { return channels_.size(); }

 private:
  void InvokeMethod(const internal::Packet& request,
                    Channel& channel,
                    internal::Packet& response,
                    span<std::byte> buffer);

  internal::Channel* FindChannel(uint32_t id) const;
  internal::Channel* AssignChannel(uint32_t id, ChannelOutput& interface);

  span<internal::Channel> channels_;
  IntrusiveList<internal::Service> services_;
};

}  // namespace pw::rpc
