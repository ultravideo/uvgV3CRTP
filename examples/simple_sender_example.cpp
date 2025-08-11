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
  uvgV3CRTP::V3C_State<uvgV3CRTP::V3C_Sender> state(buf.get(), length); // Create a new state in a sender configuration
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
  //auto info = std::unique_ptr<char, decltype(&free)>(state.get_bitstream_info_string(&len, uvgV3CRTP::INFO_FMT::PARAM), &free);
  //std::cout << info.get() << std::endl;
//
// **************************************

// *********** Handle writing out-of-band info to file *************
//
  if (argc >= 3) { // If an out-of-band file is specified write out-of-band info to it
    size_t len = 0;
    auto out_info = std::unique_ptr<char, decltype(&free)>(state.get_bitstream_info_string(&len, uvgV3CRTP::INFO_FMT::RAW), &free);

    std::cout << "Writing out-of-band info to file... ";
    std::ofstream out_of_band_file(argv[2]);
    if (!out_of_band_file.is_open()) {
      std::cerr << "Error: Could not open out-of-band file for writing." << std::endl;
      return EXIT_FAILURE;
    }
    out_of_band_file.write(out_info.get(), len);
    out_of_band_file.close();
    std::cout << "Done" << std::endl;

    // Wait for a bit so the receiver can be started before sending the bitstream
    std::cout << "Waiting for receiver to be started... (5 seconds)" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(5));
  } 
//
// *****************************************************************

// ******** Send sample stream **********
//
  std::cout << "Sending bitstream... ";
  uvgV3CRTP::send_bitstream(&state);
  std::cout << "Done" << std::endl;
//
// **************************************

  return EXIT_SUCCESS;
}