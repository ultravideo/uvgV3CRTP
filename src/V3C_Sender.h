#pragma once
#include "V3C.h"

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
    V3C_Sender();
    ~V3C_Sender();

    int send();

    static void sender_func(uvgrtp::media_stream* stream, const char* cbuf, const std::vector<v3c_unit_info> &units, rtp_flags_t flags, int fmt, std::atomic<size_t> &bytes_sent);

  private:
    static constexpr char LOCAL_IP[] = "127.0.0.1";

    // Path to the V-PCC file that you want to send
    std::string PATH = "";
    
  };

}