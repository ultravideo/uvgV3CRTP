#pragma once

#include "uvgv3crtp/global.h"

#include <memory>
#include <type_traits>
#include <iostream>
#include <limits>

/// \file Api for using the library in an application.

namespace uvgV3CRTP {

  //Forward declaration
  class V3C_Sender;
  class V3C_Receiver;
  template <SAMPLE_STREAM_TYPE E>
  class Sample_Stream;

  template <typename T>
  class V3C_State {
  public:

    /**
     * @brief Construct a V3C_State object without initializing sample stream.
     * @param flags Initialization flags. Which unit types to expect.
     * @param endpoint_address Address to connect/bind to.
     * @param port Port number.
     */
    V3C_State(INIT_FLAGS flags = INIT_FLAGS::ALL, const char* endpoint_address = "127.0.0.1", uint16_t port = 8890) noexcept;

    /**
     * @brief Construct a V3C_State object and initialize the sample stream with a given size precision.
     * @param size_precision Size precision for the sample stream. Auto infer if (uint8_t)-1.
     * @param flags Initialization flags. Which unit types to expect.
     * @param endpoint_address Address to connect/bind to.
     * @param port Port number.
     */
    V3C_State(const uint8_t size_precision, INIT_FLAGS flags = INIT_FLAGS::ALL, const char* endpoint_address = "127.0.0.1", uint16_t port = 8890) noexcept;

    /**
     * @brief Construct a V3C_State object and initialize the sample stream from a bitstream.
     * @details bitstream must contain sample stream headers to parse correctly.
     * @param bitstream Pointer to the bitstream data.
     * @param len Length of the bitstream.
     * @param flags Initialization flags. Which unit types to expect.
     * @param endpoint_address Address to connect/bind to.
     * @param port Port number.
     */
    V3C_State(const char* bitstream, size_t len, INIT_FLAGS flags = INIT_FLAGS::ALL, const char* endpoint_address = "127.0.0.1", uint16_t port = 8890) noexcept;

    /**
     * @brief Destructor. Cleans up connections and sample stream data.
     */
    ~V3C_State() noexcept;

    /**
     * @brief Initialize the sample stream with a given size precision.
     * @details Sample stream must not be initialized or should be cleared before calling this function.
     * @param size_precision Size precision for the sample stream. Auto infer if (uint8_t)-1.
     * @return ERROR_TYPE::OK on success, error code otherwise.
     */
    ERROR_TYPE init_sample_stream(const uint8_t size_precision) noexcept;

    /**
     * @brief Initialize the sample stream from a bitstream.
     * @details Sample stream must not be initialized or should be cleared before calling this function. bitstream must contain sample stream headers to parse correctly.
     * @param bitstream Pointer to the bitstream data.
     * @param len Length of the bitstream.
     * @return ERROR_TYPE::OK on success, error code otherwise.
     */
    ERROR_TYPE init_sample_stream(const char* bitstream, size_t len) noexcept;

    /**
     * @brief Append data to the sample stream.
     * @details Sample stream must be initialized before calling this function. If has_sample_stream_headers is true, bitstream must contain sample stream headers to parse correctly. Only one V3C unit is parsed when has_sample_stream_headers is false.
     * @param bitstream Pointer to the bitstream data.
     * @param len Length of the bitstream.
     * @param has_sample_stream_headers If true, bitstream contains sample stream headers; otherwise, a single V3C unit.
     * @return ERROR_TYPE::OK on success, error code otherwise.
     */
    ERROR_TYPE append_to_sample_stream(const char* bitstream, size_t len, bool has_sample_stream_headers = false) noexcept;

    /**
     * @brief Clear the sample stream data and reset state.
     * @details Last timestamp is saved and used to derive the initial timestamp if more data is appended. This should allow continuous timestamps when sending data in parts.
     */
    void clear_sample_stream() noexcept;

    /**
     * @brief Get the full bitstream.
     * @details Sample stream must be initialized and contain data. If not, nullptr is returned and error flag is set. Returned bitstream contains sample stream headers.
     * @param length pointer to store the length of the returned bitstream.
     * @return Pointer to the bitstream (caller must free i.e. c-style free).
     */
    char* get_bitstream(size_t* length) const noexcept;

    /**
     * @brief Get the bitstream for the current GoF.
     * @details Sample stream must be initialized and contain data and cur gof iterator should be valid. If not, nullptr is returned and error flag is set. Returned bitstream does not contains v3c sample stream headers.
     * @param length pointer to store the length of the returned bitstream.
     * @return Pointer to the bitstream (caller must free i.e. c-style free).
     */
    char* get_bitstream_cur_gof(size_t* length) const noexcept;

    /**
     * @brief Get the bitstream for a specific unit type in the current GoF.
     * @details Sample stream must be initialized and contain data and cur gof iterator should be valid. type also has to be a type present in the gof. If not, nullptr is returned and error flag is set. Returned bitstream does not contains v3c sample stream headers.
     * @param type The V3C unit type.
     * @param length pointer to store the length of the returned bitstream.
     * @return Pointer to the bitstream (caller must free i.e. c-style free).
     */
    char* get_bitstream_cur_gof_unit(const V3C_UNIT_TYPE type, size_t* length) const noexcept;

    /**
     * @brief Reset the current GoF iterator to the first GoF.
     * @return ERROR_TYPE::OK on success, error code otherwise.
     */
    ERROR_TYPE first_gof() noexcept;

    /**
     * @brief Reset the current GoF iterator to the last GoF.
     * @return ERROR_TYPE::OK on success, error code otherwise.
     */
    ERROR_TYPE last_gof() noexcept;

    /**
     * @brief Set the current GoF iterator to the i-th GoF.
     * @details If i is out of range, error flag is set to INVALID_IT and iterator is not changed.
     * @param i Index of the GoF.
     * @return ERROR_TYPE::OK on success, error code otherwise.
     */
    ERROR_TYPE gof_at(size_t i) noexcept;

    /**
    * @brief Get the index of the current GoF.
    * @details If the iterator is not valid, 0 is returned and error flag is set to INVALID_IT or DATA if no data exists.
    * @return Index of the current GoF, or 0 if the iterator is not valid.
    */
   size_t cur_gof_ind() const noexcept;
    
    /**
     * @brief Get the total number of GoFs in the sample stream.
     * @details If the sample stream is not initialized, 0 is returned and error flag is set to DATA.
     * @return Total number of GoFs, or 0 if the sample stream is not initialized.
     */
    size_t num_gofs() const noexcept;

    /**
     * @brief Advance the current GoF iterator to the next GoF.
     * @details Sets error flag to EOS if the end of the stream is reached.
     * @return ERROR_TYPE::OK on success, error code otherwise.
     */
    ERROR_TYPE next_gof() noexcept;

    /**
     * @brief Move the current GoF iterator to the previous GoF.
     * @return ERROR_TYPE::OK on success, error code otherwise.
     */
    ERROR_TYPE prev_gof() noexcept;

    /**
     * @brief Check if the current GoF contains a unit of the specified type.
     * @details cur gof iterator should be valid. If not, error flag is set to INVALID_IT or DATA if no data exists.
     * @param unit The V3C unit type.
     * @return True if the unit exists, false otherwise.
     */
    bool cur_gof_has_unit(V3C_UNIT_TYPE unit) const noexcept;

    /**
     * @brief Check if the current GoF contains all units specified during state creation.
     * @details cur gof iterator should be valid. If not, error flag is set to INVALID_IT or DATA if no data exists.
     * @return True if the GoF is full, false otherwise.
     */
    bool cur_gof_is_full() const noexcept;

    /**
     * @brief Get a null-terminated string with information about the bitstream.
     * @details Sample stream must be initialized and contain data. If not, nullptr is returned and error flag is set.
     * @param out_len Optional pointer to store the length of the returned string.
     * @param fmt Output format.
     * @return Pointer to the info string (caller must free i.e. c-style free).
     */
    char* get_bitstream_info_string(const INFO_FMT fmt = INFO_FMT::LOGGING, size_t* out_len = nullptr) const noexcept;

    /**
     * @brief Get a null-terminated string with information about the current GoF.
     * @details Sample stream must be initialized and contain data and cur gof iterator should be valid. If not, nullptr is returned and error flag is set.
     * @param out_len Optional pointer to store the length of the returned string.
     * @param fmt Output format.
     * @return Pointer to the info string (caller must free i.e. c-style free).
     */
    char* get_cur_gof_bitstream_info_string(const INFO_FMT fmt = INFO_FMT::LOGGING, size_t* out_len = nullptr) const noexcept;

    /**
     * @brief Get a null-terminated string with information about a specific unit type in the current GoF.
     * @details Sample stream must be initialized and contain data and cur gof iterator should be valid. type also has to be a type present in the gof. If not, nullptr is returned and error flag is set.
     * @param out_len Optional pointer to store the length of the returned string.
     * @param type The V3C unit type.
     * @param fmt Output format.
     * @return Pointer to the info string (caller must free i.e. c-style free).
     */
    char* get_cur_gof_bitstream_info_string(const V3C_UNIT_TYPE type, const INFO_FMT fmt = INFO_FMT::LOGGING, size_t* out_len = nullptr) const noexcept;


    char* get_cur_gof_unit_info_string(const INFO_FMT field_fmt = INFO_FMT::LOGGING, const INFO_FMT value_fmt = INFO_FMT::NONE, size_t* out_len = nullptr) const noexcept;
    char* get_cur_gof_unit_info_string(const V3C_UNIT_TYPE type, const INFO_FMT header_field_fmt = INFO_FMT::LOGGING, const INFO_FMT header_value_fmt = INFO_FMT::NONE, const INFO_FMT payload_field_fmt = INFO_FMT::NONE, const INFO_FMT payload_value_fmt = INFO_FMT::NONE, size_t* out_len = nullptr) const noexcept;


    /**
     * @brief Parse out-of-band info from a bitstream info string.
     * @param in_data Pointer to the input data.
     * @param len Length of the input data.
     * @param fmt Input format.
     * @param out_info Output structure to fill with parsed info.
     * @return ERROR_TYPE::OK on success, error code otherwise.
     */
    ERROR_TYPE parse_bitstream_info_string(const char* const in_data, size_t len, INFO_FMT fmt, BitstreamInfo& out_info) noexcept;

    /**
     * @brief Print the current state (sample stream, etc.) to stdout.
     * @details Sample stream must be initialized and contain data and cur gof iterator should be valid. If not, error flag is set and no output is printed.
     * @param print_nalus If true, print NALU information as well.
     * @param num_gofs Maximum number of GoFs to print.
     * @return ERROR_TYPE::OK on success, error code otherwise.
     */
    ERROR_TYPE print_state(const bool print_nalus = false, size_t num_gofs = std::numeric_limits<size_t>::max()) const noexcept;

    /**
     * @brief Print information about the bitstream to stdout.
     * @details Sample stream must be initialized and contain data. If not, error flag is set and no output is printed.
     * @param fmt Output format.
     */
    void print_bitstream_info(const INFO_FMT fmt = INFO_FMT::LOGGING) const noexcept;

    /**
     * @brief Print information about the current GoF to stdout.
     * @details Sample stream must be initialized and contain data and cur gof iterator should be valid. If not, error flag is set and no output is printed.
     * @param fmt Output format.
     */
    void print_cur_gof_bitstream_info(const INFO_FMT fmt = INFO_FMT::LOGGING) const noexcept;

    /**
     * @brief Print information about a specific unit type in the current GoF to stdout.
     * @details Sample stream must be initialized and contain data and cur gof iterator should be valid. type also has to be a type present in the gof. If not, error flag is set and no output is printed.
     * @param type The V3C unit type.
     * @param fmt Output format.
     */
    void print_cur_gof_bitstream_info(const V3C_UNIT_TYPE type, const INFO_FMT fmt = INFO_FMT::LOGGING) const noexcept;
    
    void print_cur_gof_unit_info(const INFO_FMT field_fmt = INFO_FMT::LOGGING, const INFO_FMT value_fmt = INFO_FMT::NONE) const noexcept;
    void print_cur_gof_unit_info(const V3C_UNIT_TYPE type, const INFO_FMT header_field_fmt = INFO_FMT::LOGGING, const INFO_FMT header_value_fmt = INFO_FMT::NONE, const INFO_FMT payload_field_fmt = INFO_FMT::NONE, const INFO_FMT payload_value_fmt = INFO_FMT::NONE) const noexcept;

    /**
     * @brief Get the current error flag.
     * @return The current error code.
     */
    ERROR_TYPE get_error_flag() const noexcept;

    /**
     * @brief Get the current error message.
     * @return The current error message string (should not be freed by caller).
     */
    const char* get_error_msg() const noexcept;

    /**
     * @brief Reset the error flag and message.
     */
    void reset_error_flag() const noexcept;

  private:
    /// \cond DO_NOT_DOCUMENT
    friend ERROR_TYPE send_bitstream(V3C_State<V3C_Sender>* state) noexcept;
    friend ERROR_TYPE send_gof(V3C_State<V3C_Sender>* state) noexcept;
    friend ERROR_TYPE send_unit(V3C_State<V3C_Sender>* state, V3C_UNIT_TYPE type) noexcept;
    friend ERROR_TYPE receive_bitstream(V3C_State<V3C_Receiver>* state, const uint8_t v3c_size_precision, const uint8_t size_precisions[NUM_V3C_UNIT_TYPES], const size_t expected_num_gofs, const size_t num_nalus[NUM_V3C_UNIT_TYPES], const HeaderStruct header_defs[NUM_V3C_UNIT_TYPES], int timeout) noexcept;
    friend ERROR_TYPE receive_gof(V3C_State<V3C_Receiver>* state, const uint8_t size_precisions[NUM_V3C_UNIT_TYPES], const size_t num_nalus[NUM_V3C_UNIT_TYPES], const HeaderStruct header_defs[NUM_V3C_UNIT_TYPES], int timeout) noexcept;
    friend ERROR_TYPE receive_unit(V3C_State<V3C_Receiver>* state, const V3C_UNIT_TYPE unit_type, const uint8_t size_precision, const size_t expected_size, const HeaderStruct header_def, int timeout) noexcept;

    void init_connection(INIT_FLAGS flags, const char* endpoint_address, uint16_t port) noexcept;
    ERROR_TYPE init_cur_gof(size_t to = 0, bool reverse = false) noexcept;
    void set_timestamps(const uint32_t init_timestamp) noexcept;

    T* connection_;
    const INIT_FLAGS flags_;

    Sample_Stream<SAMPLE_STREAM_TYPE::V3C>* data_;
    void* cur_gof_it_;
    bool is_gof_it_valid_;
    size_t cur_gof_ind_;

    ERROR_TYPE set_error(ERROR_TYPE error, std::string msg) const noexcept;
    mutable ERROR_TYPE error_;
    mutable std::string error_msg_;

    // Helpers for validation
    bool validate_data() const noexcept;
    bool validate_nodata() const noexcept;
    bool validate_cur_gof(bool check_eos = true) const noexcept;
    /// \endcond
  };

  /**
   * @brief Send the entire bitstream using the sender state.
   * @details Sends all GoFs in the sample stream using the associated V3C_Sender connection.
   *          The sample stream must be initialized and contain data.
   *          Only unit types specified during state creation are sent.
   *          Sending is rate limited to SEND_FRAME_RATE
   * @param state Pointer to the V3C_State<V3C_Sender> object.
   * @return ERROR_TYPE::OK on success, error code otherwise.
   */
  ERROR_TYPE send_bitstream(V3C_State<V3C_Sender>* state) noexcept;

  /**
   * @brief Send the current GoF using the sender state.
   * @details Sends the GoF pointed to by the current GoF iterator using the associated V3C_Sender connection.
   *          The sample stream and current GoF iterator must be valid.
   *          Only unit types specified during state creation are sent.
   * @param state Pointer to the V3C_State<V3C_Sender> object.
   * @return ERROR_TYPE::OK on success, error code otherwise.
   */
  ERROR_TYPE send_gof(V3C_State<V3C_Sender>* state) noexcept;

  /**
   * @brief Send a specific V3C unit from the current GoF using the sender state.
   * @details Sends the specified V3C unit from the current GoF using the associated V3C_Sender connection.
   *          The sample stream and current GoF iterator must be valid.
   *          Only unit types specified during state creation are sent.
   * @param state Pointer to the V3C_State<V3C_Sender> object.
   * @param type The V3C unit type to send.
   * @return ERROR_TYPE::OK on success, error code otherwise.
   */
  ERROR_TYPE send_unit(V3C_State<V3C_Sender>* state, V3C_UNIT_TYPE type) noexcept;

  /**
   * @brief Receive a full bitstream using the receiver state.
   * @details Receives a complete sample stream from the associated V3C_Receiver connection.
   *          The state must not already contain data. Timeout error is set if expected number of GoFs is not received within the timeout period.
   *          Only unit types specified during state creation are received.
   * @param state Pointer to the V3C_State<V3C_Receiver> object.
   * @param v3c_size_precision Size precision for the V3C sample stream. Auto infer if (uint8_t)-1.
   * @param size_precisions Array of size precisions for each V3C unit type. Auto infer if (uint8_t)-1.
   * @param expected_num_gofs Expected number of GoFs to receive.
   * @param num_nalus Array of expected number of NALUs for each V3C unit type. Auto infer if (size_t)-1.
   * @param header_defs Array of header definitions for each V3C unit type.
   * @param timeout Timeout in milliseconds for the receive operation.
   * @return ERROR_TYPE::OK on success, error code otherwise.
   */
  ERROR_TYPE receive_bitstream(
      V3C_State<V3C_Receiver>* state,
      const uint8_t v3c_size_precision,
      const uint8_t size_precisions[NUM_V3C_UNIT_TYPES],
      const size_t expected_num_gofs,
      const size_t num_nalus[NUM_V3C_UNIT_TYPES],
      const HeaderStruct header_defs[NUM_V3C_UNIT_TYPES],
      int timeout
  ) noexcept;

  /**
   * @brief Receive a single GoF using the receiver state.
   * @details Receives a single GoF from the associated V3C_Receiver connection and appends it to the sample stream.
   *          The sample stream must be initialized.
   *          Only unit types specified during state creation are received.
   * @param state Pointer to the V3C_State<V3C_Receiver> object.
   * @param size_precisions Array of size precisions for each V3C unit type. Auto infer if (uint8_t)-1.
   * @param num_nalus Array of expected number of NALUs for each V3C unit type. Auto infer if (size_t)-1.
   * @param header_defs Array of header definitions for each V3C unit type.
   * @param timeout Timeout in milliseconds for the receive operation.
   * @return ERROR_TYPE::OK on success, error code otherwise.
   */
  ERROR_TYPE receive_gof(
      V3C_State<V3C_Receiver>* state,
      const uint8_t size_precisions[NUM_V3C_UNIT_TYPES],
      const size_t num_nalus[NUM_V3C_UNIT_TYPES],
      const HeaderStruct header_defs[NUM_V3C_UNIT_TYPES],
      int timeout
  ) noexcept;

  /**
   * @brief Receive a single V3C unit using the receiver state.
   * @details Receives a single V3C unit from the associated V3C_Receiver connection and appends it to the sample stream.
   *          The sample stream must be initialized.
   *          CONNECTION_ERROR is set if the unit type has not been initialized.
   * @param state Pointer to the V3C_State<V3C_Receiver> object.
   * @param unit_type The V3C unit type to receive.
   * @param size_precision Size precision for the V3C unit. Auto infer if (uint8_t)-1.
   * @param expected_size Expected size or number of NALUs for the unit. Auto infer if (size_t)-1.
   * @param header_def Header definition for the V3C unit.
   * @param timeout Timeout in milliseconds for the receive operation.
   * @return ERROR_TYPE::OK on success, error code otherwise.
   */
  ERROR_TYPE receive_unit(
      V3C_State<V3C_Receiver>* state,
      const V3C_UNIT_TYPE unit_type,
      const uint8_t size_precision,
      const size_t expected_size,
      const HeaderStruct header_def,
      int timeout
  ) noexcept;

  // Explicitly define necessary instantiations so code is linked properly
  extern template class V3C_State<V3C_Sender>;
  extern template class V3C_State<V3C_Receiver>;

}