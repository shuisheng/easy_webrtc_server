#include "net/udp_server.h"
#include "rtc/str_util.h"

#include <memory>


//#include "muduo/base/Logging.h"
//#include "muduo/net/Channel.h"
//#include "muduo/net/EventLoop.h"
//#include "muduo/net/EventLoopThreadPool.h"
//#include "muduo/net/Socket.h"
//#include "muduo/net/SocketsOps.h"

//using namespace muduo;
//using namespace muduo::net;

static const int BUF_SIZE = 4 * 1024;

UdpServerOuter::UdpServerOuter(int port) 
    : port(port) {
  //size_ = BUF_SIZE;
  //buf_ = new char[size_];
  //int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
  //socket_ = std::unique_ptr<Socket>(new Socket(fd));
  //socket_->setReuseAddr(true);
  //socket_->setReusePort(true);
  //socket_->bindAddress(listen_addr);
  //channel_ = std::unique_ptr<Channel>(new Channel(loop, socket_->fd()));
  //channel_->setReadCallback(std::bind(&UdpServer::HandleRead, this, _1));
  //thread_pool_->setThreadNum(num_threads);
  udpSrv_.reset(new hv::UdpServer());
  fd = udpSrv_->createsocket(port);
}

UdpServerOuter::~UdpServerOuter() {
  udpSrv_->stop();
}

static int DissolveUdpSock(int fd) {
  struct sockaddr_storage addr;
  socklen_t addr_len = sizeof(addr);
  if (-1 == getsockname(fd, (struct sockaddr*)&addr, &addr_len)) {
    return -1;
  }
  addr.ss_family = AF_UNSPEC;
  if (-1 == ::connect(fd, (struct sockaddr*)&addr, addr_len)) {
    return -1;
  }
  return 0;
}

void UdpServerOuter::HandleRead(struct timeval& receive_time) {
  //loop_->assertInLoopThread();
  //struct sockaddr_in remote_addr;
  //unsigned int addr_len = sizeof(remote_addr);
  //size_t recvLen =
  //    recvfrom(socket_->fd(), buf_, size_, 0, (struct sockaddr*)&remote_addr, &addr_len);
  //if (callback_) {
  //  callback_(this, (const uint8_t*)buf_, recvLen, InetAddress(remote_addr), receive_time);
  //}
}

void getLocalAddr(int sockfd, struct sockaddr* localAddr) {
  //struct sockaddr_in localaddr;
 // ZeroMemory(&localaddr, sizeof localaddr);
  socklen_t addrlen = static_cast<socklen_t>(sizeof sockaddr);
  ::getsockname(sockfd, localAddr, &addrlen);
}

UdpConnectionPtr UdpServerOuter::GetOrCreatConnection(const string& remote_addr) {
  //std::string key = remote_addr.toIpPort();
  string key = remote_addr;
  if (auto it = connections_.find(key); it != connections_.end()) {
    return it->second;
  }
  //EventLoopPtr loop = udpSrv_->loop();

  //EventLoop* io_loop = thread_pool_->getNextLoop();
  //InetAddress local_addr(sockets::getLocalAddr(socket_->fd()));
  //struct sockaddr local_addr;
  //ZeroMemory(&local_addr, sizeof local_addr);
  //getLocalAddr(udpSrv_->channel->fd(), &local_addr);
  //int fd = ::socket(AF_INET, SOCK_DGRAM, 0);

  //auto ipPort = Split(remote_addr, ":");
  //struct sockaddr_in remote_sockaddr_in = {0};
  //remote_sockaddr_in.sin_family = AF_INET;
  //remote_sockaddr_in.sin_port = htons(atoi(ipPort[0].c_str()));
  //remote_sockaddr_in.sin_addr.s_addr = inet_addr(ipPort[0].c_str()); 
  auto connection = shared_ptr<UdpConnection>(new UdpConnection(key, remote_addr));
  connections_[key] = connection;
  //DissolveUdpSock(udpSrv_->channel->fd());
  return connection;

    //return nullptr;
}

void UdpServerOuter::Start() {
  udpSrv_->start();
  //thread_pool_->start(thread_init_callback_);
  //loop_->runInLoop(std::bind(&Channel::enableReading, channel_.get()));
}
