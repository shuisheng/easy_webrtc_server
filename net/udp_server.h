#pragma once

//#include <boost/any.hpp>
#include <map>
#include <memory>
#include <string>

#include "UdpServer.h"
//#include "muduo/base/Timestamp.h"
//#include "muduo/net/Buffer.h"
//#include "muduo/net/InetAddress.h"
#include "net/callbacks.h"
#include "net/udp_connection.h"

//namespace muduo {
//namespace net {
//class Channel;
//class EventLoop;
//class Socket;
//class EventLoopThreadPool;
//}  // namespace net
//}  // namespace muduo
using namespace std;
using namespace hv;

class UdpServerOuter {
 public:
  //typedef std::function<void(muduo::net::EventLoop*)> ThreadInitCallback;

  UdpServerOuter(int port);
  ~UdpServerOuter();
  void SetPacketCallback(ServerPacketCallback cb) { callback_ = cb; }

  ServerPacketCallback GetPacketCallback() { return callback_; }
 // void SetThreadInitCallback(const ThreadInitCallback& cb) { thread_init_callback_ = cb; }
  UdpConnectionPtr GetOrCreatConnection(const string& remote_addr);
  void Start();
  
  std::unique_ptr<UdpServer> udpSrv_;
 private:
  void HandleRead(struct timeval& receiveTime);

 private:
     
  typedef std::map<std::string, UdpConnectionPtr> ConnectionMap;
  int port = 0;
  int fd = -1;

  //muduo::net::EventLoop* loop_;
  //std::shared_ptr<muduo::net::EventLoopThreadPool> thread_pool_;
  ConnectionMap connections_;
  //std::unique_ptr<muduo::net::Socket> socket_;
  //std::unique_ptr<muduo::net::Channel> channel_;
  char* buf_;
  size_t size_;
  //ThreadInitCallback thread_init_callback_;
  ServerPacketCallback callback_;
};