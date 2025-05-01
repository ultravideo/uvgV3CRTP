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

namespace v3cRTPLib {

  class V3C_Receiver :
    public V3C
  {
  public:
    V3C_Receiver();
    V3C_Receiver(const char* receiver_address, const char* sender_address, INIT_FLAGS flags);
    ~V3C_Receiver() = default;

    Sample_Stream<SAMPLE_STREAM_TYPE::V3C> receive_bitstream(const uint8_t v3c_size_precision, const std::map<V3C_UNIT_TYPE, uint8_t>& nal_size_precisions, const size_t expected_num_gofs, const std::map<V3C_UNIT_TYPE, size_t>& expected_num_nalus, const std::map<V3C_UNIT_TYPE, const V3C_Unit::V3C_Unit_Header>& headers, int timeout) const;
    template <typename V3CUnitHeaderMap>
    V3C_Gof receive_gof(const std::map<V3C_UNIT_TYPE, uint8_t>& size_precisions, const std::map<V3C_UNIT_TYPE, size_t>& expected_sizes, V3CUnitHeaderMap&& headers, const int timeout, const bool expected_size_as_num_nalus = false) const;
    template <typename V3CUnitHeader>
    V3C_Unit receive_v3c_unit(const V3C_UNIT_TYPE type, const uint8_t size_precision, const size_t expected_size, V3CUnitHeader&& header, const int timeout, const bool expected_size_as_num_nalus = false) const;


private:


  };

}