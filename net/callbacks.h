#pragma once

#include <functional>
#include <memory>
#include <string>

using namespace std;
//#include "muduo/base/Timestamp.h"
//#include "muduo/net/InetAddress.h"

class UdpConnection;
class UdpServerOuter;

typedef std::function<void(UdpServerOuter*, const uint8_t* buf, size_t len,
                           const string& peer_addr, struct timeval& time)>
    ServerPacketCallback;
typedef std::function<void(const std::shared_ptr<UdpConnection>&, const uint8_t* buf, size_t len,
                           const string& peer_addr, struct timeval& time)>
    PacketCallback;