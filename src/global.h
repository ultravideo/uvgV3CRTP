#pragma once

#include <cstdint>

namespace v3cRTPLib {

  // vuh_unit_type definitions:
  enum V3C_UNIT_TYPE {
    V3C_VPS = 0, // V3C parameter set
    V3C_AD = 1, // Atlas data
    V3C_OVD = 2, // Occupancy video data
    V3C_GVD = 3, // Geometry video data
    V3C_AVD = 4, // Attribute video data
    V3C_PVD = 5, // Packed video data 
    V3C_CAD = 6, // Common atlas data
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
    PARAM    // Print relevant parameters directly to c++ expressions
  };
  enum class INFO_FIELDS {
    NUM_GOF,
    NUM_VPS_NALU,
    NUM_AD_NALU,
    NUM_OVD_NALU,
    NUM_GVD_NALU,
    NUM_AVD_NALU,
    NUM_CAD_NALU,
    NUM_PVD_NALU,

    V3C_SIZE_PREC,
    VIDEO_NAL_SIZE_PREC,
    ATLAS_NAL_SIZE_PREC,

    VAR_NAL_PREC,
    VAR_NAL_NUM
  };

  enum class SAMPLE_STREAM_TYPE {
    V3C,
    NAL
  };

  static constexpr int V3C_HDR_LEN = 4; // 32 bits for v3c unit header
  static constexpr int SAMPLE_STREAM_HDR_LEN = 1; // 8 bits for sample stream headers


  static constexpr uint8_t DEFAULT_ATLAS_NAL_SIZE_PRECISION = 2;
  static constexpr uint8_t DEFAULT_VIDEO_NAL_SIZE_PRECISION = 4;

  static constexpr uint8_t MAX_V3C_SIZE_PREC = 8;

  static constexpr uint8_t NAL_UNIT_HEADER_SIZE = 2;

}