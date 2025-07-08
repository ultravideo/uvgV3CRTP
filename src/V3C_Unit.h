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
      V3C_Unit_Header(); //Not a valid header necessarily
      V3C_Unit_Header(const V3C_UNIT_TYPE type);
      V3C_Unit_Header(const char * const bitstream);

      template<typename T, typename ...Vuh, typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, V3C_Unit_Header>>>
      inline V3C_Unit_Header(T&& first_vuh, Vuh&& ...vuh) :
        V3C_Unit_Header(static_cast<uint8_t>(std::forward<T>(first_vuh)),
                        std::forward_as_tuple(std::forward<Vuh>(vuh)...),
                                              std::index_sequence_for<Vuh...>{}) {
      }
      template<typename VuhTuple, std::size_t ...I>
      inline V3C_Unit_Header(const uint8_t vuh_unit_type, VuhTuple && vuh_t, std::index_sequence<I...>) :
        vuh_unit_type(vuh_unit_type),
        vuh_v3c_parameter_set_id(std::get<0>(vuh_t)),
        type(vuh_to_type(vuh_unit_type))
      {
        static_assert(std::tuple_size<std::decay_t<VuhTuple>>::value == sizeof...(I), "Tuple size mismatch");
        if constexpr (sizeof...(I) > 1)
        {
          if (type != V3C_CAD)
          {
            vuh_atlas_id = std::get<1>(vuh_t);

            if constexpr (sizeof...(I) > 3)
            {
              if (type == V3C_GVD)
              {
                vuh_map_index = std::get<2>(vuh_t);
                vuh_auxiliary_video_flag = std::get<3>(vuh_t);
              }

              if constexpr (sizeof...(I) > 5)
              {
                if (type == V3C_AVD)
                {
                  vuh_attribute_index = std::get<2>(vuh_t);
                  vuh_attribute_partition_index = std::get<3>(vuh_t);

                  vuh_map_index = std::get<4>(vuh_t);
                  vuh_auxiliary_video_flag = std::get<5>(vuh_t);
                }
              }
            }
          }
        }
      }

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
    // size_precision template needed to disambiguate from other constructor
    template<typename Header, typename T, typename = typename std::enable_if_t<std::is_same_v<T, uint8_t>>>
    V3C_Unit(Header&& header, const T size_precision) :
      header_(std::forward<Header>(header)),
      payload_(size_precision, get_sample_stream_header_size())
    {
    }
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

  protected:

    //friend std::unique_ptr<char[]> Sample_Stream<SAMPLE_STREAM_TYPE::V3C>::get_bitstream();
    friend Sample_Stream<SAMPLE_STREAM_TYPE::V3C>;
    size_t write_bitstream(char* const bitstream) const;

  private:
    size_t get_sample_stream_header_size() const;
    uint8_t parse_precision(const char * const bitstream) const;

    const V3C_Unit_Header header_;
    Sample_Stream<SAMPLE_STREAM_TYPE::NAL> payload_;

  };

  // Define specialization for template function so code is generated when built as a library
  //template <>
  //size_t V3C_Unit::size<V3C_VPS>() const;
  template <>
  size_t V3C_Unit::size<V3C_UNDEF>() const;

  //extern template V3C_Unit::V3C_Unit<V3C_Unit::V3C_Unit_Header,uint8_t>(V3C_Unit::V3C_Unit_Header&& header, const uint8_t size_precision, const size_t generic_unit_size);
  //extern template V3C_Unit::V3C_Unit<V3C_Unit::V3C_Unit_Header&, uint8_t>(V3C_Unit::V3C_Unit_Header& header, const uint8_t size_precision, const size_t generic_unit_size);
}