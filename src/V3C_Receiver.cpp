#include "V3C_Receiver.h"

#include <bitset>
#include <utility>

namespace v3cRTPLib {

  //V3C_Receiver::V3C_Receiver()
  //{
  //  V3C_Receiver("127.0.0.1", "127.0.0.1", INIT_FLAGS::ALL);
  //}

  V3C_Receiver::V3C_Receiver(const INIT_FLAGS flags, const char * local_address, const uint16_t local_port, int stream_flags): V3C(flags, local_address, local_port, (stream_flags | RCE_RECEIVE_ONLY))
  {
    // Parent class initializes media streams. Just set context here.
    for (const auto&[type, stream] : streams_) {
      stream->configure_ctx(RCC_REMOTE_SSRC, static_cast<int>(type)); //TODO: SSRC value could be set in a better way. Need to match the sender values respectively
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
      try
      {
        new_gof.set(std::move(receive_v3c_unit(type, size_precisions.at(type), expected_sizes.at(type), std::forward<V3CUnitHeaderMap>(headers).at(type), timeout, expected_size_as_num_nalus)));
      }
      catch (const TimeoutException& e)
      {
        // Timeout trying to receive v3c unit, try receiving othre units
        std::cerr << "Timeout: " << e.what() << " in unit type id " << static_cast<int>(type) << std::endl;
      }
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
        throw TimeoutException("V3C_VPS receiving timeout");
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
        if (size_received == 0) throw TimeoutException("V3C unit receiving timeout");
        break;
      }
      
      Nalu new_nalu(reinterpret_cast<char*>(new_frame->payload), new_frame->payload_len, type);
      if (expected_size_as_num_nalus)
      {
        size_received++;
      }
      else
      {
        size_received += new_nalu.size();
      }
      new_unit.push_back(std::move(new_nalu));

      (void)uvgrtp::frame::dealloc_frame(new_frame);
    }

    return new_unit;
  }

  // Explicitly define necessary instantiations so code is linked properly
  template V3C_Gof V3C_Receiver::receive_gof<std::map<V3C_UNIT_TYPE, V3C_Unit::V3C_Unit_Header>>(const std::map<V3C_UNIT_TYPE, uint8_t>& size_precisions, const std::map<V3C_UNIT_TYPE, size_t>& expected_sizes, std::map<V3C_UNIT_TYPE, V3C_Unit::V3C_Unit_Header>&& headers, const int timeout, const bool expected_size_as_num_nalus) const;
  template V3C_Gof V3C_Receiver::receive_gof<std::map<V3C_UNIT_TYPE, V3C_Unit::V3C_Unit_Header>&>(const std::map<V3C_UNIT_TYPE, uint8_t>& size_precisions, const std::map<V3C_UNIT_TYPE, size_t>& expected_sizes, std::map<V3C_UNIT_TYPE, V3C_Unit::V3C_Unit_Header>& headers, const int timeout, const bool expected_size_as_num_nalus) const;
  template V3C_Gof V3C_Receiver::receive_gof<std::map<V3C_UNIT_TYPE, const V3C_Unit::V3C_Unit_Header>>(const std::map<V3C_UNIT_TYPE, uint8_t>& size_precisions, const std::map<V3C_UNIT_TYPE, size_t>& expected_sizes, std::map<V3C_UNIT_TYPE, const V3C_Unit::V3C_Unit_Header>&& headers, const int timeout, const bool expected_size_as_num_nalus) const;
  template V3C_Gof V3C_Receiver::receive_gof<std::map<V3C_UNIT_TYPE, const V3C_Unit::V3C_Unit_Header>&>(const std::map<V3C_UNIT_TYPE, uint8_t>& size_precisions, const std::map<V3C_UNIT_TYPE, size_t>& expected_sizes, std::map<V3C_UNIT_TYPE, const V3C_Unit::V3C_Unit_Header>& headers, const int timeout, const bool expected_size_as_num_nalus) const;
  template V3C_Unit V3C_Receiver::receive_v3c_unit<V3C_Unit::V3C_Unit_Header>(const V3C_UNIT_TYPE type, const uint8_t size_precision, const size_t expected_size, V3C_Unit::V3C_Unit_Header&& header, const int timeout, const bool expected_size_as_num_nalus) const;
  template V3C_Unit V3C_Receiver::receive_v3c_unit<V3C_Unit::V3C_Unit_Header&>(const V3C_UNIT_TYPE type, const uint8_t size_precision, const size_t expected_size, V3C_Unit::V3C_Unit_Header& header, const int timeout, const bool expected_size_as_num_nalus) const;
}