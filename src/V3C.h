#pragma once

#include <uvgrtp/lib.hh>

#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <string>
#include <map>
#include <exception>

#include "uvgv3crtp/global.h"
#include "Sample_Stream.h"

namespace uvgV3CRTP {

  // Forward decleration
  class V3C_GOF;
  class V3C_Unit;

  class V3C
  {
  public:
    
    V3C() = delete;
    // Uni-directional stream. For sending address should be remote address. For receiving address should be the local address (to bind to). Caller should set either RCE_SEND_ONLY or RCE_RECEIVE_ONLY to stream_flags
    V3C(const INIT_FLAGS init_flags, const char * endpoint_address, const uint16_t port, int stream_flags);
    // Bi-directional stream
    V3C(const INIT_FLAGS init_flags, const char * local_address, const char * remote_address, const uint16_t src_port, const uint16_t dst_port, int stream_flags = 0); 
    ~V3C();

    V3C(const V3C&) = delete;
    V3C& operator=(const V3C&) = delete;

    V3C(V3C&&) = default;
    V3C& operator=(V3C&&) = default;

    static std::vector<V3C_UNIT_TYPE> unit_types_from_init_flag(const INIT_FLAGS flags);
    static INIT_FLAGS init_flags_from_unit_types(const std::vector<V3C_UNIT_TYPE>& unit_types);
    
    static Sample_Stream<SAMPLE_STREAM_TYPE::V3C> parse_bitstream(const char * const bitstream, const size_t len);

    static uint8_t parse_size_precision(const char * const bitstream);
    static size_t write_size_precision(char * const bitstream, const uint8_t precision);
    static size_t parse_sample_stream_size(const char * const bitstream, const uint8_t precision);
    static size_t write_sample_stream_size(char * const bitstream, const size_t size, const uint8_t precision);

    template <SAMPLE_STREAM_TYPE E>
    static size_t sample_stream_header_size(V3C_UNIT_TYPE type);

    static size_t combineBytes(const uint8_t *const bytes, const uint8_t num_bytes);
    static void convert_size_big_endian(const uint64_t in, uint8_t* const out, const size_t output_size);


    using InfoDataType = std::map<INFO_FIELDS, INFO_FIELD_TYPES>;
    template <typename DataClass>
    static void write_out_of_band_info(std::ostream& out_stream, const DataClass& data, INFO_FMT fmt = INFO_FMT::LOGGING);
    template <typename DataClass>
    static InfoDataType read_out_of_band_info(std::istream& in_stream, INFO_FMT fmt = INFO_FMT::LOGGING, INIT_FLAGS init_flags = INIT_FLAGS::NUL);


    protected:
      uvgrtp::media_stream* get_stream(const V3C_UNIT_TYPE type) const;
      
      static RTP_FLAGS get_flags(const V3C_UNIT_TYPE type);
      static RTP_FORMAT get_format(const V3C_UNIT_TYPE type);
      static int unit_type_to_ssrc(const V3C_UNIT_TYPE type);

      static uint32_t get_new_sampling_instant(); // Generating a new sampling instant for RTP streams. Should only be used for the first instance of a media stream.
      static uint32_t calc_new_timestamp(const uint32_t old_timestamp, const uint32_t sample_rate, const uint32_t clock_rate);

      std::map<V3C_UNIT_TYPE, uvgrtp::media_stream*> streams_;
      uvgrtp::context ctx_;
      uvgrtp::session* session_;

    private:
  };

  class TimeoutException : public std::exception
  {
  public:
    explicit TimeoutException(const std::string& msg): message_(msg) {}
    virtual const char* what() const noexcept override {
      return message_.c_str();
    }
  private:
    std::string message_;
  };

  class TimestampException : public std::exception
  {
  public:
    explicit TimestampException(const std::string& msg) : message_(msg) {}
    virtual const char* what() const noexcept override {
      return message_.c_str();
    }
  private:
    std::string message_;
  };

  class ParseException : public std::exception
  {
  public:
    explicit ParseException(const std::string& msg) : message_(msg) {}
    virtual const char* what() const noexcept override {
      return message_.c_str();
    }
  private:
    std::string message_;
  };

  class ConnectionException : public std::exception
  {
  public:
    explicit ConnectionException(const std::string& msg) : message_(msg) {}
    virtual const char* what() const noexcept override {
      return message_.c_str();
    }
  private:
    std::string message_;
  };

  // Explicitly define necessary instantiations so code is linked properly
  extern template void V3C::write_out_of_band_info<Sample_Stream<SAMPLE_STREAM_TYPE::V3C>>(std::ostream&, Sample_Stream<SAMPLE_STREAM_TYPE::V3C> const&, INFO_FMT);
  extern template void V3C::write_out_of_band_info<V3C_Gof>(std::ostream&, V3C_Gof const&, INFO_FMT);
  extern template void V3C::write_out_of_band_info<V3C_Unit>(std::ostream&, V3C_Unit const&, INFO_FMT);
  extern template V3C::InfoDataType V3C::read_out_of_band_info<Sample_Stream<SAMPLE_STREAM_TYPE::V3C>>(std::istream&, INFO_FMT, INIT_FLAGS);
  extern template size_t V3C::sample_stream_header_size<SAMPLE_STREAM_TYPE::V3C>(V3C_UNIT_TYPE type);
  extern template size_t V3C::sample_stream_header_size<SAMPLE_STREAM_TYPE::NAL>(V3C_UNIT_TYPE type);
}