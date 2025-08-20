#include "V3C_Unit.h"
#include "V3C.h"
#include "Sample_Stream.h"

#include <cassert>
#include <stdexcept>

namespace uvgV3CRTP {


  //template<typename Header, typename T, typename>
  //V3C_Unit::V3C_Unit(Header&& header, const T size_precision, const size_t generic_unit_size):
  //  header_(std::forward<Header>(header)),
  //  payload_(size_precision),
  //  generic_payload_size_(type() != V3C_VPS ? 0 : (generic_unit_size - header_.size()))
  //{
  //  generic_payload_ = std::make_unique<char[]>(generic_payload_size_);
  //}

  V3C_Unit::V3C_Unit(const char * const bitstream, const size_t len, const uint32_t timestamp) :
    header_(bitstream),
    payload_(parse_precision(&bitstream[header_.size()]), get_sample_stream_header_size()),
    timestamp_(timestamp)
  {
    if (type() == V3C_VPS) {
      // Parameter set contains no NAL units, but repurpose NAL for the VPS payload anyway
      Nalu vps_nalu(0, 0, 0, &bitstream[header_.size()], len - header_.size(), type());
      payload_.push_back(std::move(vps_nalu));
    }
    else
    {
      const uint8_t nal_size_precision = payload_.size_precision();
      const size_t sample_stream_hdr_offset = payload_.header_size;

      // Rest of the function goes inside the V3C unit payload and parses it into NAL units
      // Now start to parse the NAL sample stream
      for (size_t ptr = sample_stream_hdr_offset + header_.size(); ptr < len;) {

        size_t nal_size = V3C::parse_sample_stream_size(&bitstream[ptr], nal_size_precision);
        ptr += nal_size_precision;

        Nalu new_nalu(&bitstream[ptr], nal_size, type());
        ptr += nal_size;
        payload_.push_back(std::move(new_nalu));
      }
    }
  }

  /*+
    + ---------------------------------------------------------------- +
    + 
    + x1 bytes of whole V3C VPS unit(incl.header)
    + ---------------------------------------------------------------- +
    Atlas V3C unit
    + 
    + 4 bytes of V3C header
    + 1 byte of NAL Unit Size Precision(x1)
    + NALs count(x1 bytes of NAL Unit Size
      + x2 bytes of NAL unit payload)
    + ---------------------------------------------------------------- +
    Video V3C unit
    + 
    + 4 bytes of V3C header
    + NALs count(4 bytes of NAL Unit Size
      + x2 bytes of NAL unit payload)
    + ---------------------------------------------------------------- +
    .
    .
    .
    + ---------------------------------------------------------------- +
    Video V3C unit
    + 
    + 4 bytes of V3C header
    + NALs count(4 bytes of NAL Unit Size
      + x2 bytes of NAL unit payload)
    + ---------------------------------------------------------------- + */

  size_t V3C_Unit::get_sample_stream_header_size() const
  {
    return V3C::sample_stream_header_size<SAMPLE_STREAM_TYPE::NAL>(type());
  }

  uint8_t V3C_Unit::parse_precision(const char * const bitstream) const
  {
    if (type() == V3C_VPS) {
      return 0;
    }
    uint8_t nal_size_precision = 0;
    if (type() == V3C_AD || type() == V3C_CAD) {
      nal_size_precision = V3C::parse_size_precision(bitstream);
    } else {
      nal_size_precision = DEFAULT_VIDEO_NAL_SIZE_PRECISION;
    }
    if (!(0 < nal_size_precision && nal_size_precision <= 8))
    {
      throw std::runtime_error("sample stream precision should be in range [1,8]");
    }

    return nal_size_precision;
  }

  //template <>
  //size_t V3C_Unit::size<V3C_VPS>() const
  //{
  //  return header_.size() + generic_payload_size_;
  //}

  template <>
  uint64_t V3C_Unit::size<V3C_UNDEF>() const
  {
    throw std::invalid_argument("Not a valid unit type");
    //return 0;
  }

  template <V3C_UNIT_TYPE E>
  size_t V3C_Unit::size() const
  {
    return header_.size() + payload_.size();
  }

  size_t V3C_Unit::size() const
  {
    switch (type())
    {
    case V3C_VPS:
      return size<V3C_VPS>();

    case V3C_AD:
      return size<V3C_AD>();

    case V3C_OVD:
      return size<V3C_OVD>();

    case V3C_GVD:
      return size<V3C_GVD>();

    case V3C_AVD:
      return size<V3C_AVD>();

    case V3C_PVD:
      return size<V3C_PVD>();

    case V3C_CAD:
      return size<V3C_CAD>();

    default:
      return size<V3C_UNDEF>();
    }
  }

  uint8_t V3C_Unit::nal_size_precision() const
  {
    return payload_.size_precision();
  }

  V3C_Unit::nalu_ref_list V3C_Unit::nalus() const
  {
    // Populate nalu refs
    nalu_ref_list nalu_refs = {};
    for (const auto& nalu : payload_)
    {
      nalu_refs.push_back(std::ref(nalu));
    }
    return nalu_refs;
  }

  size_t V3C_Unit::num_nalus() const
  {
    return payload_.num_samples();
  }

  void V3C_Unit::push_back(Nalu && nalu)
  {
    if (payload_.num_samples() == 0 && timestamp_ == 0)
    {
      // If timestamp is not set and this is the first nalu, set the v3c units timestamp to the nalus timestamp
      timestamp_ = nalu.get_timestamp();
    }
    // Check that the nalu timestamp matches v3c units timestamp, if not this nalu does not belong to this v3c unit
    if (timestamp_ != 0 && nalu.get_timestamp() != timestamp_)
    {
      throw TimestampException("Nalu timestamp does not match V3C unit timestamp");
    }
    payload_.push_back(std::move(nalu));
  }

  size_t V3C_Unit::write_bitstream(char * const bitstream) const
  {
    size_t ptr = 0;
    ptr += header_.write_header(&bitstream[ptr]);
    ptr += payload_.write_bitstream(&bitstream[ptr]);

    return ptr;
  }


  V3C_Unit::V3C_Unit_Header::V3C_Unit_Header() : V3C_Unit_Header(V3C_VPS)
  {
  }

  V3C_Unit::V3C_Unit_Header::V3C_Unit_Header(const V3C_UNIT_TYPE type): vuh_unit_type(type_to_vuh(type)), type(type)
  {
  }

  V3C_Unit::V3C_Unit_Header::V3C_Unit_Header(const char * const bitstream):
    vuh_unit_type((bitstream[0] & 0b11111000) >> 3),
    type(vuh_to_type(vuh_unit_type))
  {
    vuh_v3c_parameter_set_id = 0;
    vuh_atlas_id = 0;

    if (type == V3C_AVD || type == V3C_GVD ||
        type == V3C_OVD || type == V3C_AD ||
        type == V3C_CAD || type == V3C_PVD) {

      // 3 last bits from first byte and 1 first bit from second byte
      vuh_v3c_parameter_set_id = ((bitstream[0] & 0b111) << 1) | ((bitstream[1] & 0b10000000) >> 7);
    }
    if (type == V3C_AVD || type == V3C_GVD ||
        type == V3C_OVD || type == V3C_AD ||
        type == V3C_PVD) {

      // 6 middle bits from the second byte
      vuh_atlas_id = ((bitstream[1] & 0b01111110) >> 1);
    }

    switch (type) {

    case V3C_GVD:
      // last bit of second byte and 3 first bytes of third byte
      vuh_map_index = ((bitstream[1] & 0b1) << 3) | ((bitstream[2] & 0b11100000) >> 5);
      // fourth bit of third byte
      vuh_auxiliary_video_flag = (bitstream[2] & 0b00010000) >> 4;
      break;

    case V3C_AVD:
      // last bit of second byte and 6 first bytes of third byte
      vuh_attribute_index = ((bitstream[1] & 0b1) << 6) | ((bitstream[2] & 0b11111100) >> 2);
      // 2 last bits of third byte and 3 first bits of fourth byte
      vuh_attribute_partition_index = (bitstream[2] & 0b11) | ((bitstream[3] & 0b11100000) >> 5);
      // fourth byte: 4 bits
      vuh_map_index = (bitstream[3] & 0b00011110) >> 1;
      // last bit of fourth byte
      vuh_auxiliary_video_flag = (bitstream[3] & 0b1);
      break;

    default:
      break;
    }
    return;
  }

  size_t V3C_Unit::V3C_Unit_Header::write_header(char * const bitstream) const
  {
    uint8_t hdr[V3C_HDR_LEN] = {};

    if (type != V3C_VPS)
    {
      // All V3C unit types have parameter_set_id in header
      hdr[0] = (vuh_unit_type << 3) | ((vuh_v3c_parameter_set_id & 0b1110) >> 1);
      hdr[1] = ((vuh_v3c_parameter_set_id & 0b1) << 7);

      // Only CAD does NOT have atlas_id
      if (type != V3C_CAD) {
        hdr[1] = hdr[1] | ((vuh_atlas_id & 0b111111) << 1);
      }
      // GVD has map_index and aux_video_flag, then zeroes
      if (type == V3C_GVD) {
        hdr[1] = hdr[1] | ((vuh_map_index & 0b1000) >> 3);
        hdr[2] = ((vuh_map_index & 0b111) << 5) | (static_cast<int>(vuh_auxiliary_video_flag) << 4);
      }
      else if (type == V3C_AVD) {
        hdr[1] = hdr[1] | ((vuh_attribute_index & 0b1000000) >> 7);
        hdr[2] = ((vuh_attribute_index & 0b111111) << 2) | ((vuh_attribute_partition_index & 0b11000) >> 3);
        hdr[3] = ((vuh_attribute_partition_index & 0b111) << 5) | (vuh_map_index << 1) | static_cast<int>(vuh_auxiliary_video_flag);
      }
    }
    else
    {
      //VPS only has vuh_unit_type field
      hdr[0] = (vuh_unit_type << 3);
    }

    // Copy V3C header to outbut buffer
    memcpy(bitstream, hdr, size());

    return size();
  }

  V3C_UNIT_TYPE V3C_Unit::V3C_Unit_Header::vuh_to_type(const uint8_t vuh_unit_type)
  {
    switch (vuh_unit_type)
    {
    case 0:
      return V3C_VPS;
    case 1:
      return V3C_AD;
    case 2:
      return V3C_OVD;
    case 3:
      return V3C_GVD;
    case 4:
      return V3C_AVD;
    case 5:
      return V3C_PVD;
    case 6:
      return V3C_CAD;
    default:
      throw std::invalid_argument("Not a recognized unit type");
      //return V3C_UNDEF;
    }
  }

  uint8_t V3C_Unit::V3C_Unit_Header::type_to_vuh(const V3C_UNIT_TYPE type)
  {
    switch (type)
    {
    case V3C_VPS:
      return 0;
    case V3C_AD:
      return 1;
    case V3C_OVD:
      return 2;
    case V3C_GVD:
      return 3;
    case V3C_AVD:
      return 4;
    case V3C_PVD:
      return 5;
    case V3C_CAD:
      return 6;
    default:
      throw std::invalid_argument("Not a valid unit type");
      //return static_cast<uint8_t>(-1);
    }
  }

  // Define specialization for template function so code is generated when built as a library
  //template V3C_Unit::V3C_Unit<V3C_Unit::V3C_Unit_Header, uint8_t>(V3C_Unit::V3C_Unit_Header&& header, const uint8_t size_precision, const size_t generic_unit_size);
  //template V3C_Unit::V3C_Unit<V3C_Unit::V3C_Unit_Header&, uint8_t>(V3C_Unit::V3C_Unit_Header& header, const uint8_t size_precision, const size_t generic_unit_size);
}