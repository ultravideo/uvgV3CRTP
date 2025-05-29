#pragma once

#include "v3crtplib/global.h"

#include <cstring>

namespace v3cRTPLib {

  class Nalu
  {
  public:
    Nalu(const char * const bitstream, const size_t len, const V3C_UNIT_TYPE type);
    Nalu(const uint8_t nal_unit_type, const uint8_t nal_layer_id, const uint8_t nal_temporal_id, const char * const payload, const size_t len, const V3C_UNIT_TYPE type);
    ~Nalu();

    Nalu(const Nalu&) = delete;
    Nalu& operator=(const Nalu&) = delete;

    Nalu(Nalu&&) = default;
    Nalu& operator=(Nalu&&) = default;

    uint8_t* bitstream() const;
    size_t size() const;

    uint8_t nal_unit_type() const;
    uint8_t nal_layer_id() const;
    uint8_t nal_temporal_id() const;

  private:

    void init_bitstream(const size_t len);

    void parse_header(const V3C_UNIT_TYPE type);
    template <V3C_UNIT_TYPE E>
    void parse_header();

    void write_header(const V3C_UNIT_TYPE type);
    template <V3C_UNIT_TYPE E>
    void write_header();

    uint8_t nal_unit_type_;
    uint8_t nal_layer_id_;
    uint8_t nal_temporal_id_;

    size_t size_;
    uint8_t * bitstream_;
  };

}