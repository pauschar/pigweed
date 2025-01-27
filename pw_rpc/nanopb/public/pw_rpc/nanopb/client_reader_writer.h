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

// This file defines the ServerReaderWriter, ServerReader, and ServerWriter
// classes for the Nanopb RPC interface. These classes are used for
// bidirectional, client, and server streaming RPCs.
#pragma once

#include "pw_rpc/channel.h"
#include "pw_rpc/internal/client_call.h"
#include "pw_rpc/internal/lock.h"
#include "pw_rpc/nanopb/internal/common.h"

namespace pw::rpc {
namespace internal {

// Base class for unary and client streaming calls.
template <typename Response>
class NanopbUnaryResponseClientCall : public UnaryResponseClientCall {
 public:
  template <typename CallType, typename... Request>
  static CallType Start(Endpoint& client,
                        uint32_t channel_id,
                        uint32_t service_id,
                        uint32_t method_id,
                        const NanopbMethodSerde& serde,
                        Function<void(const Response&, Status)>&& on_completed,
                        Function<void(Status)>&& on_error,
                        const Request&... request)
      PW_LOCKS_EXCLUDED(rpc_lock()) {
    rpc_lock().lock();
    CallType call(
        client.ClaimLocked(), channel_id, service_id, method_id, serde);

    call.set_nanopb_on_completed_locked(std::move(on_completed));
    call.set_on_error_locked(std::move(on_error));

    if constexpr (sizeof...(Request) == 0u) {
      call.SendInitialClientRequest({});
    } else {
      NanopbSendInitialRequest(call, serde.request(), &request...);
    }
    client.CleanUpCalls();
    return call;
  }

  ~NanopbUnaryResponseClientCall() { DestroyClientCall(); }

 protected:
  constexpr NanopbUnaryResponseClientCall() : serde_(nullptr) {}

  NanopbUnaryResponseClientCall(LockedEndpoint& client,
                                uint32_t channel_id,
                                uint32_t service_id,
                                uint32_t method_id,
                                MethodType type,
                                const NanopbMethodSerde& serde)
      PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock())
      : UnaryResponseClientCall(
            client, channel_id, service_id, method_id, StructCallProps(type)),
        serde_(&serde) {}

  NanopbUnaryResponseClientCall(NanopbUnaryResponseClientCall&& other)
      PW_LOCKS_EXCLUDED(rpc_lock()) {
    *this = std::move(other);
  }

  NanopbUnaryResponseClientCall& operator=(
      NanopbUnaryResponseClientCall&& other) PW_LOCKS_EXCLUDED(rpc_lock()) {
    RpcLockGuard lock;
    MoveUnaryResponseClientCallFrom(other);
    serde_ = other.serde_;
    set_nanopb_on_completed_locked(std::move(other.nanopb_on_completed_));
    return *this;
  }

  void set_on_completed(
      Function<void(const Response& response, Status)>&& on_completed)
      PW_LOCKS_EXCLUDED(rpc_lock()) {
    RpcLockGuard lock;
    set_nanopb_on_completed_locked(std::move(on_completed));
  }

  Status SendClientStream(const void* payload) PW_LOCKS_EXCLUDED(rpc_lock()) {
    RpcLockGuard lock;
    return NanopbSendStream(*this, payload, serde_);
  }

 private:
  void set_nanopb_on_completed_locked(
      Function<void(const Response& response, Status)>&& on_completed)
      PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock()) {
    nanopb_on_completed_ = std::move(on_completed);

    UnaryResponseClientCall::set_on_completed_locked(
        [this](ConstByteSpan payload, Status status)
            PW_NO_LOCK_SAFETY_ANALYSIS {
              DecodeToStructAndInvokeOnCompleted(
                  payload, serde_->response(), nanopb_on_completed_, status);
            });
  }

  const NanopbMethodSerde* serde_ PW_GUARDED_BY(rpc_lock());
  Function<void(const Response&, Status)> nanopb_on_completed_ PW_GUARDED_BY(
      rpc_lock());
};

// Base class for server and bidirectional streaming calls.
template <typename Response>
class NanopbStreamResponseClientCall : public StreamResponseClientCall {
 public:
  template <typename CallType, typename... Request>
  static CallType Start(Endpoint& client,
                        uint32_t channel_id,
                        uint32_t service_id,
                        uint32_t method_id,
                        const NanopbMethodSerde& serde,
                        Function<void(const Response&)>&& on_next,
                        Function<void(Status)>&& on_completed,
                        Function<void(Status)>&& on_error,
                        const Request&... request)
      PW_LOCKS_EXCLUDED(rpc_lock()) {
    rpc_lock().lock();
    CallType call(
        client.ClaimLocked(), channel_id, service_id, method_id, serde);

    call.set_nanopb_on_next_locked(std::move(on_next));
    call.set_on_completed_locked(std::move(on_completed));
    call.set_on_error_locked(std::move(on_error));

    if constexpr (sizeof...(Request) == 0u) {
      call.SendInitialClientRequest({});
    } else {
      NanopbSendInitialRequest(call, serde.request(), &request...);
    }
    client.CleanUpCalls();
    return call;
  }

  ~NanopbStreamResponseClientCall() { DestroyClientCall(); }

 protected:
  constexpr NanopbStreamResponseClientCall() : serde_(nullptr) {}

  NanopbStreamResponseClientCall(NanopbStreamResponseClientCall&& other)
      PW_LOCKS_EXCLUDED(rpc_lock()) {
    *this = std::move(other);
  }

  NanopbStreamResponseClientCall& operator=(
      NanopbStreamResponseClientCall&& other) PW_LOCKS_EXCLUDED(rpc_lock()) {
    RpcLockGuard lock;
    MoveStreamResponseClientCallFrom(other);
    serde_ = other.serde_;
    set_nanopb_on_next_locked(std::move(other.nanopb_on_next_));
    return *this;
  }

  NanopbStreamResponseClientCall(LockedEndpoint& client,
                                 uint32_t channel_id,
                                 uint32_t service_id,
                                 uint32_t method_id,
                                 MethodType type,
                                 const NanopbMethodSerde& serde)
      PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock())
      : StreamResponseClientCall(
            client, channel_id, service_id, method_id, StructCallProps(type)),
        serde_(&serde) {}

  Status SendClientStream(const void* payload) PW_LOCKS_EXCLUDED(rpc_lock()) {
    RpcLockGuard lock;
    return NanopbSendStream(*this, payload, serde_);
  }

  void set_on_next(Function<void(const Response& response)>&& on_next)
      PW_LOCKS_EXCLUDED(rpc_lock()) {
    RpcLockGuard lock;
    set_nanopb_on_next_locked(std::move(on_next));
  }

 private:
  void set_nanopb_on_next_locked(
      Function<void(const Response& response)>&& on_next)
      PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock()) {
    nanopb_on_next_ = std::move(on_next);

    Call::set_on_next_locked(
        [this](ConstByteSpan payload) PW_NO_LOCK_SAFETY_ANALYSIS {
          DecodeToStructAndInvokeOnNext(
              payload, serde_->response(), nanopb_on_next_);
        });
  }

  const NanopbMethodSerde* serde_ PW_GUARDED_BY(rpc_lock());
  Function<void(const Response&)> nanopb_on_next_ PW_GUARDED_BY(rpc_lock());
};

}  // namespace internal

// The NanopbClientReaderWriter is used to send and receive messages in a
// bidirectional streaming RPC.
template <typename Request, typename ResponseType>
class NanopbClientReaderWriter
    : private internal::NanopbStreamResponseClientCall<ResponseType> {
 public:
  using Response = ResponseType;

  constexpr NanopbClientReaderWriter() = default;

  NanopbClientReaderWriter(NanopbClientReaderWriter&&) = default;
  NanopbClientReaderWriter& operator=(NanopbClientReaderWriter&&) = default;

  using internal::Call::active;
  using internal::Call::channel_id;

  using internal::ClientCall::id;

  // Writes a response struct. Returns the following Status codes:
  //
  //   OK - the response was successfully sent
  //   FAILED_PRECONDITION - the writer is closed
  //   INTERNAL - pw_rpc was unable to encode the Nanopb protobuf
  //   other errors - the ChannelOutput failed to send the packet; the error
  //       codes are determined by the ChannelOutput implementation
  //
  Status Write(const Request& request) {
    return internal::NanopbStreamResponseClientCall<Response>::SendClientStream(
        &request);
  }

  // Notifies the server that the client has requested to stop communication by
  // sending CLIENT_REQUEST_COMPLETION.
  using internal::ClientCall::RequestCompletion;

  // Cancels this RPC. Closes the call locally and sends a CANCELLED error to
  // the server.
  using internal::Call::Cancel;

  // Closes this RPC locally. Sends a CLIENT_REQUEST_COMPLETION, but no
  // cancellation packet. Future packets for this RPC are dropped, and the
  // client sends a FAILED_PRECONDITION error in response because the call is
  // not active.
  using internal::ClientCall::Abandon;

  // Functions for setting RPC event callbacks.
  using internal::Call::set_on_error;
  using internal::StreamResponseClientCall::set_on_completed;
  using internal::NanopbStreamResponseClientCall<Response>::set_on_next;

 private:
  friend class internal::NanopbStreamResponseClientCall<Response>;

  NanopbClientReaderWriter(internal::LockedEndpoint& client,
                           uint32_t channel_id_value,
                           uint32_t service_id,
                           uint32_t method_id,
                           const internal::NanopbMethodSerde& serde)
      PW_EXCLUSIVE_LOCKS_REQUIRED(internal::rpc_lock())
      : internal::NanopbStreamResponseClientCall<Response>(
            client,
            channel_id_value,
            service_id,
            method_id,
            MethodType::kBidirectionalStreaming,
            serde) {}
};

// The NanopbClientReader is used to receive messages and send a response in a
// client streaming RPC.
template <typename ResponseType>
class NanopbClientReader
    : private internal::NanopbStreamResponseClientCall<ResponseType> {
 public:
  using Response = ResponseType;

  constexpr NanopbClientReader() = default;

  NanopbClientReader(NanopbClientReader&&) = default;
  NanopbClientReader& operator=(NanopbClientReader&&) = default;

  using internal::Call::active;
  using internal::Call::channel_id;

  using internal::ClientCall::id;

  // Functions for setting RPC event callbacks.
  using internal::NanopbStreamResponseClientCall<Response>::set_on_next;
  using internal::Call::set_on_error;
  using internal::StreamResponseClientCall::set_on_completed;

  using internal::Call::Cancel;
  using internal::Call::RequestCompletion;
  using internal::ClientCall::Abandon;

 private:
  friend class internal::NanopbStreamResponseClientCall<Response>;

  NanopbClientReader(internal::LockedEndpoint& client,
                     uint32_t channel_id_value,
                     uint32_t service_id,
                     uint32_t method_id,
                     const internal::NanopbMethodSerde& serde)
      PW_EXCLUSIVE_LOCKS_REQUIRED(internal::rpc_lock())
      : internal::NanopbStreamResponseClientCall<Response>(
            client,
            channel_id_value,
            service_id,
            method_id,
            MethodType::kServerStreaming,
            serde) {}
};

// The NanopbClientWriter is used to send responses in a server streaming RPC.
template <typename Request, typename ResponseType>
class NanopbClientWriter
    : private internal::NanopbUnaryResponseClientCall<ResponseType> {
 public:
  using Response = ResponseType;

  constexpr NanopbClientWriter() = default;

  NanopbClientWriter(NanopbClientWriter&&) = default;
  NanopbClientWriter& operator=(NanopbClientWriter&&) = default;

  using internal::Call::active;
  using internal::Call::channel_id;

  using internal::ClientCall::id;

  // Functions for setting RPC event callbacks.
  using internal::NanopbUnaryResponseClientCall<Response>::set_on_completed;
  using internal::Call::set_on_error;

  Status Write(const Request& request) {
    return internal::NanopbUnaryResponseClientCall<Response>::SendClientStream(
        &request);
  }

  using internal::Call::Cancel;
  using internal::Call::RequestCompletion;
  using internal::ClientCall::Abandon;

 private:
  friend class internal::NanopbUnaryResponseClientCall<Response>;

  NanopbClientWriter(internal::LockedEndpoint& client,
                     uint32_t channel_id_value,
                     uint32_t service_id,
                     uint32_t method_id,
                     const internal::NanopbMethodSerde& serde)
      PW_EXCLUSIVE_LOCKS_REQUIRED(internal::rpc_lock())
      : internal::NanopbUnaryResponseClientCall<Response>(
            client,
            channel_id_value,
            service_id,
            method_id,
            MethodType::kClientStreaming,
            serde) {}
};

// The NanopbUnaryReceiver is used to receive a response in a unary RPC.
template <typename ResponseType>
class NanopbUnaryReceiver
    : private internal::NanopbUnaryResponseClientCall<ResponseType> {
 public:
  using Response = ResponseType;

  constexpr NanopbUnaryReceiver() = default;

  NanopbUnaryReceiver(NanopbUnaryReceiver&&) = default;
  NanopbUnaryReceiver& operator=(NanopbUnaryReceiver&&) = default;

  using internal::Call::active;
  using internal::Call::channel_id;

  using internal::ClientCall::id;

  // Functions for setting RPC event callbacks.
  using internal::NanopbUnaryResponseClientCall<Response>::set_on_completed;
  using internal::Call::set_on_error;

  using internal::Call::Cancel;
  using internal::ClientCall::Abandon;

 private:
  friend class internal::NanopbUnaryResponseClientCall<Response>;

  NanopbUnaryReceiver(internal::LockedEndpoint& client,
                      uint32_t channel_id_value,
                      uint32_t service_id,
                      uint32_t method_id,
                      const internal::NanopbMethodSerde& serde)
      PW_EXCLUSIVE_LOCKS_REQUIRED(internal::rpc_lock())
      : internal::NanopbUnaryResponseClientCall<Response>(client,
                                                          channel_id_value,
                                                          service_id,
                                                          method_id,
                                                          MethodType::kUnary,
                                                          serde) {}
};

}  // namespace pw::rpc
