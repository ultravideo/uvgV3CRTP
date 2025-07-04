#pragma once

#include "v3crtplib/global.h"

#include <memory>
#include <type_traits>
#include <iostream>
#include <limits>

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

    V3C_State(INIT_FLAGS flags = INIT_FLAGS::ALL, const char* endpoint_address = "127.0.0.1", uint16_t port = 8890);
    V3C_State(const uint8_t size_precision, INIT_FLAGS flags = INIT_FLAGS::ALL, const char* endpoint_address = "127.0.0.1", uint16_t port = 8890);
    V3C_State(const char* bitstream, size_t len, INIT_FLAGS flags = INIT_FLAGS::ALL, const char* endpoint_address = "127.0.0.1", uint16_t port = 8890);
    ~V3C_State();

    // (Caller responsible for freeing char*)
    char* get_bitstream(size_t* length = nullptr) const;
    char* get_bitstream_cur_gof(size_t* length = nullptr) const;
    char* get_bitstream_cur_gof_unit(const V3C_UNIT_TYPE type, size_t* length = nullptr) const;

    void next_gof();

    void init_sample_stream(const uint8_t size_precision);
    void init_sample_stream(const char* bitstream, size_t len);

    // Functions for bitstream info writing (Caller responsible for freeing char*)
    char* get_bitstream_info_string(size_t* out_len, const INFO_FMT fmt = INFO_FMT::LOGGING) const;
    char* get_cur_gof_info_string(size_t* out_len, const INFO_FMT fmt = INFO_FMT::LOGGING) const;
    char* get_cur_gof_info_string(size_t* out_len, const V3C_UNIT_TYPE type, const INFO_FMT fmt = INFO_FMT::LOGGING) const;

    // Print current state (Sample stream etc.) to cout
    void print_state(const bool print_nalus = false, size_t num_gofs = std::numeric_limits<size_t>::max()) const;
    void print_bitstream_info(const INFO_FMT fmt = INFO_FMT::LOGGING) const;
    void print_cur_gof_info(const INFO_FMT fmt = INFO_FMT::LOGGING) const;
    void print_cur_gof_info(const V3C_UNIT_TYPE type, const INFO_FMT fmt = INFO_FMT::LOGGING) const;

  private:

    friend void send_bitstream(V3C_State<V3C_Sender>* state);
    friend void send_gof(V3C_State<V3C_Sender>* state);
    friend void send_unit(V3C_State<V3C_Sender>* state, V3C_UNIT_TYPE type);
    friend void receive_bitstream(V3C_State<V3C_Receiver>* state, const uint8_t v3c_size_precision, const uint8_t size_precisions[NUM_V3C_UNIT_TYPES], const size_t expected_num_gofs, const size_t num_nalus[NUM_V3C_UNIT_TYPES], const HeaderStruct header_defs[NUM_V3C_UNIT_TYPES], int timeout);
    friend void receive_gof(V3C_State<V3C_Receiver>* state, const uint8_t size_precisions[NUM_V3C_UNIT_TYPES], const size_t num_nalus[NUM_V3C_UNIT_TYPES], const HeaderStruct header_defs[NUM_V3C_UNIT_TYPES], int timeout);
    friend void receive_unit(V3C_State<V3C_Receiver>* state, const V3C_UNIT_TYPE unit_type, const uint8_t size_precision, const size_t expected_size, const HeaderStruct header_def, int timeout);

    void init_connection(INIT_FLAGS flags, const char* endpoint_address, uint16_t port);
    void init_cur_gof();

    T* connection_;

    Sample_Stream<SAMPLE_STREAM_TYPE::V3C>* data_;
    void* cur_gof_it_;
  };



  // Functions that operate on V3C_State
  void send_bitstream(V3C_State<V3C_Sender>* state);
  void send_gof(V3C_State<V3C_Sender>* state);
  void send_unit(V3C_State<V3C_Sender>* state, V3C_UNIT_TYPE type);

  void receive_bitstream(V3C_State<V3C_Receiver>* state, const uint8_t size_precision, const uint8_t size_precisions[NUM_V3C_UNIT_TYPES], const size_t expected_num_gofs, const size_t num_nalus[NUM_V3C_UNIT_TYPES], const HeaderStruct header_defs[NUM_V3C_UNIT_TYPES], int timeout);
  void receive_gof(V3C_State<V3C_Receiver>* state, const uint8_t size_precisions[NUM_V3C_UNIT_TYPES], const size_t num_nalus[NUM_V3C_UNIT_TYPES], const HeaderStruct header_defs[NUM_V3C_UNIT_TYPES], int timeout);
  void receive_unit(V3C_State<V3C_Receiver>* state, const V3C_UNIT_TYPE unit_type, const uint8_t size_precision, const size_t expected_size, const HeaderStruct header_def, int timeout);

  // Explicitly define necessary instantiations so code is linked properly
  extern template class V3C_State<V3C_Sender>;
  extern template class V3C_State<V3C_Receiver>;
}