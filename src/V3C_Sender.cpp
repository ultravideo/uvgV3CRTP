#include "V3C_Sender.h"

#include <stdexcept>

namespace v3cRTPLib {

  V3C_Sender::V3C_Sender(const char* sender_address, const char* receiver_address, const INIT_FLAGS flags): V3C(sender_address, receiver_address, flags)
  {
    // Parent class initializes media streams. Just set context here.
    int i = 1;
    for (const auto& [type, stream] : streams_) {
      stream->configure_ctx(RCC_SSRC, i++);
    }
  }


  V3C_Sender::V3C_Sender()
  {
    V3C_Sender("127.0.0.1", "127.0.0.1", INIT_FLAGS::ALL);
  }

  void V3C_Sender::send_bitstream(const Sample_Stream<SAMPLE_STREAM_TYPE::V3C>& bitstream) const
  {
    for (const auto& gof : bitstream) {
      send_gof(gof);
    }
  }

  void V3C_Sender::send_gof(const V3C_Gof& gof) const
  {
    for (const auto& [type, v3c_unit] : gof) {
      send_v3c_unit(v3c_unit);
    }
  }

  void V3C_Sender::send_v3c_unit(const V3C_Unit& unit) const
  {
    size_t bytes_sent = 0;
    for (const auto& nalu : unit.nalus()) {
      rtp_error_t ret = RTP_OK;
      ret = this->get_stream(unit.type())->push_frame(nalu.get().bitstream(), nalu.get().size(), this->get_flags(unit.type()));
      if (ret != RTP_OK) {
        throw std::runtime_error("Failed to send RTP frame");
      }
      bytes_sent += nalu.get().size();
    }
  }

}