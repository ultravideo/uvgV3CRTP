#pragma once

#include "V3C.h"
#include "V3C_Gof.h"
#include "V3C_Unit.h"
#include "Sample_Stream.h"

#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <string>
#include <atomic>

namespace uvgV3CRTP {

  class V3C_Sender :
    public V3C
  {
  public:
    //V3C_Sender() = delete;
    V3C_Sender(const INIT_FLAGS flags = INIT_FLAGS::ALL, const char * receiver_address = "127.0.0.1", const uint16_t dst_port = 8890, int stream_flags = 0);
    ~V3C_Sender() = default;

    void send_bitstream(const Sample_Stream<SAMPLE_STREAM_TYPE::V3C>& bitstream, const uint32_t rate_limit = 0) const;
    void send_gof(const V3C_Gof& gof) const;
    void send_v3c_unit(const V3C_Unit& unit) const;

    uint32_t get_initial_timestamp() const;

  private:

    const Timestamp initial_timestamp_; // Initial timestamp for the first frame sent
  };

}