# uvgV3CRTP example codes

This directory contains a collection of commented and functioning examples that demonstrate the usage of uvgV3CRTP. The receiver and sender implementations are provided for the following scenarios

1. Sending/Receiving whole bitstream (simple_*_example.cpp)
2. Sending/Receiving one gof at a time (gof_*_example.cpp)
3. Sending/Receiving one v3c unit at a time (unit_*_example.cpp)

For ease of testing a test sequence can be downloaded from [here](https://ultravideo.fi/uvgRTP_example_sequence_longdress.vpcc).

## Building the examples

Compile the library according to instructions in [README.md](../README.md).

Navigate to `uvgV3CRTP/build/examples` and compile the programs with the `make` command.

## Running the examples

All examples take the same commandline parameters. Generally, running the examples follows the following steps:

0. If using a test sequence other than the one provided above, the constants at the start of the receiver need to be changed to match the test sequence (sender can be run first to receive correct values). Need to re-build examples.
1. Start receiver (5s timeout)
2. Start sender

Sender should be run with e.g.
```
./simple_sender_example /path/to/test_sequence.vpcc
```

and receiver with
```
./simple_receiver_example /path/to/test_sequence.vpcc
```
for the receiver the test sequence is optional. It is only used to verify that the bitstream was received correctly.

## Experimental usage

The simple example additionally allows passing out-of-band information at runtime. This is achieved as follows:

1. Start sender (Writes out-of-band information to file, 6s wait before send)
2. Start receiver (Reads out-of-band information)

Sender should be run with e.g.
```
./simple_sender_example /path/to/test_sequence.vpcc /path/to/outofbandinfo
```

and receiver with
```
./simple_receiver_example /path/to/test_sequence.vpcc  /path/to/outofbandinfo
```

Additional Receiver example options that can be changed in the example file:
- ```constexpr bool AUTO_PRECISION_MODE```: Setting this to true automatically infers size precision from v3c unit sizes, but reconstructed bitstream may not match original.