#include <istream>
#include <fstream>
#include "./stream/compression.hpp"
#include "./stream/inflation.hpp"
#include <memory>
#include <unistd.h>


int main(int argc, char *argv[])
{
    bool decompress = false;
    char * infile;
    char * outfile;

    for (size_t i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-d") == 0) {
            decompress = true;
        } else if (strcmp(argv[i], "-i") == 0 ){
            infile = argv[i+1];
        } else if (strcmp(argv[i], "-o") == 0){
            outfile = argv[i+1];
        } else if (strcmp(argv[i], "-h") == 0){
            std::cout << " -i : Input file " << std::endl;
            std::cout << " -o : Output file " << std::endl;
            std::cout << " -d : Decompress mode" << std::endl;
            return 0;
        }
    }

    auto input = static_cast<std::basic_istream<char> *>(new std::ifstream(infile));
    auto output = static_cast<std::basic_ostream<char> *>(new std::ofstream(outfile, std::ios::binary));
    if(decompress){
        std::cout << "Decompressing" << std::endl;
        auto a = Inflator<char>(std::shared_ptr<std::basic_istream<char>>(input));
        a.set_output(std::shared_ptr<std::basic_ostream<char>>(output));
        a.run();
    } else {
        std::cout << "Compressing" << std::endl;
        auto c = Compressor<char>(std::shared_ptr<std::basic_istream<char>>(input));
        c.set_output(std::shared_ptr<std::basic_ostream<char>>(output));
        c.run();
    }

    return 0;
}
