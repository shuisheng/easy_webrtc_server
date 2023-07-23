#include "net/udp_connection.h"
#include "rtc/str_util.h"
#include "common/utils.h"

//#include "muduo/base/Logging.h"
//#include "muduo/net/Channel.h"
//#include "muduo/net/EventLoop.h"
//#include "muduo/net/EventLoopThreadPool.h"
//#include "muduo/net/Socket.h"
//#include "muduo/net/SocketsOps.h"

//using namespace muduo;
//using namespace muduo::net;

static const int BUF_SIZE = 4 * 1024;

void UdpConnection::HandleRead(struct timeval& receive_time) {
  //loop_->assertInLoopThread();
  //struct sockaddr_in remote_addr;
  //unsigned int addr_len = sizeof(remote_addr);
  //size_t recvLen =
  //    recvfrom(socket_->fd(), buf_, size_, 0, (struct sockaddr*)&remote_addr, &addr_len);
  //if (callback_) {
  //  callback_(shared_from_this(), (const uint8_t*)buf_, recvLen, InetAddress(remote_addr),
  //            receive_time);
  //}
}

UdpConnection::UdpConnection(const std::string& name, const string& peer_addr)
    : name_(name),
      peer_addr_(peer_addr)
// socket_(new Socket(sockfd)),
// channel_(new Channel(loop, sockfd))
{
  size_ = BUF_SIZE;
  buf_ = new char[size_];
  // socket_->setReuseAddr(true);
  // socket_->setReusePort(true);
  // socket_->bindAddress(local_addr);
  // channel_->setReadCallback(std::bind(&UdpConnection::HandleRead, this, _1));
  // sockets::connect(sockfd, peer_addr.getSockAddr());
  auto ipPort = Split(peer_addr, ":");
  const char* remoteIp = ipPort[0].c_str();
  int port = atoi(ipPort[1].c_str());
  int sockfd = udpClient_.createsocket(port, remoteIp);
  assert(sockfd != -1);
  struct sockaddr_in myname;
  /* Bind to a specific interface in the Internet domain */
  /* make sure the sin_zero field is cleared */
  memset(&myname, 0, sizeof(myname));
  myname.sin_family = AF_INET;
  myname.sin_addr.s_addr = inet_addr("127.0.0.1");
  /* specific interface */
  myname.sin_port = htons(10000);
  bool reuse = true;
  //int ret = ::setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, (const char*)&reuse, sizeof(reuse));
  int ret = ::setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));
  ret = ::bind(sockfd, (struct sockaddr*)&myname, sizeof(myname));
  //udpClient_.onMessage = [this](const SocketChannelPtr& channel, Buffer* buf) {
  //  printf("< %.*s\n", (int)buf->size(), (char*)buf->data());
  //  if (callback_) {
  //    struct timeval receive_time;
  //    Utils::Time::gettimeofday(&receive_time, NULL);
  //    callback_(shared_from_this(), (const uint8_t*)buf->data(), buf->size(),
  //              channel.get()->peeraddr(), receive_time);
  //  }
  //};
  loop_ = udpClient_.loop();
  //udpClient_.start();
}

UdpConnection::~UdpConnection() {}

void UdpConnection::Start() {
  //loop_->runInLoop(std::bind(&Channel::enableReading, channel_.get()));
  udpClient_.start();
}

void UdpConnection::Send(const void* data, size_t len) {
  //loop_->assertInLoopThread();
  //::write(socket_->fd(), data, len);
  udpClient_.sendto(data, len);
}
