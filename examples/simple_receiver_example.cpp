#include <v3crtplib/version.h>
#include <v3crtplib/v3c_api.h>

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

constexpr int TIMEOUT = 5000;
constexpr bool AUTO_PRECISION_MODE = false;


static void compare_bitstreams(v3cRTPLib::V3C_State<v3cRTPLib::V3C_Receiver>& state, std::unique_ptr<char[]>& buf, size_t length)
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
  std::cout << "V3C RTP lib version: " << v3cRTPLib::get_version() << std::endl;

  bool orig_available = false;
  bool out_of_band_available = false;
  std::unique_ptr<char[]> buf;
  size_t length = 0;
  if (argc >= 2) {
    //TODO: read orig bitstream for comparison
    std::cout << "Reading input bitstream... ";
    // Open file
    std::ifstream bitstream(argv[1]);

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

    std::cout << "Reading file to buffer... ";
    auto buf = std::make_unique<char[]>(length);
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
    //TODO: read out-of-band data
    //std::cout << "Reading input bitstream... ";
    //// Open file
    //std::ifstream bitstream(argv[1]);

    //if (!bitstream.is_open()) {
    //  //TODO: Raise exception
    //  return EXIT_FAILURE;
    //}

    ////get length of file
    //bitstream.seekg(0, bitstream.end);
    //size_t length = bitstream.tellg();
    //bitstream.seekg(0, bitstream.beg);

    ///* Read the file and its size */
    //if (length == 0) {
    //  return EXIT_FAILURE; //TODO: Raise exception
    //}
    //std::cout << "Done" << std::endl;

    //std::cout << "Reading file to buffer... ";
    //auto buf = std::make_unique<char[]>(length);
    //if (buf == nullptr) return EXIT_FAILURE;//TODO: Raise exception

    //// read into char*
    //if (!(bitstream.read(buf.get(), length))) // read up to the size of the buffer
    //{
    //  if (!bitstream.eof())
    //  {
    //    //TODO: Raise exception
    //    buf = nullptr; // Release allocated memory before returning nullptr
    //    return EXIT_FAILURE;
    //  }
    //}
    //std::cout << "Done" << std::endl;
    out_of_band_available = true;
  }

  // ******** Initialize sample stream with default values or out_of_band info ***********
  //
  std::cout << "Initialize state... ";
  v3cRTPLib::V3C_State<v3cRTPLib::V3C_Receiver> state(
    v3cRTPLib::INIT_FLAGS::VPS |
    v3cRTPLib::INIT_FLAGS::AD |
    v3cRTPLib::INIT_FLAGS::OVD |
    v3cRTPLib::INIT_FLAGS::GVD |
    v3cRTPLib::INIT_FLAGS::AVD
  ); // Create a new state in a receiver configuration
  //state.init_sample_stream(v3c_size_precision); //Don't init sample stream here since receive_bistream() will create one
  std::cout << "Done" << std::endl;

  // Define necessary information for receiving a v3c stream
  uint8_t v3c_size_precision = AUTO_PRECISION_MODE ? 0 : V3C_SIZE_PRECISION;
  uint8_t atlas_size_precision = AUTO_PRECISION_MODE ? 0 : AtlasNAL_SIZE_PRECISION;
  uint8_t video_size_precision = AUTO_PRECISION_MODE ? 0 : Video_SIZE_PRECISION;
  
  size_t expected_number_of_gof = EXPECTED_NUM_GOFs;
  size_t num_vps = EXPECTED_NUM_VPSs;
  size_t num_ad_nalu = EXPECTED_NUM_AD_NALU;
  size_t num_ovd_nalu = EXPECTED_NUM_OVD_NALU;
  size_t num_gvd_nalu = EXPECTED_NUM_GVD_NALU;
  size_t num_avd_nalu = EXPECTED_NUM_AVD_NALU;
  size_t num_pvd_nalu = 0;
  size_t num_cad_nalu = 0;

  if (out_of_band_available)
  {
    //TODO: set outofband info
  }

  uint8_t size_precisions[v3cRTPLib::NUM_V3C_UNIT_TYPES] = {
    0,
    atlas_size_precision,
    video_size_precision,
    video_size_precision,
    video_size_precision,
    video_size_precision,
    atlas_size_precision,
  };
  size_t num_nalus[v3cRTPLib::NUM_V3C_UNIT_TYPES] = {
    num_vps,
    num_ad_nalu,
    num_ovd_nalu,
    num_gvd_nalu,
    num_avd_nalu,
    num_pvd_nalu,
    num_cad_nalu,
  };
  v3cRTPLib::HeaderStruct header_defs[v3cRTPLib::NUM_V3C_UNIT_TYPES] = {
    {v3cRTPLib::V3C_VPS},
    {v3cRTPLib::V3C_AD},
    {v3cRTPLib::V3C_OVD},
    {v3cRTPLib::V3C_GVD},
    {v3cRTPLib::V3C_AVD},
    {v3cRTPLib::V3C_PVD},
    {v3cRTPLib::V3C_CAD},
  };
  //
  // ************************************************************************************

  // ******* Receive sample stream ********
  //
  std::cout << "Receiving bitstream... ";
  v3cRTPLib::receive_bitstream(&state, v3c_size_precision, size_precisions, expected_number_of_gof, num_nalus, header_defs, TIMEOUT);
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
  //auto info = std::unique_ptr<char, decltype(&free)>(state.get_bitstream_info_string(&len, v3cRTPLib::INFO_FMT::PARAM), &free);
  //std::cout << info.get() << std::endl;
  //
  // **************************************

  return EXIT_SUCCESS;
}