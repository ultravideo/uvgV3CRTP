#pragma once

#include "v3crtplib/global.h"
#include "Nalu.h"
#include "Sample_Stream.h"

#include <vector>
#include <functional>
#include <tuple>
#include <utility>
#include <type_traits>

namespace v3cRTPLib {

  class V3C_Unit
  {
  public:

    class V3C_Unit_Header {
    public:
      V3C_Unit_Header(const V3C_UNIT_TYPE type);
      V3C_Unit_Header(const char * const bitstream);

      template <typename... Vuh>
      V3C_Unit_Header(Vuh&&... vuh);
      template <typename VuhTuple, std::size_t... I>
      V3C_Unit_Header(VuhTuple&& vuh_t, std::index_sequence<I...>);

      ~V3C_Unit_Header() = default;

      const uint8_t vuh_unit_type;
      uint8_t vuh_v3c_parameter_set_id;
      uint8_t vuh_atlas_id; // Not V3C_CAD

      // Only V3C_GVD
      uint8_t vuh_attribute_index;
      uint8_t vuh_attribute_partition_index;

      // Only V3C_GVD and V3C_AVD
      uint8_t vuh_map_index;
      bool vuh_auxiliary_video_flag;


      const V3C_UNIT_TYPE type;

      size_t write_header(char * const bitstream) const;

      size_t size() const { return V3C_HDR_LEN; };
      static V3C_UNIT_TYPE vuh_to_type(const uint8_t vuh_unit_type);
      static uint8_t type_to_vuh(const V3C_UNIT_TYPE type);
    };

    //V3C_Unit(const V3C_Unit_Header& header, uint8_t size_precision);
    template<typename Header, typename T, typename = typename std::enable_if<std::is_same<T, uint8_t>::value>::type>
    V3C_Unit(Header&& header, const T size_precision, const size_t generic_unit_size = 0);
    V3C_Unit(const char * const bitstream, const size_t len);

    V3C_Unit(const V3C_Unit&) = delete;
    V3C_Unit& operator=(const V3C_Unit&) = delete;

    V3C_Unit(V3C_Unit&&) = default;
    V3C_Unit& operator=(V3C_Unit&&) = default;

    ~V3C_Unit() = default;

    V3C_UNIT_TYPE type() const { return header_.type; }
    
    const V3C_Unit_Header& header() const { return header_; }


    // Get sizes of different v3c unit types
    size_t size() const;
    template <V3C_UNIT_TYPE E>
    size_t size() const;
    
    uint8_t nal_size_precision() const;

    using nalu_ref_list = std::vector<std::reference_wrapper<const Nalu>>;
    nalu_ref_list nalus() const;
    size_t num_nalus() const;

    void push_back(Nalu&& nalu);
    void push_back(const char* const generic_payload);

  protected:

    //friend std::unique_ptr<char[]> Sample_Stream<SAMPLE_STREAM_TYPE::V3C>::get_bitstream();
    friend Sample_Stream<SAMPLE_STREAM_TYPE::V3C>;
    size_t write_bitstream(char* const bitstream) const;

  private:

    uint8_t parse_precision(const char * const bitstream) const;

    const V3C_Unit_Header header_;
    Sample_Stream<SAMPLE_STREAM_TYPE::NAL> payload_;
    const size_t generic_payload_size_;
    std::unique_ptr<char[]> generic_payload_;

    nalu_ref_list nalu_refs_;

  };

  // Define specialization for template function
  template <>
  size_t V3C_Unit::size<V3C_VPS>() const;
  template <>
  size_t V3C_Unit::size<V3C_UNDEF>() const;
}