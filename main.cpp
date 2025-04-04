#include <istream>
#include <fstream>
#include "./stream/compression.hpp"
#include "./stream/inflation.hpp"
#include <memory>
#include <unistd.h>
#include "computing/worker.h"
#include <thread>
#include<algorithm>
#include<functional>
#include <atomic>
#include<array>


void test (char bob []) {
    std::cout << std::is_same_v<decltype(bob), char []> << std::endl;
};


std::array<std::atomic<int>, UCHAR_MAX + 1> free_array;
std::array<std::atomic<int>, UCHAR_MAX + 1> free_array2;

void atomic_test() {
    std::vector<std::thread> pool;
    for (int n = 0; n < 100; ++n)
        pool.emplace_back([]() {
            for(auto n = 10000; n; n--) {
                free_array[0]++;
            };
        });
    for (int n = 0; n < 100; ++n)
        pool[n].join();
    
    INFO(free_array[0]);
}

void count_char_p(std::shared_ptr<std::basic_istream<char>> input) {
    int CHUNK_SIZE = 1000000;

    LoadDispatcher<char> dispatcher(0, CHUNK_SIZE);

    for(auto&x : free_array)
        assert(x.load() == 0);

    assert(std::atomic_int::is_always_lock_free);

    while (!input->eof()) {
        auto worker = dispatcher.request_worker();
        auto buffer = reinterpret_cast<char*>(worker->get_buffer().get());
        input->read(buffer, CHUNK_SIZE);
        auto n = input->gcount();
        auto data = worker->get_buffer();
        worker->run(compute_chunk<char, UCHAR_MAX + 1>, n, data, &free_array);
    };

    dispatcher.join();
    INFO(free_array['1']);
}

void count_char(std::shared_ptr<std::basic_istream<char>> input) {
    int count = 0;
    std::mutex c_mutex;
    while (!input->eof()) {
        auto c = input->get();
        free_array2[(uint8_t) c]++;
    };
    INFO(free_array2['1']);
}

void parallel_tester() {
    LoadDispatcher<uint8_t> dispatcher(100, 1000);
    for (int i = 0; i < 1000; i ++) {
        auto w = dispatcher.request_worker();
        w->run([]() {
            std::this_thread::sleep_for(std::chrono::milliseconds (100));
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    // std::cout << sizeof(*buffer) << std::endl;
}

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
        auto a = Inflator<char>(std::shared_ptr<std::basic_istream<char>>(input));
        a.set_output(std::shared_ptr<std::basic_ostream<char>>(output));
        a.run();
    } else {
        auto c = Compressor<char>(std::shared_ptr<std::basic_istream<char>>(input));
        c.set_output(std::shared_ptr<std::basic_ostream<char>>(output));
        c.run();
    }

    return 0;

    // auto input = static_cast<std::basic_istream<char> *>(new std::ifstream("big.txt"));
    // PROFILE(count_char(std::shared_ptr<std::basic_istream<char>> (input)))
    auto input2 = static_cast<std::basic_istream<char> *>(new std::ifstream("big.txt"));
    PROFILE(count_char_p(std::shared_ptr<std::basic_istream<char>> (input2)))
    // atomic_test();
}
