#pragma once

#include <cstdint>
#include <cstddef>

namespace uvgV3CRTP {

  // vuh_unit_type definitions:
  enum V3C_UNIT_TYPE {
    V3C_VPS = 0, // V3C parameter set
    V3C_AD = 1, // Atlas data
    V3C_OVD = 2, // Occupancy video data
    V3C_GVD = 3, // Geometry video data
    V3C_AVD = 4, // Attribute video data
    V3C_PVD = 5, // Packed video data 
    V3C_CAD = 6, // Common atlas data
    NUM_V3C_UNIT_TYPES, // Number of valid types
    V3C_UNDEF = -1 // Unit type not defined
  };

  enum CODEC {
    CODEC_AVC = 0, // AVC Progressive High
    CODEC_HEVC_MAIN10 = 1, // HEVC Main10
    CODEC_HEVC444 = 2, // HEVC444
    CODEC_VVC_MAIN10 = 3 // VVC Main10 
  };

  enum class INFO_FMT {
    LOGGING, // Logging printout in a human readable format
    PARAM,   // Print relevant parameters directly to c++ expressions
    RAW,     // Output data values directly
    BASE64,  // Base64 encoded data
    SDP      // SDP format
  };

  enum class INFO_FIELDS {
    NUM         , // s

    SIZE_PREC   , // ui8           
    
    VAR_NAL_PREC, // b    
    VAR_NAL_NUM   // b  
  };

  enum class HEADER_FIELDS {
    VUH_UNIT_TYPE,                  // ui8
    VUH_V3C_PARAMETER_SET_ID,       // ui8
    VUH_ATLAS_ID, // Not V3C_CAD    // ui8
    // Only V3C_GVD
    VUH_ATTRIBUTE_INDEX,            // ui8
    VUH_ATTRIBUTE_PARTITION_INDEX,  // ui8
    // Only V3C_GVD and V3C_AVD
    VUH_MAP_INDEX,                  // ui8
    VUH_AUXILIARY_VIDEO_FLAG,       // b
  };

  enum class PAYLOAD_FIELDS {
    PAYLOAD // std::string
  };

  union FIELD_TYPES
  {
    size_t s;
    uint8_t ui8;
    bool b;
  };

  // Api related structs
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

  struct BitstreamInfo {
    size_t num_gofs;
    size_t num_vps;
    size_t num_ad_nalu;
    size_t num_ovd_nalu;
    size_t num_gvd_nalu;
    size_t num_avd_nalu;
    size_t num_pvd_nalu;
    size_t num_cad_nalu;
    uint8_t v3c_size_precision;
    uint8_t video_nal_size_precision;
    uint8_t atlas_nal_size_precision;
    bool var_nal_prec;
    bool var_nal_num;
  };

  // V3C error state flags
  enum class ERROR_TYPE {
    OK = 0,

    // Critical
    GENERAL,  // Un-specified error
    CONNECTION, // Connection related error, most likely connections not initialized

    // Non-critical
    TIMEOUT,    // Receiving has timed out 
    TIMESTAMP,  // Timestamp related error, e.g. incorrect or unexpected timestamps 
    DATA,       // Data related error, most likely data not initilized
    EOS,        // End of stream reached, cur_gof points to end()
    INVALID_IT, // Current iterator is invalid, need to (re-)initialize it
    PARSE,      // Parsing error, e.g. parsing out-of-band info or bitstream failed
  };

  enum class SAMPLE_STREAM_TYPE {
    V3C,
    NAL
  };

  // Control what media streams are initialized
  enum class INIT_FLAGS : uint16_t {
    NUL = 0,
    VPS = 1,
    AD = 2,
    OVD = 4,
    GVD = 8,
    AVD = 16,
    PVD = 32,
    CAD = 64,

    ALL = 127,
  };
  inline INIT_FLAGS operator|(INIT_FLAGS a, INIT_FLAGS b) { return static_cast<INIT_FLAGS>(static_cast<uint16_t>(a) | static_cast<uint16_t>(b)); };
  inline INIT_FLAGS operator&(INIT_FLAGS a, INIT_FLAGS b) { return static_cast<INIT_FLAGS>(static_cast<uint16_t>(a) & static_cast<uint16_t>(b)); };
  inline bool is_set(INIT_FLAGS all_flags, INIT_FLAGS flag) { return (all_flags & flag) == flag; };

  // Define global constants
  static constexpr int V3C_HDR_LEN = 4; // 32 bits for v3c unit header
  static constexpr int SAMPLE_STREAM_HDR_LEN = 1; // 8 bits for sample stream headers


  static constexpr uint8_t DEFAULT_ATLAS_NAL_SIZE_PRECISION = 2;
  static constexpr uint8_t DEFAULT_VIDEO_NAL_SIZE_PRECISION = 4;

  static constexpr uint8_t MAX_V3C_SIZE_PREC = 8;
  static constexpr size_t SIZE_PREC_MULT = 8;

  static constexpr uint8_t NAL_UNIT_HEADER_SIZE = 2;

  // RTP clock for video increments 90000 in RTP payload format RFC
  constexpr uint32_t RTP_CLOCK_RATE = 90000;
  constexpr uint32_t DEFAULT_FRAME_RATE = 25;
  constexpr uint32_t SEND_FRAME_RATE = 30; // Limit rate for sending when using send_bitstream

  // Max number of rtp frames the v3c receiver stores before dropping frames. Mostly frames with incorrect timestamps are buffered e.g. frames getting re-ordered or dropped. Large value may slow down processing.
  constexpr size_t RECEIVE_BUFFER_SIZE = 1000;//50000;
}