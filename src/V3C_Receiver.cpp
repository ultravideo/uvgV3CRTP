#include "V3C_Receiver.h"

#include <bitset>
#include <utility>

namespace uvgV3CRTP {

  //V3C_Receiver::V3C_Receiver()
  //{
  //  V3C_Receiver("127.0.0.1", "127.0.0.1", INIT_FLAGS::ALL);
  //}

  V3C_Receiver::V3C_Receiver(const INIT_FLAGS flags, const char * local_address, const uint16_t local_port, int stream_flags): V3C(flags, local_address, local_port, (stream_flags | RCE_RECEIVE_ONLY))
  {
    // Parent class initializes media streams. Just set context here.
    for (const auto&[type, stream] : streams_) {
      stream->configure_ctx(RCC_REMOTE_SSRC, V3C::unit_type_to_ssrc(type)); 
      // Init a receive buffer for each stream type
      receive_buffer_.emplace(type, std::queue<uvgrtp::frame::rtp_frame*>());
    }
  }


  Sample_Stream<SAMPLE_STREAM_TYPE::V3C> V3C_Receiver::receive_bitstream(const uint8_t v3c_size_precision, const std::map<V3C_UNIT_TYPE, uint8_t>& nal_size_precisions, const size_t expected_num_gofs, const std::map<V3C_UNIT_TYPE, size_t>& expected_num_nalus, const std::map<V3C_UNIT_TYPE, const V3C_Unit::V3C_Unit_Header>& headers, const int timeout) const
  {
    Sample_Stream<SAMPLE_STREAM_TYPE::V3C> new_stream(v3c_size_precision);
    auto local_exp_num_nalus = expected_num_nalus;
    std::map<V3C_UNIT_TYPE, V3C_Unit::V3C_Unit_Header> local_headers = {};
    for (const auto& [type, header]: headers)
    {
      local_headers.emplace(type, header);
    }

    try
    {
      for (size_t i = 0; i < expected_num_gofs; i++)
      {
        new_stream.push_back(std::move(receive_gof(nal_size_precisions, local_exp_num_nalus, local_headers, timeout, true)));
        // Decrement VPS count so we don't try to receive stuff that isn't coming
        if (new_stream.back().find(V3C_VPS) != new_stream.back().end())
        {
          local_exp_num_nalus.at(V3C_VPS) -= 1;
          if (local_exp_num_nalus.at(V3C_VPS) != 0)
          {
            // We're expecting a new VPS so following headers should increment VPS id (this is kind of a hacky thing to do but VPS should not be sent over rtp anyway)
            for (auto&[type, unit_header] : local_headers)
            {
              unit_header.vuh_v3c_parameter_set_id += 1;
            }
          }
        }
      }
    }
    catch (const TimeoutException& e)
    {
      // Timeout trying to receive anymore gofs
      std::cerr << "Timeout: " << e.what() << std::endl;
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
    if (new_gof.size() == 0) throw TimeoutException("GoF receiving timeout");

    return new_gof;
  }

  template <typename V3CUnitHeader>
  V3C_Unit V3C_Receiver::receive_v3c_unit(const V3C_UNIT_TYPE type, const uint8_t size_precision, const size_t expected_size, V3CUnitHeader&& header, const int timeout, const bool expected_size_as_num_nalus) const
  {
    // TODO: check that type is a valid type and has been initialized in streams_
    if (type == V3C_VPS && expected_size > 0)
    {
      // Special handling for VPS because it contains no nalu
      uvgrtp::frame::rtp_frame* new_frame = streams_.at(type)->pull_frame(timeout);
      if (!new_frame)
      {
        throw TimeoutException("V3C_VPS receiving timeout");
        //return V3C_Unit(std::forward<V3CUnitHeader>(header), size_precision);
      }
      V3C_Unit new_unit(std::forward<V3CUnitHeader>(header), size_precision, new_frame->header.timestamp);
      new_unit.push_back(Nalu(0, 0, 0, reinterpret_cast<char*>(new_frame->payload), new_frame->payload_len, type, new_frame->header.timestamp));

      (void)uvgrtp::frame::dealloc_frame(new_frame);
      return new_unit;
    }

    V3C_Unit new_unit(std::forward<V3CUnitHeader>(header), size_precision);
    size_t size_received = 0;
       
    while (size_received < expected_size)
    {
      uvgrtp::frame::rtp_frame* new_frame;
      
      if(!receive_buffer_.at(type).empty())
      {
        // If we have frames in the receive buffer, use those first
        new_frame = receive_buffer_.at(type).front();
        receive_buffer_.at(type).pop();
      }
      else
      {
        new_frame = streams_.at(type)->pull_frame(timeout);
      }

      if (!new_frame)
      {
        //Timeout
        if (size_received == 0) throw TimeoutException("V3C unit receiving timeout");
        //std::cerr << "timeout " << (int)type << std::endl;
        break;
      }
      
      Nalu new_nalu(reinterpret_cast<char*>(new_frame->payload), new_frame->payload_len, type, new_frame->header.timestamp);
      if (expected_size_as_num_nalus)
      {
        size_received++;
      }
      else
      {
        size_received += new_nalu.size();
      }

      try {
        new_unit.push_back(std::move(new_nalu));
      }
      catch (const TimestampException& e)
      {
        // Nalu timestamp does not match V3C unit timestamp, this nalu does not belong to this v3c unit
        std::cerr << "Timestamp exception: " << e.what() << " in unit type id " << static_cast<int>(type) << std::endl;
        // Store the nalu in the timestamp buffer for later processing
        receive_buffer_.at(type).push(new_frame);
        break; // No guarantee that the next frame is from the same unit, so break here. Need to use receive_buffer_ to add possible other nalus for this unit later
      }

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