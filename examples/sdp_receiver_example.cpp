#include <uvgv3crtp/version.h>
#include <uvgv3crtp/v3c_api.h>

#include <iostream>
#include <fstream>
#include <bitset>
#include <cstdlib>
#include <chrono>

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
constexpr bool AUTO_PRECISION_MODE = true;
constexpr bool AUTO_EXPECTED_NUM_MODE = true;
constexpr int VPS_PERIOD = 1; // How often to insert VPS

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

  if (argc < 2) {
    std::cout << "Enter sdp file name as input parameters" << std::endl;
    return EXIT_FAILURE;
  }

  // ******** Initialize sample stream with default values or out_of_band info ***********
  //
  std::cout << "Initialize state... " << std::flush;
  uint16_t ports[uvgV3CRTP::NUM_V3C_UNIT_TYPES] = { 0, 8890, 8891, 8892, 8893, 0, 0 }; // Use separate ports
  uvgV3CRTP::V3C_State<uvgV3CRTP::V3C_Receiver> state(
    /*uvgV3CRTP::INIT_FLAGS::VPS |*/ // Send VPS out-of-band
    uvgV3CRTP::INIT_FLAGS::AD  |
    uvgV3CRTP::INIT_FLAGS::OVD |
    uvgV3CRTP::INIT_FLAGS::GVD |
    uvgV3CRTP::INIT_FLAGS::AVD,
    "127.0.0.1", ports //Receiver address and port
  ); // Create a new state in a receiver configuration

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
  uvgV3CRTP::HeaderStruct* header_ptrs[uvgV3CRTP::NUM_V3C_UNIT_TYPES] = {
    &header_defs[uvgV3CRTP::V3C_VPS],
    &header_defs[uvgV3CRTP::V3C_AD],
    &header_defs[uvgV3CRTP::V3C_OVD],
    &header_defs[uvgV3CRTP::V3C_GVD],
    &header_defs[uvgV3CRTP::V3C_AVD],
    &header_defs[uvgV3CRTP::V3C_PVD],
    &header_defs[uvgV3CRTP::V3C_CAD],
  };

  state.init_sample_stream(v3c_size_precision); //Init sample stream here since receive_gof() assumes data is initialized
  std::cout << "Done" << std::endl;

  std::cout << "Read SDP... " << std::flush;

  // Read Headers and VPS from sdp definition
  std::ifstream sdp(argv[2]);

  if (!sdp.is_open()) {
    std::cerr << "Failed to open SDP file" << std::endl;
    return EXIT_FAILURE;
  }

  //get length of file
  sdp.seekg(0, sdp.end);
  auto sdp_len = sdp.tellg();
  sdp.seekg(0, sdp.beg);

  /* Read the file and its size */
  if (sdp_len == 0) {
    std::cerr << "Empty file" << std::endl;
    return EXIT_FAILURE;
  }

  auto sdp_buf = std::make_unique<char[]>(sdp_len);
  if (sdp_buf == nullptr) return EXIT_FAILURE;

  // read into char*
  if (!(sdp.read(sdp_buf.get(), sdp_len))) // read up to the size of the buffer
  {
    if (!sdp.eof())
    {
      std::cerr << "Failed to SDP to buffer" << std::endl;
      sdp_buf = nullptr; // Release allocated memory before returning nullptr
      return EXIT_FAILURE;
    }
  }
  sdp.close();

  // Read headers
  state.parse_unit_info_string(sdp_buf.get(), sdp_len, header_ptrs, uvgV3CRTP::INFO_FMT::SDP, uvgV3CRTP::INFO_FMT::BASE64);

  // Read vps
  size_t vps_len = 0;
  auto vps = std::unique_ptr<char, decltype(&free)>(
    state.parse_unit_info_string(sdp_buf.get(), sdp_len, uvgV3CRTP::V3C_VPS, nullptr, &vps_len,
                                 uvgV3CRTP::INFO_FMT::NONE, uvgV3CRTP::INFO_FMT::NONE, 
                                 uvgV3CRTP::INFO_FMT::SDP, uvgV3CRTP::INFO_FMT::BASE64),
    &free
  );
  
  std::cout << "Done" << std::endl;

  //
  // ************************************************************************************

  // ******* Receive sample stream ********
  //
  std::cout << "Receiving bitstream... " << std::endl;
  size_t gof_num = 0;
  
  while (state.get_error_flag() == uvgV3CRTP::ERROR_TYPE::OK && expected_number_of_gof > 0)
  {
    if (gof_num > 0 && gof_num % VPS_PERIOD)
    {
      //Increment VPS id in headers when a new vps is expected
      for (auto& unit_header : header_defs)
      {
        unit_header.vuh_v3c_parameter_set_id += 1;
      }
    }

    std::cout << "  Receiving GoF..." << std::flush;
    uvgV3CRTP::receive_gof(&state, size_precisions, num_nalus, header_defs, TIMEOUT);

    // Insert VPS to the current gov here based on the VPS period 
    if (gof_num % VPS_PERIOD)
    {
      state.append_to_sample_stream(vps.get(), vps_len);
    }
    gof_num++;

    // Dont't stop receiving even if timestamp error occurs, just print the error
    if (state.get_error_flag() == uvgV3CRTP::ERROR_TYPE::TIMESTAMP) {
      std::cout << " Timestamp error: " << state.get_error_msg() << std::endl;
      state.reset_error_flag();
    }

    if (state.get_error_flag() == uvgV3CRTP::ERROR_TYPE::OK)
    {
      state.last_gof();
      std::cout << " Received:" << std::endl;
      state.print_cur_gof_bitstream_info();
    }
    else
    {
      std::cout << std::endl;
    }
    expected_number_of_gof -= 1;
  }

  std::cout << "Stopping receiving: " << state.get_error_msg() << std::endl;
  //
  // **************************************

  // If original file given, compare to received
  if (argc >= 4) {
    //read orig bitstream for comparison
    std::cout << "Compare to orig bitstream... " << std::flush;
    // Open file
    std::ifstream bitstream(argv[4], std::ios::in | std::ios::binary);

    if (!bitstream.is_open()) {
      std::cerr << "File open failed" << std::endl;
      return EXIT_FAILURE;
    }

    //get length of file
    bitstream.seekg(0, bitstream.end);
    auto length = bitstream.tellg();
    bitstream.seekg(0, bitstream.beg);

    /* Read the file and its size */
    if (length == 0) {
      std::cerr << "Empty file" << std::endl;
      return EXIT_FAILURE;
    }
    
    auto buf = std::make_unique<char[]>(length);
    if (buf == nullptr) return EXIT_FAILURE;

    // read into char*
    if (!(bitstream.read(buf.get(), length))) // read up to the size of the buffer
    {
      if (!bitstream.eof())
      {
        std::cerr << "Failed to read orig bitstream to buffer" << std::endl;
        buf = nullptr; // Release allocated memory before returning nullptr
        return EXIT_FAILURE;
      }
    }
    // Check that bitstream was correctly parsed
    compare_bitstreams(state, buf, length);

    std::cout << "Done" << std::endl;
  }

  // If out file given, write received bitstream to file
  if (argc >= 3)
  {
    std::cout << "Writing rec... " << std::flush;
    // Open file
    std::ofstream rec_out_stream(argv[2]);

    if (!rec_out_stream.is_open()) {
      std::cerr << "Failed to open rec out file" << std::endl;
      return EXIT_FAILURE;
    }

    size_t len = 0;
    auto out_bitstream = state.get_bitstream(&len);
    rec_out_stream.write(out_bitstream, len);
    free(out_bitstream);
    std::cout << "Done" << std::endl;
  }

  // ******** Print info about sample stream **********
  //
  // Print state and bitstream info
  state.print_state(false);

  //std::cout << "Bitstream info: " << std::endl;
  state.print_bitstream_info();
  //
  // **************************************

  return EXIT_SUCCESS;
}