#include "V3C_Receiver.h"

#include <bitset>
#include <utility>

namespace v3cRTPLib {

  V3C_Receiver::V3C_Receiver()
  {
    V3C_Receiver("127.0.0.1", "127.0.0.1", INIT_FLAGS::ALL);
  }

  V3C_Receiver::V3C_Receiver(const char* receiver_address, const char* sender_address, const INIT_FLAGS flags): V3C(receiver_address, sender_address, flags)
  {
    // Parent class initializes media streams. Just set context here.
    int i = 1;
    for (const auto&[type, stream] : streams_) {
      stream->configure_ctx(RCC_REMOTE_SSRC, i++);
    }
  }


  Sample_Stream<SAMPLE_STREAM_TYPE::V3C> V3C_Receiver::receive_bitstream(const uint8_t v3c_size_precision, const std::map<V3C_UNIT_TYPE, uint8_t>& nal_size_precisions, const size_t expected_num_gofs, const std::map<V3C_UNIT_TYPE, size_t>& expected_num_nalus, const std::map<V3C_UNIT_TYPE, const V3C_Unit::V3C_Unit_Header>& headers, const int timeout) const
  {
    Sample_Stream<SAMPLE_STREAM_TYPE::V3C> new_stream(v3c_size_precision);

    for (size_t i = 0; i < expected_num_gofs; i++)
    {
      new_stream.push_back(std::move(receive_gof(nal_size_precisions, expected_num_nalus, headers, timeout, true)));
    }

    return new_stream;
  }

  template <typename V3CUnitHeaderMap>
  V3C_Gof V3C_Receiver::receive_gof(const std::map<V3C_UNIT_TYPE, uint8_t>& size_precisions, const std::map<V3C_UNIT_TYPE, size_t>& expected_sizes, V3CUnitHeaderMap&& headers, const int timeout, const bool expected_size_as_num_nalus) const
  {
    V3C_Gof new_gof;

    for (const auto&[type, stream] : streams_)
    {
      new_gof.set(std::move(receive_v3c_unit(type, size_precisions.at(type), expected_sizes.at(type), std::forward<V3CUnitHeaderMap>(headers).at(type), timeout, expected_size_as_num_nalus)));
    }

    return new_gof;
  }

  template <typename V3CUnitHeader>
  V3C_Unit V3C_Receiver::receive_v3c_unit(const V3C_UNIT_TYPE type, const uint8_t size_precision, const size_t expected_size, V3CUnitHeader&& header, const int timeout, const bool expected_size_as_num_nalus) const
  {
    if (type == V3C_VPS)
    {
      // Special handling for VPS because it contains no nalu
      uvgrtp::frame::rtp_frame* new_frame = streams_.at(type)->pull_frame(timeout);
      if (!new_frame)
      {
        //Timeout
        return V3C_Unit(std::forward<V3CUnitHeader>(header), size_precision);
      }
      V3C_Unit new_unit(std::forward<V3CUnitHeader>(header), size_precision, new_frame->payload_len);
      new_unit.push_back(reinterpret_cast<char*>(new_frame->payload));
      return new_unit;
    }

    V3C_Unit new_unit(std::forward<V3CUnitHeader>(header), size_precision);
    size_t size_received = 0;
       
    while (size_received < expected_size)
    {
      uvgrtp::frame::rtp_frame* new_frame = streams_.at(type)->pull_frame(timeout);
      if (!new_frame)
      {
        //Timeout
        break;
      }

      Nalu new_nalu(reinterpret_cast<char*>(new_frame->payload), new_frame->payload_len, type);
      new_unit.push_back(std::move(new_nalu));

      if (expected_size_as_num_nalus)
      {
        size_received++;
      }
      else
      {
        size_received += new_frame->padding_len;
      }

      (void)uvgrtp::frame::dealloc_frame(new_frame);
    }

    return new_unit;
  }

}