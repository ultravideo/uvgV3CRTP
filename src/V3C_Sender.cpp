#include "V3C_Sender.h"

#include <stdexcept>

namespace uvgV3CRTP {

  V3C_Sender::V3C_Sender(const INIT_FLAGS flags, const char * receiver_address, const uint16_t dst_port, int stream_flags): V3C(flags, receiver_address, dst_port, (stream_flags | RCE_SEND_ONLY)), initial_timestamp_(get_new_sampling_instant())
  {
    // Parent class initializes media streams. Just set context here.
    for (const auto& [type, stream] : streams_) {
      stream->configure_ctx(RCC_SSRC, V3C::unit_type_to_ssrc(type));

      // Set RTCP for the custom timestamp if RTCP is enabled
      if (stream_flags & RCE_RTCP) {
        stream->get_rtcp()->set_ts_info(uvgrtp::clock::ntp::now(), RTP_CLOCK_RATE, initial_timestamp_);
      }
    }
  }


  //V3C_Sender::V3C_Sender()
  //{
  //  V3C_Sender("127.0.0.1", "127.0.0.1", INIT_FLAGS::ALL);
  //}

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
    for (const auto& nalu : unit.nalus()) {
      rtp_error_t ret = RTP_OK;
      ret = this->get_stream(unit.type())->push_frame(nalu.get().bitstream(), nalu.get().size(), this->get_flags(unit.type()));
      if (ret != RTP_OK) {
        throw std::runtime_error("Failed to send RTP frame");
      }
    }
  }

}