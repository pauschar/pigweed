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

#include "pw_rpc/channel.h"

#include "gtest/gtest.h"
#include "pw_rpc/internal/packet.h"
#include "pw_rpc_private/test_utils.h"

namespace pw::rpc::internal {
namespace {

using std::byte;

TEST(ChannelOutput, Name) {
  class NameTester : public ChannelOutput {
   public:
    NameTester(const char* name) : ChannelOutput(name) {}
    span<std::byte> AcquireBuffer() override { return {}; }
    void SendAndReleaseBuffer(size_t) override {}
  };

  EXPECT_STREQ("hello_world", NameTester("hello_world").name());
  EXPECT_EQ(nullptr, NameTester(nullptr).name());
}

constexpr Packet kTestPacket(PacketType::RPC, 1, 42, 100);
const size_t kReservedSize = 2 /* type */ + 2 /* channel */ + 2 /* service */ +
                             2 /* method */ + 2 /* payload key */ +
                             2 /* status */;

TEST(Channel, TestPacket_ReservedSizeMatchesMinEncodedSizeBytes) {
  EXPECT_EQ(kReservedSize, kTestPacket.MinEncodedSizeBytes());
}

TEST(Channel, OutputBuffer_EmptyBuffer) {
  TestOutput<0> output;
  internal::Channel channel(100, &output);

  Channel::OutputBuffer buffer = channel.AcquireBuffer();
  EXPECT_TRUE(buffer.payload(kTestPacket).empty());
}

TEST(Channel, OutputBuffer_TooSmall) {
  TestOutput<kReservedSize - 1> output;
  internal::Channel channel(100, &output);

  Channel::OutputBuffer output_buffer = channel.AcquireBuffer();
  EXPECT_TRUE(output_buffer.payload(kTestPacket).empty());

  EXPECT_EQ(Status::INTERNAL, channel.Send(output_buffer, kTestPacket));
}

TEST(Channel, OutputBuffer_ExactFit) {
  TestOutput<kReservedSize> output;
  internal::Channel channel(100, &output);

  Channel::OutputBuffer output_buffer(channel.AcquireBuffer());
  const span payload = output_buffer.payload(kTestPacket);

  EXPECT_EQ(payload.size(), output.buffer().size() - kReservedSize);
  EXPECT_EQ(output.buffer().data() + kReservedSize, payload.data());

  EXPECT_EQ(Status::OK, channel.Send(output_buffer, kTestPacket));
}

TEST(Channel, OutputBuffer_PayloadDoesNotFit_ReportsError) {
  TestOutput<kReservedSize> output;
  internal::Channel channel(100, &output);

  Channel::OutputBuffer output_buffer(channel.AcquireBuffer());

  Packet packet = kTestPacket;
  byte data[1] = {};
  packet.set_payload(data);

  EXPECT_EQ(Status::INTERNAL, channel.Send(output_buffer, packet));
}

TEST(Channel, OutputBuffer_ExtraRoom) {
  TestOutput<kReservedSize * 3> output;
  internal::Channel channel(100, &output);

  Channel::OutputBuffer output_buffer = channel.AcquireBuffer();
  const span payload = output_buffer.payload(kTestPacket);

  EXPECT_EQ(payload.size(), output.buffer().size() - kReservedSize);
  EXPECT_EQ(output.buffer().data() + kReservedSize, payload.data());

  EXPECT_EQ(Status::OK, channel.Send(output_buffer, kTestPacket));
}

}  // namespace
}  // namespace pw::rpc::internal
