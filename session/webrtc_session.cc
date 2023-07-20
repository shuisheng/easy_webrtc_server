#include "session/webrtc_session.h"
#include "rtc/str_util.h"
#include "common/utils.h"

#include <iostream>

//using namespace muduo;
//using namespace muduo::net;

std::vector<std::string> Split(const string& s, const char* delim) {
  std::vector<std::string> ret;
  size_t last = 0;
  auto index = s.find(delim, last);
  while (index != string::npos) {
    if (index - last >= 0) {
      ret.push_back(s.substr(last, index - last));
    }
    last = index + strlen(delim);
    index = s.find(delim, last);
  }
  if (!s.size() || s.size() - last >= 0) {
    ret.push_back(s.substr(last));
  }
  return ret;
}

void WebRTCSession::SetNetworkTransport(const std::shared_ptr<NetworkTransport>& transport) {
  network_transport_ = transport;
  webrtc_transport_->SetNetworkTransport(transport);
  auto connection = transport->connection();
  connection->SetPacketCallback([this](const std::shared_ptr<UdpConnection>& con,
                                       const uint8_t* buf, size_t len,
                                       const string& peer_addr, struct timeval&) {
    //struct sockaddr_in remote_sockaddr_in = *(struct sockaddr_in*)peer_addr;
    auto ipPort = Split(peer_addr, ":");
    struct sockaddr_in remote_sockaddr_in = {0};
    remote_sockaddr_in.sin_family = AF_INET;
    remote_sockaddr_in.sin_port = htons(atoi(ipPort[0].c_str()));
    remote_sockaddr_in.sin_addr.s_addr = inet_addr(ipPort[0].c_str()); 
    webrtc_transport_->OnInputDataPacket(buf, len, remote_sockaddr_in);
  });
  connection->GetUdpClient()->onMessage = [connection](const SocketChannelPtr& channel, Buffer* buf) {
        PacketCallback cb = connection->GetPacketCallback();
        if (cb) {
          struct timeval receive_time;
          Utils::Time::gettimeofday(&receive_time, NULL);
          cb(connection, (const uint8_t*)buf->data(), buf->size(), channel->peeraddr(), receive_time);
        }
  };
  connection->Start();
  SetLoop(connection->loop());
  is_ready_.store(true);
}

std::shared_ptr<WebRTCSession> WebRTCSessionFactory::CreateWebRTCSession(const std::string& ip,
                                                                         uint16_t port) {
  std::shared_ptr<WebRtcTransport> rtc_transport(new WebRtcTransport(ip, port));
  std::shared_ptr<WebRTCSession> rtc_session(new WebRTCSession(rtc_transport));
  {
    std::lock_guard<std::mutex> guard(mutex_);
    rtc_sessions.insert(std::make_pair(rtc_transport->GetidentifyID(), rtc_session));
  }
  return rtc_session;
}

std::shared_ptr<WebRTCSession> WebRTCSessionFactory::GetWebRTCSession(const std::string& key) {
  std::shared_ptr<WebRTCSession> ptr = nullptr;
  {
    std::lock_guard<std::mutex> guard(mutex_);
    auto it = rtc_sessions.find(key);
    if (it == rtc_sessions.end()) {
      ptr = nullptr;
      return ptr;
    }
    ptr = it->second;
  }
  return ptr;
}

void WebRTCSessionFactory::GetAllReadyWebRTCSession(
    std::vector<std::shared_ptr<WebRTCSession>>* sessions) {
  {
    std::lock_guard<std::mutex> guard(mutex_);
    for (const auto& session : rtc_sessions) {
      if (session.second->is_ready().load()) {
        sessions->push_back(session.second);
      }
    }
  }
  return;
}

void WebRTCSessionFactory::HandlePacket(WebRTCSessionFactory* factory,
                                        UdpServerOuter* server,
                                        const uint8_t* buf, size_t len,
                                        const string& peer_addr,
                                        struct timeval& timestamp) {
  if (!RTC::StunPacket::IsStun(buf, len)) {
    std::cout << "receive not stun packet" << std::endl;
    return;
  }
  RTC::StunPacket* packet = RTC::StunPacket::Parse(buf, len);
  if (packet == nullptr) {
    std::cout << "parse stun error" << std::endl;
    return;
  }
  auto vec = Split(packet->GetUsername(), ":");
  std::string use_name = vec[0];
  auto session = factory->GetWebRTCSession(use_name);
  if (!session) {
    std::cout << "no rtc session" << std::endl;
    return;
  }
  auto connection = server->GetOrCreatConnection(peer_addr);
  if (!connection) {
    std::cout << "get connection error" << std::endl;
    return;
  }
  auto transport = std::shared_ptr<NetworkTransport>(new NetworkTransport(connection));
  session->SetNetworkTransport(transport);
  //struct sockaddr_in remote_sockaddr_in = *(struct sockaddr_in*)peer_addr;
  auto ipPort = Split(peer_addr, ":");
  struct sockaddr_in remote_sockaddr_in = {0};
  remote_sockaddr_in.sin_family = AF_INET;
  remote_sockaddr_in.sin_port = htons(atoi(ipPort[0].c_str()));
  remote_sockaddr_in.sin_addr.s_addr = inet_addr(ipPort[0].c_str()); 
  std::shared_ptr<uint8_t> shared_buf(new uint8_t[len]);
  memcpy(shared_buf.get(), buf, len);
  //connection->GetUdpClient()->onMessage = [](const SocketChannelPtr& channel,
  //                                           Buffer* buf) {};
  session->loop()->runInLoop([session, shared_buf, len, remote_sockaddr_in]() {
    session->webrtc_transport()->OnInputDataPacket(shared_buf.get(), len, remote_sockaddr_in);
  });

}