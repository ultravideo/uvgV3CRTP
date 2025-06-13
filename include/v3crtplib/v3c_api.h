#pragma once

#include "v3crtplib/global.h"

#include <memory>
#include <type_traits>
#include <iostream>

namespace v3cRTPLib {

  //Forward declaration
  class V3C_Sender;
  class V3C_Receiver;
  template <SAMPLE_STREAM_TYPE E>
  class Sample_Stream;

  struct HeaderStruct {
    const uint8_t vuh_unit_type;
    uint8_t vuh_v3c_parameter_set_id;
    uint8_t vuh_atlas_id; // Not V3C_CAD

    // Only V3C_GVD
    uint8_t vuh_attribute_index;
    uint8_t vuh_attribute_partition_index;

    // Only V3C_GVD and V3C_AVD
    uint8_t vuh_map_index;
    bool vuh_auxiliary_video_flag;
  };

  template <typename T>
  class V3C_State {
  public:

    V3C_State(const char* dst_address = "127.0.0.1", const char* src_address = "127.0.0.1", INIT_FLAGS flags = INIT_FLAGS::ALL);
    V3C_State(const uint8_t size_precision, const char* dst_address = "127.0.0.1", const char* src_address = "127.0.0.1", INIT_FLAGS flags = INIT_FLAGS::ALL);
    V3C_State(const char* bitstream, size_t len, const char* dst_address = "127.0.0.1", const char* src_address = "127.0.0.1", INIT_FLAGS flags = INIT_FLAGS::ALL);
    ~V3C_State();

    std::unique_ptr<char[]> get_bitstream() const;
    std::unique_ptr<char[]> get_bitstream_cur_gof() const;
    std::unique_ptr<char[]> get_bitstream_cur_gof_unit(V3C_UNIT_TYPE type) const;

    void next_gof();

    void init_sample_stream(const uint8_t size_precision);
    void init_sample_stream(const char* bitstream, size_t len);

    // Functions for bitstream info writing (Caller responsible for freeing char*)
    char* write_bitstream_info(size_t& out_len, INFO_FMT fmt = INFO_FMT::LOGGING);
    char* write_cur_gof_info(size_t& out_len, INFO_FMT fmt = INFO_FMT::LOGGING);
    char* write_cur_gof_info(size_t& out_len, V3C_UNIT_TYPE type, INFO_FMT fmt = INFO_FMT::LOGGING);

  private:

    friend void send_bitstream(V3C_State<V3C_Sender>& state);
    friend void send_gof(V3C_State<V3C_Sender>& state);
    friend void send_unit(V3C_State<V3C_Sender>& state, V3C_UNIT_TYPE type);
    friend void receive_bitstream(V3C_State<V3C_Receiver>& state, const uint8_t v3c_size_precision, const uint8_t size_precisions[NUM_V3C_UNIT_TYPES], const size_t expected_num_gofs, const size_t num_nalus[NUM_V3C_UNIT_TYPES], const HeaderStruct header_defs[NUM_V3C_UNIT_TYPES], int timeout);
    friend void receive_gof(V3C_State<V3C_Receiver>& state, const uint8_t size_precisions[NUM_V3C_UNIT_TYPES], const size_t num_nalus[NUM_V3C_UNIT_TYPES], const HeaderStruct header_defs[NUM_V3C_UNIT_TYPES], int timeout);
    friend void receive_unit(V3C_State<V3C_Receiver>& state, const V3C_UNIT_TYPE unit_type, const uint8_t size_precision, const size_t expected_size, const HeaderStruct& header_def, int timeout);

    void init_connection(const char* dst_address, const char* src_address, INIT_FLAGS flags);
    void init_cur_gof();

    T* connection_;

    Sample_Stream<SAMPLE_STREAM_TYPE::V3C>* data_;
    void* cur_gof_it_;
  };



  // Functions that operate on V3C_State
  void send_bitstream(V3C_State<V3C_Sender>& state);
  void send_gof(V3C_State<V3C_Sender>& state);
  void send_unit(V3C_State<V3C_Sender>& state, V3C_UNIT_TYPE type);

  void receive_bitstream(V3C_State<V3C_Receiver>& state, const uint8_t size_precision, const uint8_t size_precisions[NUM_V3C_UNIT_TYPES], const size_t expected_num_gofs, const size_t num_nalus[NUM_V3C_UNIT_TYPES], const HeaderStruct header_defs[NUM_V3C_UNIT_TYPES], int timeout);
  void receive_gof(V3C_State<V3C_Receiver>& state, const uint8_t size_precisions[NUM_V3C_UNIT_TYPES], const size_t num_nalus[NUM_V3C_UNIT_TYPES], const HeaderStruct header_defs[NUM_V3C_UNIT_TYPES], int timeout);
  void receive_unit(V3C_State<V3C_Receiver>& state, const V3C_UNIT_TYPE unit_type, const uint8_t size_precision, const size_t expected_size, const HeaderStruct& header_def, int timeout);

  // Explicitly define necessary instantiations so code is linked properly
  extern template class V3C_State<V3C_Sender>;
  extern template class V3C_State<V3C_Receiver>;
}