#ifndef MS_RTC_ICE_SERVER_HPP
#define MS_RTC_ICE_SERVER_HPP

#include <list>
#include <string>

#include "common/common.h"
#include "rtc/stun_packet.h"

namespace RTC {
//using TransportTuple = struct sockaddr_in;
typedef struct sockaddr_in TransportTuple;
class IceServer {
 public:
  enum class IceState { NEW = 1, CONNECTED, COMPLETED, DISCONNECTED };

 public:
  class Listener {
   public:
    virtual ~Listener() = default;

   public:
    /**
     * These callbacks are guaranteed to be called before ProcessStunPacket()
     * returns, so the given pointers are still usable.
     */
    virtual void OnIceServerSendStunPacket(const RTC::IceServer* iceServer,
                                           const RTC::StunPacket* packet,
                                           RTC::TransportTuple* tuple) = 0;
    virtual void OnIceServerSelectedTuple(const RTC::IceServer* iceServer,
                                          RTC::TransportTuple* tuple) = 0;
    virtual void OnIceServerConnected(const RTC::IceServer* iceServer) = 0;
    virtual void OnIceServerCompleted(const RTC::IceServer* iceServer) = 0;
    virtual void OnIceServerDisconnected(const RTC::IceServer* iceServer) = 0;
  };

 public:
  IceServer(Listener* listener, const std::string& usernameFragment, const std::string& password);

 public:
  void ProcessStunPacket(RTC::StunPacket* packet, RTC::TransportTuple* tuple);
  const std::string& GetUsernameFragment() const { return this->usernameFragment; }
  const std::string& GetPassword() const { return this->password; }
  IceState GetState() const { return this->state; }
  RTC::TransportTuple* GetSelectedTuple() const { return this->selectedTuple; }
  void SetUsernameFragment(const std::string& usernameFragment) {
    this->oldUsernameFragment = this->usernameFragment;
    this->usernameFragment = usernameFragment;
  }
  void SetPassword(const std::string& password) {
    this->oldPassword = this->password;
    this->password = password;
  }

 private:
  void HandleTuple(RTC::TransportTuple* tuple);

 private:
  // Passed by argument.
  Listener* listener{nullptr};
  // Others.
  std::string usernameFragment;
  std::string password;
  std::string oldUsernameFragment;
  std::string oldPassword;
  IceState state{IceState::NEW};
  RTC::TransportTuple* selectedTuple{nullptr};
};
}  // namespace RTC

#endif
