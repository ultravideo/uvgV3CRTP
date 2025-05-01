#include "V3C_Sender.h"

#include <stdexcept>

namespace v3cRTPLib {

  V3C_Sender::V3C_Sender(const char* sender_address, const char* receiver_address, const INIT_FLAGS flags): V3C(sender_address, receiver_address, flags)
  {
    // Parent class initializes media streams. Just set context here.
    int i = 1;
    for (const auto& [type, stream] : streams_) {
      stream->configure_ctx(RCC_SSRC, i++);
    }
  }


  V3C_Sender::V3C_Sender()
  {
    V3C_Sender("127.0.0.1", "127.0.0.1", INIT_FLAGS::NUL);
  }

  int V3C_Sender::send_bitstream(std::ifstream& bitstream) const
  {
    //get length of file
    bitstream.seekg(0, bitstream.end);
    size_t length = bitstream.tellg();
    bitstream.seekg(0, bitstream.beg);

    /* Read the file and its size */
    if (length == 0) {
      return EXIT_FAILURE; //TODO: Raise exception
    }
    
    if (!bitstream.is_open()) {
      //TODO: Raise exception
      return -1;
    }

    auto buf = std::make_unique<char[]>(length);
    if (buf == nullptr) return EXIT_FAILURE;//TODO: Raise exception

    // read into char*
    if (!(bitstream.read(buf.get(), length))) // read up to the size of the buffer
    {
      if (!bitstream.eof())
      {
        //TODO: Raise exception
        buf = nullptr; // Release allocated memory before returning nullptr
        return -1;
      }
    }

    /* Parse bitstream into sample stream */
    auto sample_stream = parse_bitstream(buf.get(), length);

    for (const auto& gof : sample_stream) {
      send_gof(gof);
    }

    // Print side-channel information needed by the receiver to receive the bitstream correctly.
    // Check INFO_FMT for available formats. Writing to file also possible.
    //write_out_bitstream_info(info, std::cout, INFO_FMT::LOGGING);


    return EXIT_SUCCESS;
  }

  void V3C_Sender::send_gof(const V3C_Gof& gof) const
  {
    for (const auto& [type, v3c_unit] : gof) {
      send_v3c_unit(v3c_unit);
    }
  }

  void V3C_Sender::send_v3c_unit(const V3C_Unit& unit) const
  {
    size_t bytes_sent = 0;
    for (const auto& nalu : unit.nalus()) {
      rtp_error_t ret = RTP_OK;
      ret = this->get_stream(unit.type())->push_frame(nalu.get().bitstream(), nalu.get().size(), this->get_flags(unit.type()));
      if (ret != RTP_OK) {
        throw std::runtime_error("Failed to send RTP frame");
      }
      bytes_sent += nalu.get().size();
    }
  }

}