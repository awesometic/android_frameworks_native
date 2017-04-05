#ifndef ANDROID_PDX_UDS_SERVICE_ENDPOINT_H_
#define ANDROID_PDX_UDS_SERVICE_ENDPOINT_H_

#include <sys/stat.h>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <pdx/service.h>
#include <pdx/service_endpoint.h>
#include <uds/channel_event_set.h>
#include <uds/service_dispatcher.h>

namespace android {
namespace pdx {
namespace uds {

class Endpoint : public pdx::Endpoint {
 public:
  enum {
    kIpcTag = 0x00736674,  // 'uds'
  };

  // Blocking modes for service endpoint. Controls whether the epoll set is in
  // blocking mode or not for message receive.
  enum {
    kBlocking = true,
    kNonBlocking = false,
    kDefaultBlocking = kNonBlocking,
  };

  enum : mode_t {
    kDefaultMode = 0,
  };

  ~Endpoint() override = default;

  uint32_t GetIpcTag() const override { return kIpcTag; }
  Status<void> SetService(Service* service) override;
  Status<void> SetChannel(int channel_id, Channel* channel) override;
  Status<void> CloseChannel(int channel_id) override;
  Status<void> ModifyChannelEvents(int channel_id, int clear_mask,
                                   int set_mask) override;
  Status<RemoteChannelHandle> PushChannel(Message* message, int flags,
                                          Channel* channel,
                                          int* channel_id) override;
  Status<int> CheckChannel(const Message* message, ChannelReference ref,
                           Channel** channel) override;
  Status<void> MessageReceive(Message* message) override;
  Status<void> MessageReply(Message* message, int return_code) override;
  Status<void> MessageReplyFd(Message* message, unsigned int push_fd) override;
  Status<void> MessageReplyChannelHandle(
      Message* message, const LocalChannelHandle& handle) override;
  Status<void> MessageReplyChannelHandle(
      Message* message, const BorrowedChannelHandle& handle) override;
  Status<void> MessageReplyChannelHandle(
      Message* message, const RemoteChannelHandle& handle) override;
  Status<size_t> ReadMessageData(Message* message, const iovec* vector,
                                 size_t vector_length) override;
  Status<size_t> WriteMessageData(Message* message, const iovec* vector,
                                  size_t vector_length) override;
  Status<FileReference> PushFileHandle(Message* message,
                                       const LocalHandle& handle) override;
  Status<FileReference> PushFileHandle(Message* message,
                                       const BorrowedHandle& handle) override;
  Status<FileReference> PushFileHandle(Message* message,
                                       const RemoteHandle& handle) override;
  Status<ChannelReference> PushChannelHandle(
      Message* message, const LocalChannelHandle& handle) override;
  Status<ChannelReference> PushChannelHandle(
      Message* message, const BorrowedChannelHandle& handle) override;
  Status<ChannelReference> PushChannelHandle(
      Message* message, const RemoteChannelHandle& handle) override;
  LocalHandle GetFileHandle(Message* message, FileReference ref) const override;
  LocalChannelHandle GetChannelHandle(Message* message,
                                      ChannelReference ref) const override;

  void* AllocateMessageState() override;
  void FreeMessageState(void* state) override;

  Status<void> Cancel() override;

  // Open an endpoint at the given path.
  // Second parameter is unused for UDS, but we have it here for compatibility
  // in signature with servicefs::Endpoint::Create().
  // This method uses |endpoint_path| as a relative path to endpoint socket
  // created by init process.
  static std::unique_ptr<Endpoint> Create(const std::string& endpoint_path,
                                          mode_t /*unused_mode*/ = kDefaultMode,
                                          bool blocking = kDefaultBlocking);

  // Helper method to create an endpoint at the given UDS socket path. This
  // method physically creates and binds a socket at that path.
  static std::unique_ptr<Endpoint> CreateAndBindSocket(
      const std::string& endpoint_path, bool blocking = kDefaultBlocking);

  int epoll_fd() const { return epoll_fd_.Get(); }

 private:
  struct ChannelData {
    LocalHandle data_fd;
    ChannelEventSet event_set;
    Channel* channel_state{nullptr};
  };

  // This class must be instantiated using Create() static methods above.
  Endpoint(const std::string& endpoint_path, bool blocking,
           bool use_init_socket_fd = true);

  Endpoint(const Endpoint&) = delete;
  void operator=(const Endpoint&) = delete;

  uint32_t GetNextAvailableMessageId() {
    return next_message_id_.fetch_add(1, std::memory_order_relaxed);
  }

  void BuildCloseMessage(int channel_id, Message* message);

  Status<void> AcceptConnection(Message* message);
  Status<void> ReceiveMessageForChannel(int channel_id, Message* message);
  Status<void> OnNewChannel(LocalHandle channel_fd);
  Status<ChannelData*> OnNewChannelLocked(LocalHandle channel_fd,
                                          Channel* channel_state);
  Status<void> CloseChannelLocked(int channel_id);
  Status<void> ReenableEpollEvent(int fd);
  Channel* GetChannelState(int channel_id);
  int GetChannelSocketFd(int channel_id);
  int GetChannelEventFd(int channel_id);

  std::string endpoint_path_;
  bool is_blocking_;
  LocalHandle socket_fd_;
  LocalHandle cancel_event_fd_;
  LocalHandle epoll_fd_;

  mutable std::mutex channel_mutex_;
  std::map<int, ChannelData> channels_;

  Service* service_{nullptr};
  std::atomic<uint32_t> next_message_id_;
};

}  // namespace uds
}  // namespace pdx
}  // namespace android

#endif  // ANDROID_PDX_UDS_PDX_SERVICE_ENDPOINT_H_
