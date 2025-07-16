#include <v3crtplib/version.h>
#include <v3crtplib/v3c_api.h>

#include <iostream>
#include <fstream>
#include <bitset>
#include <cstdlib>

static void compare_bitstreams(v3cRTPLib::V3C_State<v3cRTPLib::V3C_Sender>& state, std::unique_ptr<char[]>& buf, size_t length)
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
  
  if (argc < 2) {
    std::cout << "Enter bitstream file name as input parameter" << std::endl;
    return EXIT_FAILURE;
  }

  // ********************* Handle inpu reading ***********************
  //
  std::cout << "Reading input bitstream... ";
  // Open file
  std::ifstream bitstream(argv[1]);

  if (!bitstream.is_open()) {
    //TODO: Raise exception
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
  //
  // ******************************************************************

  // ******** Initialize sample stream with input bitstream ***********
  //
  std::cout << "Initialize state... ";
  v3cRTPLib::V3C_State<v3cRTPLib::V3C_Sender> state(buf.get(), length,
    v3cRTPLib::INIT_FLAGS::VPS |
    v3cRTPLib::INIT_FLAGS::AD  |
    v3cRTPLib::INIT_FLAGS::OVD |
    v3cRTPLib::INIT_FLAGS::GVD |
    v3cRTPLib::INIT_FLAGS::AVD,
    "127.0.0.1", 8890 //Receiver address and port
  ); // Create a new state in a sender configuration
  //state.init_sample_stream(buf.get(), length); // sample stream already initialized when creating state
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
  //auto info = std::unique_ptr<char, decltype(&free)>(state.get_bitstream_info_string(&len, v3cRTPLib::INFO_FMT::PARAM), &free);
  //std::cout << info.get() << std::endl;
  //
  // **************************************

  // ******** Send sample stream **********
  //
  std::cout << "Sending bitstream... " << std::endl;

  while (state.get_error_flag() == v3cRTPLib::ERROR_TYPE::OK)
  {
    // TODO: send side-channel info
    v3cRTPLib::send_gof(&state);
    std::cout << "  GoF sent" << std::endl;
    state.next_gof();
  }

  std::cout << "Stopping sending: " << state.get_error_msg() << std::endl;
  //
  // **************************************

  return EXIT_SUCCESS;
}