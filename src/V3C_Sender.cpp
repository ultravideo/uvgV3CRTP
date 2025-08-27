#include "V3C_Sender.h"

#include <stdexcept>
#include <thread>

namespace uvgV3CRTP {

  V3C_Sender::V3C_Sender(const INIT_FLAGS flags, const char * receiver_address, const uint16_t dst_port, int stream_flags): V3C(flags, receiver_address, dst_port, (stream_flags | RCE_SEND_ONLY)), initial_timestamp_(get_new_sampling_instant())
  {
    // Parent class initializes media streams. Just set context here.
    for (const auto& [type, stream] : streams_) {
      stream->configure_ctx(RCC_SSRC, V3C::unit_type_to_ssrc(type));

      // Set RTCP for the custom timestamp if RTCP is enabled
      if (stream_flags & RCE_RTCP) {
        stream->get_rtcp()->set_ts_info(uvgrtp::clock::ntp::now(), RTP_CLOCK_RATE, initial_timestamp_.get_timestamp());
      }
    }
  }


  //V3C_Sender::V3C_Sender()
  //{
  //  V3C_Sender("127.0.0.1", "127.0.0.1", INIT_FLAGS::ALL);
  //}

  void V3C_Sender::send_bitstream(const Sample_Stream<SAMPLE_STREAM_TYPE::V3C>& bitstream, const uint32_t rate_limit) const
  {
    auto timestamp = initial_timestamp_.get_timestamp();
    for (const auto& gof : bitstream) {
      // If initial timestamp has been set and gof timestamp is not set, use custom timestamp for gofs
      if (initial_timestamp_.is_timestamp_set() && !gof.is_timestamp_set()) {
        gof.set_timestamp(timestamp);
        timestamp = calc_new_timestamp(timestamp, DEFAULT_FRAME_RATE, RTP_CLOCK_RATE);
      }
      else if (gof.is_timestamp_set()) { // else advance timestamp based on gof timestamp
        timestamp = calc_new_timestamp(gof.get_timestamp(), DEFAULT_FRAME_RATE, RTP_CLOCK_RATE);
      }
      send_gof(gof);

      if (rate_limit > 0) {
        // Limit rate if requested
        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / rate_limit));
      }
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
    if (streams_.find(unit.type()) == streams_.end())
    {
      throw ConnectionException("Sender not initialized for V3C unit type " + std::to_string(static_cast<int>(unit.type())) + ")");
    }

    for (const auto& nalu : unit.nalus()) {
      rtp_error_t ret = RTP_OK;
      if (!nalu.get().is_timestamp_set()) {
        ret = this->get_stream(unit.type())->push_frame(nalu.get().bitstream(), nalu.get().size(), this->get_flags(unit.type()));
      }
      else
      {
        ret = this->get_stream(unit.type())->push_frame(nalu.get().bitstream(), nalu.get().size(), nalu.get().get_timestamp(), this->get_flags(unit.type()));
      }
      if (ret != RTP_OK) {
        throw std::runtime_error("Failed to send RTP frame");
      }
    }
  }

  uint32_t V3C_Sender::get_initial_timestamp() const
  {
    return initial_timestamp_.get_timestamp();
  }

}