#include <uvgv3crtp/version.h>
#include <uvgv3crtp/v3c_api.h>

#include <iostream>
#include <fstream>
#include <bitset>
#include <cstdlib>

constexpr int EXPECTED_NUM_GOFs = 10;
constexpr int EXPECTED_NUM_VPSs = 10;
constexpr int EXPECTED_NUM_AD_NALU = 35;
constexpr int EXPECTED_NUM_OVD_NALU = 35;
constexpr int EXPECTED_NUM_GVD_NALU = 131;
constexpr int EXPECTED_NUM_AVD_NALU = 131;
constexpr uint8_t V3C_SIZE_PRECISION = 3;
constexpr uint8_t AtlasNAL_SIZE_PRECISION = 2;
constexpr uint8_t Video_SIZE_PRECISION = 4;

constexpr int TIMEOUT = 6000;
// Auto size precision may not match orig bitstream
constexpr bool AUTO_PRECISION_MODE = false;
constexpr bool AUTO_EXPECTED_NUM_MODE = false;

constexpr uvgV3CRTP::INFO_FMT info_format = uvgV3CRTP::INFO_FMT::PARAM;//uvgV3CRTP::INFO_FMT::RAW; // Format for out-of-band info file

static void compare_bitstreams(uvgV3CRTP::V3C_State<uvgV3CRTP::V3C_Receiver>& state, std::unique_ptr<char[]>& buf, size_t length)
{
  std::cout << "Check bitstream was parsed correctly..." << std::endl;
  size_t rec_len = 0;
  auto rec = std::unique_ptr<char, decltype(&free)>(state.get_bitstream(&rec_len), &free); // Reconstruc bitstream from cur state
  bool diff = false;
  if (length != rec_len) {
    std::cout << "Error: Original bitstream size does not match reconstructed bitstream size: " << (int)length << " vs. " << (int)rec_len << std::endl;
    diff = true;
  }

  // Compare reconstructed file with the original one
  for (int i = 0; i < (length < rec_len ? length : rec_len); ++i) {
    if (buf[i] != rec.get()[i]) {
      diff = true;
      std::cout << "Difference found in " << i << std::endl;
      std::cout << "  orig byte: " << std::bitset<8>(buf[i]) << std::endl;
      std::cout << "   rec byte: " << std::bitset<8>(rec.get()[i]) << std::endl;

      break;
    }
  }
  if (!diff) {
    std::cout << "No differences found" << std::endl;
  }
}

int main(int argc, char* argv[]) {
  std::cout << "V3C RTP lib version: " << uvgV3CRTP::get_version() << std::endl;

  bool orig_available = false;
  bool out_of_band_available = false;
  std::unique_ptr<char[]> buf;
  std::unique_ptr<char[]> outofband_buf;
  size_t length = 0;
  size_t outofband_len = 0;
  if (argc >= 2) {
    //TODO: read orig bitstream for comparison
    std::cout << "Reading input bitstream... " << std::flush;
    // Open file
    std::ifstream bitstream(argv[1], std::ios::in | std::ios::binary);

    if (!bitstream.is_open()) {
      //TODO: Raise exception
      std::cerr << "File open failed";
      return EXIT_FAILURE;
    }

    //get length of file
    bitstream.seekg(0, bitstream.end);
    length = bitstream.tellg();
    bitstream.seekg(0, bitstream.beg);

    /* Read the file and its size */
    if (length == 0) {
      return EXIT_FAILURE; //TODO: Raise exception
    }
    std::cout << "Done" << std::endl;

    std::cout << "Reading file to buffer... " << std::flush;
    buf = std::make_unique<char[]>(length);
    if (buf == nullptr) return EXIT_FAILURE;//TODO: Raise exception

    // read into char*
    if (!(bitstream.read(buf.get(), length))) // read up to the size of the buffer
    {
      if (!bitstream.eof())
      {
        //TODO: Raise exception
        buf = nullptr; // Release allocated memory before returning nullptr
        return EXIT_FAILURE;
      }
    }
    std::cout << "Done" << std::endl;
    orig_available = true;
  }
  if (argc >= 3) {
    std::cout << "Reading out-of-band info... " << std::flush;
    // Open file
    std::ifstream outofband(argv[2], (info_format == uvgV3CRTP::INFO_FMT::RAW ? std::ios::in | std::ios::binary : std::ios::in));

    if (!outofband.is_open()) {
      std::cerr << std::endl << "File open failed. Make sure sender has been started and has written out-of-band info.";
      return EXIT_FAILURE;
    }

    //get length of file
    outofband.seekg(0, outofband.end);
    outofband_len = outofband.tellg();
    outofband.seekg(0, outofband.beg);

    /* Read the file and its size */
    if (outofband_len == 0) {
      std::cerr << std::endl << "File empty. Make sure sender has been started and has written out-of-band info.";
      return EXIT_FAILURE;
    }
    std::cout << "Done" << std::endl;

    std::cout << "Reading out of band info to buffer... " << std::flush;
    outofband_buf = std::make_unique<char[]>(outofband_len);
    if (outofband_buf == nullptr) return EXIT_FAILURE;//TODO: Raise exception

    // read into char*
    if (!(outofband.read(outofband_buf.get(), outofband_len))) // read up to the size of the buffer
    {
      if (!outofband.eof())
      {
        //TODO: Raise exception
        outofband_buf = nullptr; // Release allocated memory before returning nullptr
        return EXIT_FAILURE;
      }
    }
    std::cout << "Done" << std::endl;
    out_of_band_available = true;
  }

// ******** Initialize sample stream with default values or out_of_band info ***********
//
  std::cout << "Initialize state... " << std::flush;
  uvgV3CRTP::V3C_State<uvgV3CRTP::V3C_Receiver> state(
    uvgV3CRTP::INIT_FLAGS::VPS |
    uvgV3CRTP::INIT_FLAGS::AD  |
    uvgV3CRTP::INIT_FLAGS::OVD |
    uvgV3CRTP::INIT_FLAGS::GVD |
    uvgV3CRTP::INIT_FLAGS::AVD
  ); // Create a new state in a receiver configuration
  //state.init_sample_stream(v3c_size_precision); //Don't init sample stream here since receive_bistream() will create one
  std::cout << "Done" << std::endl;

  // Define necessary information for receiving a v3c stream
  uint8_t v3c_size_precision   = AUTO_PRECISION_MODE ? static_cast<uint8_t>(-1) : V3C_SIZE_PRECISION;
  uint8_t atlas_size_precision = AUTO_PRECISION_MODE ? static_cast<uint8_t>(-1) : AtlasNAL_SIZE_PRECISION;
  uint8_t video_size_precision = Video_SIZE_PRECISION;//AUTO_PRECISION_MODE ? static_cast<uint8_t>(-1) : Video_SIZE_PRECISION;
  
  size_t expected_number_of_gof = AUTO_EXPECTED_NUM_MODE ? static_cast<size_t>(-1) : EXPECTED_NUM_GOFs;
  size_t num_vps                = AUTO_EXPECTED_NUM_MODE ? static_cast<size_t>(-1) : EXPECTED_NUM_VPSs;
  size_t num_ad_nalu            = AUTO_EXPECTED_NUM_MODE ? static_cast<size_t>(-1) : EXPECTED_NUM_AD_NALU;
  size_t num_ovd_nalu           = AUTO_EXPECTED_NUM_MODE ? static_cast<size_t>(-1) : EXPECTED_NUM_OVD_NALU;
  size_t num_gvd_nalu           = AUTO_EXPECTED_NUM_MODE ? static_cast<size_t>(-1) : EXPECTED_NUM_GVD_NALU;
  size_t num_avd_nalu           = AUTO_EXPECTED_NUM_MODE ? static_cast<size_t>(-1) : EXPECTED_NUM_AVD_NALU;
  size_t num_pvd_nalu           = 0;
  size_t num_cad_nalu           = 0;

  if (out_of_band_available)
  {
    std::cout << "Parsing out-of-band info... " << std::flush;

    uvgV3CRTP::BitstreamInfo outofband_info = {};
    if (state.parse_bitstream_info_string(outofband_buf.get(), outofband_len, info_format, outofband_info) != uvgV3CRTP::ERROR_TYPE::OK)
    {
        std::cerr << std::endl << "Failed to parse out-of-band info:" << state.get_error_msg() << std::endl;
        return EXIT_FAILURE;
    }
    expected_number_of_gof = outofband_info.num_gofs;
    num_vps = outofband_info.num_vps;
    num_ad_nalu = outofband_info.num_ad_nalu;
    num_ovd_nalu = outofband_info.num_ovd_nalu;
    num_gvd_nalu = outofband_info.num_gvd_nalu;
    num_avd_nalu = outofband_info.num_avd_nalu;
    num_pvd_nalu = outofband_info.num_pvd_nalu;
    num_cad_nalu = outofband_info.num_cad_nalu;
    v3c_size_precision = outofband_info.v3c_size_precision;
    atlas_size_precision = outofband_info.atlas_nal_size_precision;
    video_size_precision = outofband_info.video_nal_size_precision;

    std::cout << "Done" << std::endl;
  }

  // Auto size precision if set to 0 (may not match orig bitstream)
  uint8_t size_precisions[uvgV3CRTP::NUM_V3C_UNIT_TYPES] = {
    0,
    atlas_size_precision,
    video_size_precision,
    video_size_precision,
    video_size_precision,
    video_size_precision,
    atlas_size_precision,
  };
  size_t num_nalus[uvgV3CRTP::NUM_V3C_UNIT_TYPES] = {
    num_vps,
    num_ad_nalu,
    num_ovd_nalu,
    num_gvd_nalu,
    num_avd_nalu,
    num_pvd_nalu,
    num_cad_nalu,
  };
  uvgV3CRTP::HeaderStruct header_defs[uvgV3CRTP::NUM_V3C_UNIT_TYPES] = {
    {uvgV3CRTP::V3C_VPS},
    {uvgV3CRTP::V3C_AD, 0, 0},
    {uvgV3CRTP::V3C_OVD, 0, 0},
    {uvgV3CRTP::V3C_GVD, 0, 0, 0, 0, 0, false},
    {uvgV3CRTP::V3C_AVD, 0, 0, 0, 0, 0, false},
    {uvgV3CRTP::V3C_PVD, 0, 0},
    {uvgV3CRTP::V3C_CAD, 0},
  };
//
// ************************************************************************************

// ******* Receive sample stream ********
//
  std::cout << "Receiving bitstream... " << std::flush;
  uvgV3CRTP::receive_bitstream(&state, v3c_size_precision, size_precisions, expected_number_of_gof, num_nalus, header_defs, TIMEOUT);

  if (state.get_error_flag() != uvgV3CRTP::ERROR_TYPE::OK) {
    std::cerr << "Error receiving bitstream: " << state.get_error_msg() << std::endl;
    return EXIT_FAILURE;
  }
  std::cout << "Done" << std::endl;
//
// **************************************

  if (orig_available)
  {
    // Check that bitstream was correctly parsed
    compare_bitstreams(state, buf, length);
  }

// ******** Print info about sample stream **********
//
  // Print state and bitstream info
  state.print_state(false);

  //std::cout << "Bitstream info: " << std::endl;
  state.print_bitstream_info();

  //size_t len = 0;
  //auto info = std::unique_ptr<char, decltype(&free)>(state.get_bitstream_info_string(&len, uvgV3CRTP::INFO_FMT::PARAM), &free);
  //std::cout << info.get() << std::endl;
   
  if(state.get_error_flag() != uvgV3CRTP::ERROR_TYPE::OK) {
    std::cerr << "Error printing state: " << state.get_error_msg() << std::endl;
    return EXIT_FAILURE;
  }
//
// **************************************

  return EXIT_SUCCESS;
}