#pragma once

//#include <boost/any.hpp>
#include <memory>
#include <string>
#include <vector>

//#include "muduo/base/Timestamp.h"
//#include "muduo/net/Buffer.h"
//#include "muduo/net/InetAddress.h"
#include "net/callbacks.h"
#include "EventLoop.h"
#include "UdpClient.h"

using namespace std;
using namespace hv;

//namespace muduo {
//namespace net {
//class Channel;
//class EventLoop;
//class Socket;
//}  // namespace net
//}  // namespace muduo

class UdpConnection : public std::enable_shared_from_this<UdpConnection> {
 public:
  UdpConnection(const std::string& name, const string& peer_addr);
  ~UdpConnection();

  //muduo::net::EventLoop* loop() const { return loop_; }
  EventLoopPtr loop() { return loop_; }
  const std::string& name() const { return name_; }
  //const struct sockaddr* local_address() const { return local_addr_; }
  const string peer_address() const { return peer_addr_; }
  void SetPacketCallback(PacketCallback cb) { callback_ = cb; }
  void Send(const void* data, size_t len);
  void Start();

  UdpClient* GetUdpClient() { return &udpClient_; }
  PacketCallback GetPacketCallback() { return callback_; }
  
 private:
  void HandleRead(struct timeval& receive_time);

 private:
  //muduo::net::EventLoop* loop_;
  EventLoopPtr loop_;
  std::string name_;
  UdpClient udpClient_;
  //std::unique_ptr<muduo::net::Socket> socket_;
  //std::unique_ptr<muduo::net::Channel> channel_;
  //muduo::net::Buffer input_buffer_;
  //boost::any context_;
  const string peer_addr_;
  PacketCallback callback_;
  char* buf_;
  size_t size_;
};

typedef std::shared_ptr<UdpConnection> UdpConnectionPtr;