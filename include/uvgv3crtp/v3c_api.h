#pragma once

#include "uvgv3crtp/global.h"

#include <memory>
#include <type_traits>
#include <iostream>
#include <limits>

namespace uvgV3CRTP {

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

    // For initing data stream
    ERROR_TYPE init_sample_stream(const uint8_t size_precision);
    ERROR_TYPE init_sample_stream(const char* bitstream, size_t len);
    // Append to sample stream. If has_sample_stream_headers is true, the bitstream should contain sample stream headers, otherwise bitstream should contain one V3C unit of size len
    ERROR_TYPE append_to_sample_stream(const char* bitstream, size_t len, bool has_sample_stream_headers = false);

    void clear_sample_stream(); // Clear sample stream data, i.e. delete data_ and cur_gof_it_

    // (Caller responsible for freeing char*)
    char* get_bitstream(size_t* length = nullptr) const;
    char* get_bitstream_cur_gof(size_t* length = nullptr) const;
    char* get_bitstream_cur_gof_unit(const V3C_UNIT_TYPE type, size_t* length = nullptr) const;

    // Functions for manipulating current gof pointer of the state
    ERROR_TYPE first_gof();// Resets cur gof iterator
    ERROR_TYPE last_gof(); // Resets cur gof iterator
    ERROR_TYPE gof_at(size_t i); // Resets cur gof iterator
    ERROR_TYPE next_gof();
    ERROR_TYPE prev_gof();

    // Checking if cur gof has the specified unit type unit in it. If errors occur set error flag
    bool cur_gof_has_unit(V3C_UNIT_TYPE unit) const;
    bool cur_gof_is_full() const; // Check if gof has all units specified during state creation

    // Functions for bitstream info writing (Caller responsible for freeing char*). If errors occur return nullptr and set error flag 
    char* get_bitstream_info_string(size_t* out_len, const INFO_FMT fmt = INFO_FMT::LOGGING) const;
    char* get_cur_gof_info_string(size_t* out_len, const INFO_FMT fmt = INFO_FMT::LOGGING) const;
    char* get_cur_gof_info_string(size_t* out_len, const V3C_UNIT_TYPE type, const INFO_FMT fmt = INFO_FMT::LOGGING) const;

    // Print current state (Sample stream etc.) to cout. If errors occur set error flag
    ERROR_TYPE print_state(const bool print_nalus = false, size_t num_gofs = std::numeric_limits<size_t>::max()) const;
    void print_bitstream_info(const INFO_FMT fmt = INFO_FMT::LOGGING) const;
    void print_cur_gof_info(const INFO_FMT fmt = INFO_FMT::LOGGING) const;
    void print_cur_gof_info(const V3C_UNIT_TYPE type, const INFO_FMT fmt = INFO_FMT::LOGGING) const;

    // Error reporting
    ERROR_TYPE get_error_flag() const;
    const char* get_error_msg() const;
    void reset_error_flag() const;

  private:

    friend ERROR_TYPE send_bitstream(V3C_State<V3C_Sender>* state);
    friend ERROR_TYPE send_gof(V3C_State<V3C_Sender>* state);
    friend ERROR_TYPE send_unit(V3C_State<V3C_Sender>* state, V3C_UNIT_TYPE type);
    friend ERROR_TYPE receive_bitstream(V3C_State<V3C_Receiver>* state, const uint8_t v3c_size_precision, const uint8_t size_precisions[NUM_V3C_UNIT_TYPES], const size_t expected_num_gofs, const size_t num_nalus[NUM_V3C_UNIT_TYPES], const HeaderStruct header_defs[NUM_V3C_UNIT_TYPES], int timeout);
    friend ERROR_TYPE receive_gof(V3C_State<V3C_Receiver>* state, const uint8_t size_precisions[NUM_V3C_UNIT_TYPES], const size_t num_nalus[NUM_V3C_UNIT_TYPES], const HeaderStruct header_defs[NUM_V3C_UNIT_TYPES], int timeout);
    friend ERROR_TYPE receive_unit(V3C_State<V3C_Receiver>* state, const V3C_UNIT_TYPE unit_type, const uint8_t size_precision, const size_t expected_size, const HeaderStruct header_def, int timeout);

    void init_connection(INIT_FLAGS flags, const char* endpoint_address, uint16_t port);
    ERROR_TYPE init_cur_gof(size_t to = 0, bool reverse = false);

    T* connection_;
    const INIT_FLAGS flags_;

    Sample_Stream<SAMPLE_STREAM_TYPE::V3C>* data_;
    void* cur_gof_it_;
    bool is_gof_it_valid_;

    ERROR_TYPE set_error(ERROR_TYPE error, std::string msg) const;
    mutable ERROR_TYPE error_;
    mutable std::string error_msg_;

    bool validate_data() const;
    bool validate_nodata() const;
    bool validate_cur_gof(bool check_eos = true) const;
  };



  // Functions that operate on V3C_State
  ERROR_TYPE send_bitstream(V3C_State<V3C_Sender>* state);
  ERROR_TYPE send_gof(V3C_State<V3C_Sender>* state);
  ERROR_TYPE send_unit(V3C_State<V3C_Sender>* state, V3C_UNIT_TYPE type);

  ERROR_TYPE receive_bitstream(V3C_State<V3C_Receiver>* state, const uint8_t size_precision, const uint8_t size_precisions[NUM_V3C_UNIT_TYPES], const size_t expected_num_gofs, const size_t num_nalus[NUM_V3C_UNIT_TYPES], const HeaderStruct header_defs[NUM_V3C_UNIT_TYPES], int timeout);
  ERROR_TYPE receive_gof(V3C_State<V3C_Receiver>* state, const uint8_t size_precisions[NUM_V3C_UNIT_TYPES], const size_t num_nalus[NUM_V3C_UNIT_TYPES], const HeaderStruct header_defs[NUM_V3C_UNIT_TYPES], int timeout);
  ERROR_TYPE receive_unit(V3C_State<V3C_Receiver>* state, const V3C_UNIT_TYPE unit_type, const uint8_t size_precision, const size_t expected_size, const HeaderStruct header_def, int timeout);

  // Explicitly define necessary instantiations so code is linked properly
  extern template class V3C_State<V3C_Sender>;
  extern template class V3C_State<V3C_Receiver>;

}