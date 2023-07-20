#include "webrtc_transport.h"

#include <iostream>

#include "rtp/rtp_util.h"
#include "ice_server.h"

WebRtcTransport::WebRtcTransport(std::string ip, uint16_t port)
    : is_ready_(false), ip_(ip), port_(port) {
  dtls_transport_.reset(new RTC::DtlsTransport(this));
  ice_server_.reset(new RTC::IceServer(this, Utils::Crypto::GetRandomString(4),
                                       Utils::Crypto::GetRandomString(24)));
}

WebRtcTransport::~WebRtcTransport() {}

void WebRtcTransport::Start() {}

std::string WebRtcTransport::GetLocalSdp() {
  char szsdp[1024 * 10] = {0};
  int nssrc = 12345678;
  sprintf(szsdp,
          "v=0\r\no=- 1495799811084970 1495799811084970 IN IP4 "
          "%s\r\ns=Streaming Test\r\nt=0 0\r\n"
          "a=group:BUNDLE 0\r\na=msid-semantic: WMS janus\r\n"
          "m=video 1 RTP/SAVPF 96\r\nc=IN IP4 "
          "%s\r\na=mid:0\r\na=sendonly\r\na=rtcp-mux\r\n"
          "a=ice-ufrag:%s\r\n"
          "a=ice-pwd:%s\r\na=ice-options:trickle\r\na=fingerprint:sha-256 "
          "%s\r\na=setup:actpass\r\na=connection:new\r\n"
          "a=rtpmap:96 H264/90000\r\n"
          "a=ssrc:%d cname:janusvideo\r\n"
          "a=ssrc:%d msid:janus janusv0\r\n"
          "a=ssrc:%d mslabel:janus\r\n"
          "a=ssrc:%d label:janusv0\r\n"
          "a=candidate:%s 1 udp %u %s %d typ %s\r\n",
          ip_.c_str(), ip_.c_str(), ice_server_->GetUsernameFragment().c_str(),
          ice_server_->GetPassword().c_str(), GetSHA256Fingerprint().c_str(),
          nssrc, nssrc, nssrc, nssrc, "4", 12345678, ip_.c_str(), port_,
          "host");
  return std::string(szsdp);
}

std::string WebRtcTransport::GetPublishSdp() {
  char szsdp[1024 * 10] = {0};
  int nssrc = 12345678;
  sprintf(szsdp,
          "v=0\r\n"
          "o=- 1495799811084970 1495799811084970 IN IP4 %s\r\n"
          "s=Streaming Test\r\n"
          "t=0 0\r\n"
          "a=group:BUNDLE 0\r\n"
          "a=msid-semantic: WMS janus\r\n"
          "m=video 1 RTP/SAVPF 96\r\n"
          "c=IN IP4 %s\r\n"
          "a=mid:0\r\n"
          "a=recvonly\r\n"
          "a=rtcp-mux\r\n"
          "a=ice-ufrag:%s\r\n"
          "a=ice-pwd:%s\r\n"
          "a=ice-options:trickle\r\n"
          "a=fingerprint:sha-256 %s\r\n"
          "a=setup:actpass\r\n"
          "a=connection:new\r\n"
          "a=rtpmap:96 H264/90000\r\n"
          "a=candidate:%s 1 udp %u %s %d typ %s\r\n",
          ip_.c_str(), ip_.c_str(), ice_server_->GetUsernameFragment().c_str(),
          ice_server_->GetPassword().c_str(), GetSHA256Fingerprint().c_str(),
          "4", 12345678, ip_.c_str(), port_, "host");
  return std::string(szsdp);
}

void WebRtcTransport::OnInputDataPacket(
    const uint8_t* buf,
    size_t len,
    const struct sockaddr_in& remote_address) {
  if (RTC::StunPacket::IsStun(buf, len)) {
    RTC::StunPacket* packet = RTC::StunPacket::Parse(buf, len);
    if (packet == nullptr) {
      std::cout << "parse stun error" << std::endl;
      return;
    }
    RTC::TransportTuple tt = {0};
    memcpy(&tt, &remote_address, sizeof(tt));
    ice_server_->ProcessStunPacket(packet, &tt);
    return;
  }
  if (RTC::DtlsTransport::IsDtls(buf, len)) {
    dtls_transport_->ProcessDtlsData(buf, len);
    return;
  }
  if (webrtc::IsRtcpPacket(buf, len)) {
    return;
  }
  if (webrtc::IsRtpPacket(buf, len)) {
    if (rtp_channel_) {
      size_t new_len = len;
      srtp_session_->DecryptSrtp((uint8_t*)buf, &new_len);
      rtp_channel_->OnRTP(buf, new_len);
    }
    return;
  }
}

bool WebRtcTransport::SendPacket(const uint8_t* data,
                                 size_t len,
                                 const struct sockaddr_in& remote_address) {
  if (transport_) {
    return transport_->SendPacket(data, len, remote_address);
  }
  return false;
}

void WebRtcTransport::EncryptAndSendRtpPacket(const uint8_t* data, size_t len) {
  const uint8_t* p = data;
  size_t tmp_len = len;
  bool ret = false;
  if (is_ready_.load()) {
    ret = srtp_session_->EncryptRtp(&p, &tmp_len);
  }
  SendPacket(p, tmp_len, remote_socket_address_);
  return;
}

std::string WebRtcTransport::GetSHA256Fingerprint() {
  auto finger_prints = dtls_transport_->GetLocalFingerprints();
  for (size_t i = 0; i < finger_prints.size(); i++) {
    if (finger_prints[i].algorithm ==
        RTC::DtlsTransport::FingerprintAlgorithm::SHA256) {
      return finger_prints[i].value;
    }
  }
  return "";
}

void WebRtcTransport::OnIceServerSendStunPacket(const RTC::IceServer* iceServer,
                                                const RTC::StunPacket* packet,
                                                RTC::TransportTuple* tuple) {
  SendPacket(packet->GetData(), packet->GetSize(), *tuple); 
}

void WebRtcTransport::OnIceServerSelectedTuple(const RTC::IceServer* iceServer,
                                               RTC::TransportTuple* tuple) {
  remote_socket_address_ = *(static_cast<struct sockaddr_in*>(tuple));
  //memcpy(&remote_socket_address_, tuple, sizeof(remote_socket_address_));
  dtls_transport_->Run(RTC::DtlsTransport::Role::SERVER);
}

void WebRtcTransport::OnIceServerConnected(const RTC::IceServer* iceServer) {}

void WebRtcTransport::OnIceServerCompleted(const RTC::IceServer* iceServer) {}

void WebRtcTransport::OnIceServerDisconnected(const RTC::IceServer* iceServer) {
}

void WebRtcTransport::OnDtlsTransportConnecting(
    const RTC::DtlsTransport* dtlsTransport) {}

void WebRtcTransport::OnDtlsTransportConnected(
    const RTC::DtlsTransport* dtlsTransport,
    RTC::SrtpSession::CryptoSuite srtpCryptoSuite,
    uint8_t* srtpLocalKey,
    size_t srtpLocalKeyLen,
    uint8_t* srtpRemoteKey,
    size_t srtpRemoteKeyLen,
    std::string& remoteCert) {
  this->srtp_session_.reset(
      new RTC::SrtpSession(RTC::SrtpSession::Type::OUTBOUND, srtpCryptoSuite,
                           srtpLocalKey, srtpLocalKeyLen));
  is_ready_ = true;
}

void WebRtcTransport::OnDtlsTransportFailed(
    const RTC::DtlsTransport* dtlsTransport) {}

void WebRtcTransport::OnDtlsTransportClosed(
    const RTC::DtlsTransport* dtlsTransport) {}

void WebRtcTransport::OnDtlsTransportSendData(
    const RTC::DtlsTransport* dtlsTransport,
    const uint8_t* data,
    size_t len) {
  SendPacket(data, len, remote_socket_address_);
}

void WebRtcTransport::OnDtlsTransportApplicationDataReceived(
    const RTC::DtlsTransport* dtlsTransport,
    const uint8_t* data,
    size_t len) {}
