#include <uvgv3crtp/version.h>
#include <uvgv3crtp/v3c_api.h>

#include <iostream>
#include <fstream>
#include <bitset>
#include <cstdlib>
#include <thread>

static void compare_bitstreams(uvgV3CRTP::V3C_State<uvgV3CRTP::V3C_Sender>& state, std::unique_ptr<char[]>& buf, size_t length)
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
  
  if (argc < 3) {
    std::cout << "Enter bitstream file name and sdp out file name as input parameters" << std::endl;
    return EXIT_FAILURE;
  }

  // ********************* Handle inpu reading ***********************
  //
  std::cout << "Reading input bitstream... " << std::flush;
  // Open file
  std::ifstream bitstream(argv[1], std::ios::in | std::ios::binary);

  if (!bitstream.is_open()) {
    return EXIT_FAILURE;
  }

  //get length of file
  bitstream.seekg(0, bitstream.end);
  size_t length = bitstream.tellg();
  bitstream.seekg(0, bitstream.beg);

  /* Read the file and its size */
  if (length == 0) {
    return EXIT_FAILURE; //TODO: Raise exception
  }
  std::cout << "Done" << std::endl;

  std::cout << "Reading file to buffer... " << std::flush;
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
  //
  // ******************************************************************

  // ******** Initialize sample stream with input bitstream ***********
  //
  std::cout << "Initialize state... " << std::flush;

  uint16_t ports[uvgV3CRTP::NUM_V3C_UNIT_TYPES] = { 0, 8890, 8891, 8892, 8893, 0, 0 }; // Use separate ports
  uvgV3CRTP::V3C_State<uvgV3CRTP::V3C_Sender> state(buf.get(), length,
    /*uvgV3CRTP::INIT_FLAGS::VPS |*/ // Send VPS out-of-band
    uvgV3CRTP::INIT_FLAGS::AD  |
    uvgV3CRTP::INIT_FLAGS::OVD |
    uvgV3CRTP::INIT_FLAGS::GVD |
    uvgV3CRTP::INIT_FLAGS::AVD,
    "127.0.0.1", ports //Receiver address and port
  ); // Create a new state in a sender configuration
  std::cout << "Done" << std::endl;
  //
  // ******************************************************************

  // Check that bitstream was correctly parsed
  compare_bitstreams(state, buf, length);

  // ******** Print info about sample stream **********
  //
  // Print state and bitstream info
  state.print_state(false);

  //std::cout << "Bitstream info: " << std::endl;
  state.print_bitstream_info();

  //size_t len = 0;
  //auto info = std::unique_ptr<char, decltype(&free)>(state.get_bitstream_info_string(&len, uvgV3CRTP::INFO_FMT::PARAM), &free);
  //std::cout << info.get() << std::endl;
  //
  // **************************************

  // ******** Send sample stream **********
  //
  std::cout << "Sending bitstream... " << std::endl;

  // Write ou-of-band info based on the first gof
  state.first_gof();
  std::cout << "Writing out-of-band info... " << std::flush;
  // Open file
  std::ofstream oob_stream(argv[2]);

  if (!oob_stream.is_open()) {
    return EXIT_FAILURE;
  }

  auto headers = state.get_cur_gof_unit_info_string(uvgV3CRTP::INFO_FMT::SDP, uvgV3CRTP::INFO_FMT::BASE64);
  if (headers)
  {
    oob_stream << headers;
    free(headers);
  }
  // Write VPS
  auto vps = state.get_cur_gof_unit_info_string(uvgV3CRTP::V3C_VPS, uvgV3CRTP::INFO_FMT::NONE, uvgV3CRTP::INFO_FMT::NONE,
                                                                    uvgV3CRTP::INFO_FMT::SDP, uvgV3CRTP::INFO_FMT::BASE64);
  if (vps)
  {
    oob_stream << vps;
    free(vps);
  }
  oob_stream.close();
  std::cout << " Done" << std::endl;

  // Wait for a bit so the receiver can be started before sending the bitstream
  std::cout << "Waiting for receiver to be started... (5 seconds)" << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(5));

  while (state.get_error_flag() == uvgV3CRTP::ERROR_TYPE::OK)
  {
    uvgV3CRTP::send_gof(&state);
    std::cout << "  GoF sent" << std::endl;
    state.next_gof();

    // Wait a bit between GoFs to not flood the receiver
    std::this_thread::sleep_for(std::chrono::milliseconds(1000 / uvgV3CRTP::SEND_FRAME_RATE));
  }

  std::cout << "Stopping sending: " << state.get_error_msg() << std::endl;
  //
  // **************************************

  return EXIT_SUCCESS;
}