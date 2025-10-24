#include "V3C_Receiver.h"

#include <bitset>
#include <utility>

namespace uvgV3CRTP {

  //V3C_Receiver::V3C_Receiver()
  //{
  //  V3C_Receiver("127.0.0.1", "127.0.0.1", INIT_FLAGS::ALL);
  //}

  V3C_Receiver::V3C_Receiver(const INIT_FLAGS flags, const char * local_address, const uint16_t local_ports[NUM_V3C_UNIT_TYPES], int stream_flags) : V3C(flags, local_address, local_ports, (stream_flags | RCE_RECEIVE_ONLY))
  {
    // Get overlapping ports
    auto overlaps = V3C::check_port_overlap(local_ports, flags);

    // Parent class initializes media streams. Just set context here.
    for (const auto&[type, stream] : streams_) {
      // Only set SSRC if port overlaps with another stream
      if (overlaps[type]) stream->configure_ctx(RCC_REMOTE_SSRC, V3C::unit_type_to_ssrc(type));

      // Init a receive buffer for each stream type
      receive_buffer_.emplace(type, std::queue<Nalu>());
    }
  }


  Sample_Stream<SAMPLE_STREAM_TYPE::V3C> V3C_Receiver::receive_bitstream(const uint8_t v3c_size_precision, const std::map<V3C_UNIT_TYPE, uint8_t>& nal_size_precisions, const size_t expected_num_gofs, const std::map<V3C_UNIT_TYPE, size_t>& expected_num_nalus, const std::map<V3C_UNIT_TYPE, const V3C_Unit::V3C_Unit_Header>& headers, const size_t timeout) const
  {
    Sample_Stream<SAMPLE_STREAM_TYPE::V3C> new_stream(v3c_size_precision);
    std::map<V3C_UNIT_TYPE, size_t> local_exp_num_nalus = expected_num_nalus;
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
        // Try to process receive buffer in case a lot of reordering has happened.
        push_buffer_to_sample_stream(new_stream);
      }
    }
    catch (const TimeoutException& e)
    {
      // Timeout trying to receive anymore gofs
      std::cerr << "Timeout: " << e.what() << std::endl;
    }

    return new_stream;
  }

  void V3C_Receiver::install_receive_hook(const V3C_UNIT_TYPE type, void* arg, void(*hook)(void*, uvgrtp::frame::rtp_frame*)) const
  {
    if (streams_.find(type) == streams_.end())
    {
      throw ConnectionException("Receiver not initialized for V3C unit type " + std::to_string(static_cast<int>(type)));
    }

    auto error = streams_.at(type)->install_receive_hook(arg, hook);

    if (error == RTP_INVALID_VALUE)
    {
      throw ConnectionException("Invalid hook function or argument for V3C unit type " + std::to_string(static_cast<int>(type)));
    }
    else if (error == RTP_NOT_INITIALIZED)
    {
      throw ConnectionException("RTP context not initialized for V3C unit type " + std::to_string(static_cast<int>(type)));
    }
    else if (error != RTP_OK)
    {
      throw ConnectionException("Failed to install receive hook for V3C unit type " + std::to_string(static_cast<int>(type)));
    }
  }

  void V3C_Receiver::clear_receive_buffer()
  {
    for (auto&[type, buffer] : receive_buffer_)
    {
      std::queue<Nalu> empty;
      std::swap(buffer, empty);
    }
  }

  size_t V3C_Receiver::receive_buffer_size() const
  {
    size_t total_size = 0;
    for (const auto&[type, buffer] : receive_buffer_)
    {
      total_size += buffer.size();
    }
    return total_size;
  }

  size_t V3C_Receiver::receive_buffer_size(const V3C_UNIT_TYPE type) const
  {
    return receive_buffer_.at(type).size();
  }

  void V3C_Receiver::push_buffer_to_sample_stream(Sample_Stream<SAMPLE_STREAM_TYPE::V3C>& stream) const
  {
    for( auto& [type, buffer]: receive_buffer_)
    {
      push_buffer_to_sample_stream(stream, type);
    }
  }

  void V3C_Receiver::push_buffer_to_sample_stream(Sample_Stream<SAMPLE_STREAM_TYPE::V3C>& stream, const V3C_UNIT_TYPE type) const
  {
    auto& buffer = receive_buffer_.at(type);
    size_t unbuffered_count = buffer.size();
    while (unbuffered_count > 0)
    {
      Nalu tmp_nalu = std::move(buffer.front());
      buffer.pop();
      unbuffered_count--;

      if (!stream.push_back(std::move(tmp_nalu), type))
      {
        // Could not push nalu to stream, push it back to buffer. tmp_nalu should not have been moved if push failed
        push_to_receive_buffer(std::move(tmp_nalu), type);
      }
    }
  }

  void V3C_Receiver::push_to_receive_buffer(Nalu&& nalu, const V3C_UNIT_TYPE type) const
  {
    // Push new nalu to receive buffer and check that the buffer is not too full. Pop existing nalu if buffer is full
    if (receive_buffer_.size() >= RECEIVE_BUFFER_SIZE)
    {
      std::cerr << "Warning: Receive buffer full, dropping oldest nalu of type: " << static_cast<int>(type) << std::endl;
      receive_buffer_.at(type).pop();
    }
    receive_buffer_.at(type).emplace(std::move(nalu));
  }

  template <typename V3CUnitHeaderMap>
  V3C_Gof V3C_Receiver::receive_gof(const std::map<V3C_UNIT_TYPE, uint8_t>& size_precisions, const std::map<V3C_UNIT_TYPE, size_t>& expected_sizes, V3CUnitHeaderMap&& headers, const size_t timeout, const bool expected_size_as_num_nalus) const
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
  V3C_Unit V3C_Receiver::receive_v3c_unit(const V3C_UNIT_TYPE type, const uint8_t size_precision, const size_t expected_size, V3CUnitHeader&& header, const size_t timeout, const bool expected_size_as_num_nalus) const
  {
    if (streams_.find(type) == streams_.end())
    {
      throw ConnectionException("Receiver not initialized for V3C unit type " + std::to_string(static_cast<int>(type)) + ")");
    }

    if (type == V3C_VPS && expected_size > 0)
    {
      // Special handling for VPS because it contains no nalu
      uvgrtp::frame::rtp_frame* new_frame = streams_.at(type)->pull_frame(timeout);
      if (!new_frame)
      {
        throw TimeoutException("V3C_VPS receiving timeout");
        //return V3C_Unit(std::forward<V3CUnitHeader>(header), size_precision);
      }
      V3C_Unit new_unit(std::forward<V3CUnitHeader>(header), size_precision);
      new_unit.push_back(Nalu(0, 0, 0, reinterpret_cast<char*>(new_frame->payload), new_frame->payload_len, type));
      new_unit.set_timestamp(new_frame->header.timestamp);

      (void)uvgrtp::frame::dealloc_frame(new_frame);
      return new_unit;
    }

    V3C_Unit new_unit(std::forward<V3CUnitHeader>(header), size_precision);
    size_t size_received = 0;
    size_t new_nalu_size = 1;
    bool timestamp_mismatch = false;
    size_t buffer_unprocessed_count = receive_buffer_size(type); // Track how many nalus we've processed from the receive buffer to avoid infinite loops
    const bool auto_size = expected_size == static_cast<size_t>(-1); // If expected size is -1, keep receiving until timestamp changes
       
    while (size_received < expected_size)
    {
      uvgrtp::frame::rtp_frame* new_frame;
      Nalu new_nalu;
      
      if (buffer_unprocessed_count > 0)
      {
        // If we have frames in the receive buffer, use those first
        new_nalu = std::move(receive_buffer_.at(type).front());
        receive_buffer_.at(type).pop();
        buffer_unprocessed_count--;
      }
      else
      {
        new_frame = streams_.at(type)->pull_frame(timeout);

        if (!new_frame)
        {
          //Timeout
          if (size_received == 0) throw TimeoutException("V3C unit receiving timeout");
          //std::cerr << "timeout " << (int)type << std::endl;
          break;
        }

        new_nalu = Nalu(reinterpret_cast<char*>(new_frame->payload), new_frame->payload_len, type);
        new_nalu.set_timestamp(new_frame->header.timestamp);

        (void)uvgrtp::frame::dealloc_frame(new_frame);
      }
      new_nalu_size = expected_size_as_num_nalus ? 1 : new_nalu.size();

      try {
        new_unit.push_back(std::move(new_nalu));
        size_received += new_nalu_size;
        timestamp_mismatch = false; // We got a nalu that matches the v3c unit timestamp, reset mismatch flag
      }
      catch (const TimestampException& e)
      {
        // Store the nalu in the timestamp buffer for later processing. If a timestamp exception is thrown, new_nalu should not have been moved so it is still valid
        push_to_receive_buffer(std::move(new_nalu), type);

        // If using auto size, we can just stop receiving nalus when we get a timestamp mismatch
        if (auto_size && size_received > 0) break;

        // Nalu timestamp does not match V3C unit timestamp, this nalu does not belong to this v3c unit
        std::cerr << "Timestamp exception: " << e.what() << " in unit type id " << static_cast<int>(type) << std::endl;

        // Keep trying to receive nalus for this v3c unit until we get the expected size, but if we keep getting timestamp mismatches increment the expected size so we don't get stuck in an infinite loop
        // Also don't count nalus from the receive buffer when deciding to increment expected size
        if (timestamp_mismatch && buffer_unprocessed_count <= 0)
        {
          // We had a timestamp mismatch on the previous nalu too, increment expected size so we don't get stuck in an infinite loop
          size_received += new_nalu_size;
        }
        else
        {
          timestamp_mismatch = true;
        }
      }
    }

    return new_unit;
  }

  // Explicitly define necessary instantiations so code is linked properly
  template V3C_Gof V3C_Receiver::receive_gof<std::map<V3C_UNIT_TYPE, V3C_Unit::V3C_Unit_Header>>(const std::map<V3C_UNIT_TYPE, uint8_t>& size_precisions, const std::map<V3C_UNIT_TYPE, size_t>& expected_sizes, std::map<V3C_UNIT_TYPE, V3C_Unit::V3C_Unit_Header>&& headers, const size_t timeout, const bool expected_size_as_num_nalus) const;
  template V3C_Gof V3C_Receiver::receive_gof<std::map<V3C_UNIT_TYPE, V3C_Unit::V3C_Unit_Header>&>(const std::map<V3C_UNIT_TYPE, uint8_t>& size_precisions, const std::map<V3C_UNIT_TYPE, size_t>& expected_sizes, std::map<V3C_UNIT_TYPE, V3C_Unit::V3C_Unit_Header>& headers, const size_t timeout, const bool expected_size_as_num_nalus) const;
  template V3C_Gof V3C_Receiver::receive_gof<std::map<V3C_UNIT_TYPE, const V3C_Unit::V3C_Unit_Header>>(const std::map<V3C_UNIT_TYPE, uint8_t>& size_precisions, const std::map<V3C_UNIT_TYPE, size_t>& expected_sizes, std::map<V3C_UNIT_TYPE, const V3C_Unit::V3C_Unit_Header>&& headers, const size_t timeout, const bool expected_size_as_num_nalus) const;
  template V3C_Gof V3C_Receiver::receive_gof<std::map<V3C_UNIT_TYPE, const V3C_Unit::V3C_Unit_Header>&>(const std::map<V3C_UNIT_TYPE, uint8_t>& size_precisions, const std::map<V3C_UNIT_TYPE, size_t>& expected_sizes, std::map<V3C_UNIT_TYPE, const V3C_Unit::V3C_Unit_Header>& headers, const size_t timeout, const bool expected_size_as_num_nalus) const;
  template V3C_Unit V3C_Receiver::receive_v3c_unit<V3C_Unit::V3C_Unit_Header>(const V3C_UNIT_TYPE type, const uint8_t size_precision, const size_t expected_size, V3C_Unit::V3C_Unit_Header&& header, const size_t timeout, const bool expected_size_as_num_nalus) const;
  template V3C_Unit V3C_Receiver::receive_v3c_unit<V3C_Unit::V3C_Unit_Header&>(const V3C_UNIT_TYPE type, const uint8_t size_precision, const size_t expected_size, V3C_Unit::V3C_Unit_Header& header, const size_t timeout, const bool expected_size_as_num_nalus) const;
}