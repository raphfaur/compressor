#include "./stream/compression.hpp"
#include "./stream/inflation.hpp"
#include "computing/worker.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <fstream>
#include <functional>
#include <istream>
#include <memory>
#include <thread>
#include <unistd.h>

int main(int argc, char *argv[]) {
  bool decompress = false;
  char *infile;
  char *outfile;

  for (size_t i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-d") == 0) {
      decompress = true;
    } else if (strcmp(argv[i], "-i") == 0) {
      infile = argv[i + 1];
    } else if (strcmp(argv[i], "-o") == 0) {
      outfile = argv[i + 1];
    } else if (strcmp(argv[i], "-h") == 0) {
      std::cout << " -i : Input file " << std::endl;
      std::cout << " -o : Output file " << std::endl;
      std::cout << " -d : Decompress mode" << std::endl;
      return 0;
    }
  }

  auto input =
      static_cast<std::basic_istream<char> *>(new std::ifstream(infile));
  auto output = static_cast<std::basic_ostream<char> *>(
      new std::ofstream(outfile, std::ios::binary));

  if (decompress) {
    auto a = Inflator<char>(std::shared_ptr<std::basic_istream<char>>(input));
    a.set_output(std::shared_ptr<std::basic_ostream<char>>(output));
    a.run();
  } else {
    auto c = Compressor<char>(std::shared_ptr<std::basic_istream<char>>(input));
    c.set_output(std::shared_ptr<std::basic_ostream<char>>(output));
    PROFILE(c.run())
  }

  return 0;
}
