#include "Nalu.h"

#include <stdexcept>

namespace v3cRTPLib {

  Nalu::Nalu(const uint8_t nal_unit_type, const uint8_t nal_layer_id, const uint8_t nal_temporal_id, const char * const payload, const size_t payload_len, const V3C_UNIT_TYPE type):
    nal_unit_type_(nal_unit_type),
    nal_layer_id_(nal_layer_id),
    nal_temporal_id_(nal_temporal_id)
  {
    init_bitstream(NAL_UNIT_HEADER_SIZE + payload_len);

    //Create bitstream
    write_header(type);
    memcpy(bitstream_.get() + NAL_UNIT_HEADER_SIZE, payload, payload_len);
  }

  Nalu::Nalu(const char * const bitstream, const size_t len, const V3C_UNIT_TYPE type)
  {
    init_bitstream(len);

    //Copy input bitstream
    memcpy(bitstream_.get(), bitstream, len);

    parse_header(type);
  }

  //Nalu::~Nalu()
  //{
  //  delete[] bitstream_;
  //}

  uint8_t * Nalu::bitstream() const
  {
    return bitstream_.get();
  }

  size_t Nalu::size() const
  {
    return size_;
  }

  uint8_t Nalu::nal_unit_type() const
  {
    return nal_unit_type_;
  }

  uint8_t Nalu::nal_layer_id() const
  {
    return nal_layer_id_;
  }

  uint8_t Nalu::nal_temporal_id() const
  {
    return nal_temporal_id_;
  }

  void Nalu::init_bitstream(const size_t len)
  {
    //Allocate memory for new bitstream
    size_ = len;
    bitstream_ = std::make_unique<uint8_t[]>(size_);
    memset(bitstream_.get(), 0, size_);
  }

  //Error if undef or VPS (does not have NAL)
  template <>
  void Nalu::parse_header<V3C_UNDEF>()
  {
    throw std::invalid_argument("Not a valid unit type");
  }
  template <>
  void Nalu::parse_header<V3C_VPS>()
  {
    throw std::invalid_argument("VPS should not have NALUs");
  }
  template <>
  void Nalu::write_header<V3C_UNDEF>()
  {
    throw std::invalid_argument("Not a valid unit type");
  }
  template <>
  void Nalu::write_header<V3C_VPS>()
  {
    throw std::invalid_argument("VPS should not have NALUs");
  }

  void Nalu::parse_header(const V3C_UNIT_TYPE type)
  {
    switch (type)
    {
    case v3cRTPLib::V3C_VPS:
      parse_header<V3C_VPS>();
      break;
    case v3cRTPLib::V3C_AD:
      parse_header<V3C_AD>();
      break;
    case v3cRTPLib::V3C_OVD:
      parse_header<V3C_OVD>();
      break;
    case v3cRTPLib::V3C_GVD:
      parse_header<V3C_GVD>();
      break;
    case v3cRTPLib::V3C_AVD:
      parse_header<V3C_AVD>();
      break;
    case v3cRTPLib::V3C_PVD:
      parse_header<V3C_PVD>();
      break;
    case v3cRTPLib::V3C_CAD:
      parse_header<V3C_CAD>();
      break;
    default:
      parse_header<V3C_UNDEF>();
      break;
    }
  }

  void Nalu::write_header(const V3C_UNIT_TYPE type)
  {
    switch (type)
    {
    case v3cRTPLib::V3C_VPS:
      write_header<V3C_VPS>();
      break;
    case v3cRTPLib::V3C_AD:
      write_header<V3C_AD>();
      break;
    case v3cRTPLib::V3C_OVD:
      write_header<V3C_OVD>();
      break;
    case v3cRTPLib::V3C_GVD:
      write_header<V3C_GVD>();
      break;
    case v3cRTPLib::V3C_AVD:
      write_header<V3C_AVD>();
      break;
    case v3cRTPLib::V3C_PVD:
      write_header<V3C_PVD>();
      break;
    case v3cRTPLib::V3C_CAD:
      write_header<V3C_CAD>();
      break;
    default:
      write_header<V3C_UNDEF>();
      break;
    }
  }

  // Nal header:
  //  nal_forbidden_zero_bit f(1) 
  //  nal_unit_type          u(6)
  //  nal_layer_id           u(6)
  //  nal_temporal_id_plus1  u(3)

  template <V3C_UNIT_TYPE E>
  void Nalu::parse_header()
  {
    nal_unit_type_ = ((0b01111110 & bitstream_[0]) >> 1);
    nal_layer_id_ = ((0b10000000 & bitstream_[0]) >> 2) | ((0b11111000 & bitstream_[1]) >> 3);
    nal_temporal_id_ = (0b00000111 & bitstream_[1]);
  }

  template <V3C_UNIT_TYPE E>
  void Nalu::write_header()
  {
    char h_byte1 = ((0b00111111 & nal_unit_type_) << 1) | ((0b00100000 & nal_layer_id_) >> 5);
    char h_byte2 = ((0b00011111 & nal_layer_id_) << 3) | (0b00000111 & nal_temporal_id_);

    bitstream_[0] = h_byte1;
    bitstream_[1] = h_byte2;
  }

}