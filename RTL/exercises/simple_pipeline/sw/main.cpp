// Copyright (c) 2020 University of Florida
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

// Greg Stitt
// University of Florida
//
// Description: This application demonstrates a DMA AFU where the FPGA transfers
// data from an input array into an output array.
// 
// The example demonstrates an extension of the AFU wrapper class that uses
// AFU::malloc() to dynamically allocate virtually contiguous memory that can
// be accessed by both software and the AFU.

// INSTRUCTIONS: Change the configuration settings in config.h to test 
// different types of data.

#include <cstdlib>
#include <iostream>
#include <cmath>

#include <opae/utils.h>

#include "AFU.h"
// Contains application-specific information
#include "config.h"
// Auto-generated by OPAE's afu_json_mgr script
#include "afu_json_info.h"

using namespace std;


void printUsage(char *name);
bool checkUsage(int argc, char *argv[], unsigned long &num_inputs);

int main(int argc, char *argv[]) {

  unsigned long num_inputs;
  unsigned long num_outputs;

  if (!checkUsage(argc, argv, num_inputs)) {
    printUsage(argv[0]);
    return EXIT_FAILURE;
  }

  // There are 16 inputs for every 1 output.
  num_outputs = num_inputs / 16;

  try {
    // Create an AFU object to provide basic services for the FPGA. The 
    // constructor searchers available FPGAs for one with an AFU with the
    // the specified ID
    AFU afu(AFU_ACCEL_UUID); 
    bool failed = false;

    cout << "Measured AFU Clock Frequency: " << afu.measureClock() / 1e6
	 << "MHz" << endl;

    // Allocate input and output arrays.
    auto input  = afu.malloc<volatile unsigned long>(num_inputs);
    auto output = afu.malloc<volatile unsigned long long>(num_outputs);  

    // Initialize the input and output arrays.
    for (unsigned i=0; i < num_inputs; i++) {      
      input[i] = (unsigned long) 1;
    }

    for (unsigned i=0; i < num_outputs; i++) {      
      output[i] = (unsigned long) 0;
    }   
    
    // Inform the FPGA of the starting read and write address of the arrays.
    afu.write(MMIO_RD_ADDR, (uint64_t) input);
    afu.write(MMIO_WR_ADDR, (uint64_t) output);

    // The FPGA DMA only handles cache-line transfers, so we need to convert
    // the array size to cache lines. We could also do this conversion on the 
    // FPGA and transfer the number of inputs instead here.
    // The number of output cache lines is calculated by the FPGA.
    unsigned total_bytes = num_inputs*sizeof(unsigned long);
    unsigned num_cls = ceil((float) total_bytes / (float) AFU::CL_BYTES);
    afu.write(MMIO_SIZE, num_cls);

    // Start the FPGA DMA transfer.
    afu.write(MMIO_GO, 1);  

    // Wait until the FPGA is done.
    while (afu.read(MMIO_DONE) == 0) {
#ifdef SLEEP_WHILE_WAITING
      this_thread::sleep_for(chrono::milliseconds(SLEEP_MS));
#endif
    }
        
    for (unsigned i=0; i < num_outputs; i++) {     
      cout << output[i] << endl;
    }

       // Free the allocated memory.
    afu.free(input);
    afu.free(output);
        
    cout << "All DMA Tests Successful!!!" << endl;
    return EXIT_SUCCESS;
  }
  // Exception handling for all the runtime errors that can occur within 
  // the AFU wrapper class.
  catch (const fpga_result& e) {    
    
    // Provide more meaningful error messages for each exception.
    if (e == FPGA_BUSY) {
      cerr << "ERROR: All FPGAs busy." << endl;
    }
    else if (e == FPGA_NOT_FOUND) { 
      cerr << "ERROR: FPGA with accelerator " << AFU_ACCEL_UUID 
	   << " not found." << endl;
    }
    else {
      // Print the default error string for the remaining fpga_result types.
      cerr << "ERROR: " << fpgaErrStr(e) << endl;    
    }
  }
  catch (const runtime_error& e) {    
    cerr << e.what() << endl;
  }
  catch (const opae::fpga::types::no_driver& e) {
    cerr << "ERROR: No FPGA driver found." << endl;
  }

  return EXIT_FAILURE;
}


void printUsage(char *name) {

  cout << "Usage: " << name << " size num_tests\n"     
       << "size (positive integer for number of inputs to test, must be multiple of 128)\n"
       << endl;
}

// Returns unsigned long representation of string str.
// Throws an exception if str is not a positive integer.
unsigned long stringToPositiveInt(char *str) {

  char *p;
  long num = strtol(str, &p, 10);  
  if (p != 0 && *p == '\0' && num > 0) {
    return num;
  }

  throw runtime_error("String is not a positive integer.");
  return 0;  
}


bool checkUsage(int argc, char *argv[], unsigned long &num_inputs) {
  
  if (argc == 2) {
    try {
      num_inputs = stringToPositiveInt(argv[1]);
      if (num_inputs % 128 != 0)
	return false;
    }
    catch (const runtime_error& e) {    
      return false;
    }
  }
  else {
    return false;
  }

  return true;
}