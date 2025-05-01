#pragma once

#include "V3C.h"
#include "V3C_Gof.h"
#include "V3C_Unit.h"

#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <string>
#include <atomic>

namespace v3cRTPLib {

  class V3C_Sender :
    public V3C
  {
  public:
    V3C_Sender(); // By default use 127.0.0.1 for sending
    V3C_Sender(const char* sender_address, const char* receiver_address, const INIT_FLAGS flags);
    ~V3C_Sender() = default;

    int send_bitstream(std::ifstream& bitstream) const;
    void send_gof(const V3C_Gof& gof) const;
    void send_v3c_unit(const V3C_Unit& unit) const;

  private:


  };

}