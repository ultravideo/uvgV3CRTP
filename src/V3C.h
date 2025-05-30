#pragma once

#include <uvgrtp/lib.hh>

#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <string>
#include <map>

#include "v3crtplib/global.h"
#include "Sample_Stream.h"

namespace v3cRTPLib {


  class V3C
  {
  public:
    
    V3C() = default;
    V3C(const char * local_address, const char * remote_address, const INIT_FLAGS init_flags, const uint16_t src_port = 8892, const uint16_t dst_port = 8890, int stream_flags = 0);
    ~V3C();

    V3C(const V3C&) = delete;
    V3C& operator=(const V3C&) = delete;

    V3C(V3C&&) = default;
    V3C& operator=(V3C&&) = default;

    static std::vector<V3C_UNIT_TYPE> unit_types_from_init_flag(const INIT_FLAGS flags);
    
    static Sample_Stream<SAMPLE_STREAM_TYPE::V3C> parse_bitstream(const char * const bitstream, const size_t len);

    static uint8_t parse_size_precision(const char * const bitstream);
    static size_t write_size_precision(char * const bitstream, const uint8_t precision);
    static size_t parse_sample_stream_size(const char * const bitstream, const uint8_t precision);
    static size_t write_sample_stream_size(char * const bitstream, const size_t size, const uint8_t precision);

    static size_t combineBytes(const uint8_t *const bytes, const uint8_t num_bytes);
    static void convert_size_big_endian(const uint64_t in, uint8_t* const out, const size_t output_size);


    using InfoDataType = std::map<INFO_FIELDS, uint64_t>;
    template <INFO_FMT F = INFO_FMT::LOGGING, typename DataClass>
    static void write_out_of_band_info(std::ostream& out_stream, const DataClass& data);
    template <INFO_FMT F = INFO_FMT::LOGGING, typename DataClass>
    static InfoDataType read_out_of_band_info(std::istream& in_stream);


    protected:
      uvgrtp::media_stream* get_stream(const V3C_UNIT_TYPE type) const;
      
      static RTP_FLAGS get_flags(const V3C_UNIT_TYPE type);
      static RTP_FORMAT get_format(const V3C_UNIT_TYPE type);

      std::map<V3C_UNIT_TYPE, uvgrtp::media_stream*> streams_;
      uvgrtp::context ctx_;
      uvgrtp::session* session_;

    private:
  };

}