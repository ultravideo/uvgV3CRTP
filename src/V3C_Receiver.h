#pragma once

#include "V3C.h"
#include "V3C_Gof.h"
#include "V3C_Unit.h"

#include <thread>
#include <iostream>
#include <fstream>
#include <chrono>
#include <algorithm>
#include <string>
#include <map>
#include <queue>

namespace uvgV3CRTP {

  class V3C_Receiver :
    public V3C
  {
  public:
    //V3C_Receiver() = delete;
    V3C_Receiver(const INIT_FLAGS flags = INIT_FLAGS::ALL, const char * local_address = "127.0.0.1", uint16_t local_port = 8890, int stream_flags = 0); // Local address to bind to i.e. the address sender sends to 
    ~V3C_Receiver() = default;

    Sample_Stream<SAMPLE_STREAM_TYPE::V3C> receive_bitstream(const uint8_t v3c_size_precision, const std::map<V3C_UNIT_TYPE, uint8_t>& nal_size_precisions, const size_t expected_num_gofs, const std::map<V3C_UNIT_TYPE, size_t>& expected_num_nalus, const std::map<V3C_UNIT_TYPE, const V3C_Unit::V3C_Unit_Header>& headers, const size_t timeout) const;
    template <typename V3CUnitHeaderMap>
    V3C_Gof receive_gof(const std::map<V3C_UNIT_TYPE, uint8_t>& size_precisions, const std::map<V3C_UNIT_TYPE, size_t>& expected_sizes, V3CUnitHeaderMap&& headers, const size_t timeout, const bool expected_size_as_num_nalus = false) const;
    template <typename V3CUnitHeader>
    V3C_Unit receive_v3c_unit(const V3C_UNIT_TYPE type, const uint8_t size_precision, const size_t expected_size, V3CUnitHeader&& header, const size_t timeout, const bool expected_size_as_num_nalus = false) const;

    void clear_receive_buffer(); // Drop all buffered data
    size_t receive_buffer_size() const; // Get total number of buffered nalus
    size_t receive_buffer_size(const V3C_UNIT_TYPE type) const; // Get number of buffered nalus

    // Attempt to push buffered data to stream. Does not create new units only push to existing ones.
    void push_buffer_to_sample_stream(Sample_Stream<SAMPLE_STREAM_TYPE::V3C>& stream) const; 
    void push_buffer_to_sample_stream(Sample_Stream<SAMPLE_STREAM_TYPE::V3C>& stream, const V3C_UNIT_TYPE type) const; 

  private:

    // Buffer for holding received data that could not be placed in a v3c unit because of a timestamp mismatch
    mutable std::map<V3C_UNIT_TYPE, std::queue<Nalu>> receive_buffer_ = {};
    void push_to_receive_buffer(Nalu&& nalu, const V3C_UNIT_TYPE type) const;
  };

  // Explicitly define necessary instantiations so code is linked properly
  extern template V3C_Gof V3C_Receiver::receive_gof<std::map<V3C_UNIT_TYPE, V3C_Unit::V3C_Unit_Header>>(const std::map<V3C_UNIT_TYPE, uint8_t>& size_precisions, const std::map<V3C_UNIT_TYPE, size_t>& expected_sizes, std::map<V3C_UNIT_TYPE, V3C_Unit::V3C_Unit_Header>&& headers, const size_t timeout, const bool expected_size_as_num_nalus) const;
  extern template V3C_Gof V3C_Receiver::receive_gof<std::map<V3C_UNIT_TYPE, V3C_Unit::V3C_Unit_Header>&>(const std::map<V3C_UNIT_TYPE, uint8_t>& size_precisions, const std::map<V3C_UNIT_TYPE, size_t>& expected_sizes, std::map<V3C_UNIT_TYPE, V3C_Unit::V3C_Unit_Header>& headers, const size_t timeout, const bool expected_size_as_num_nalus) const;
  extern template V3C_Gof V3C_Receiver::receive_gof<std::map<V3C_UNIT_TYPE, const V3C_Unit::V3C_Unit_Header>>(const std::map<V3C_UNIT_TYPE, uint8_t>& size_precisions, const std::map<V3C_UNIT_TYPE, size_t>& expected_sizes, std::map<V3C_UNIT_TYPE, const V3C_Unit::V3C_Unit_Header>&& headers, const size_t timeout, const bool expected_size_as_num_nalus) const;
  extern template V3C_Gof V3C_Receiver::receive_gof<std::map<V3C_UNIT_TYPE, const V3C_Unit::V3C_Unit_Header>&>(const std::map<V3C_UNIT_TYPE, uint8_t>& size_precisions, const std::map<V3C_UNIT_TYPE, size_t>& expected_sizes, std::map<V3C_UNIT_TYPE, const V3C_Unit::V3C_Unit_Header>& headers, const size_t timeout, const bool expected_size_as_num_nalus) const;
  extern template V3C_Unit V3C_Receiver::receive_v3c_unit<V3C_Unit::V3C_Unit_Header>(const V3C_UNIT_TYPE type, const uint8_t size_precision, const size_t expected_size, V3C_Unit::V3C_Unit_Header&& header, const size_t timeout, const bool expected_size_as_num_nalus) const;
  extern template V3C_Unit V3C_Receiver::receive_v3c_unit<V3C_Unit::V3C_Unit_Header&>(const V3C_UNIT_TYPE type, const uint8_t size_precision, const size_t expected_size, V3C_Unit::V3C_Unit_Header& header, const size_t timeout, const bool expected_size_as_num_nalus) const;
}