#include<istream>
#include<ostream>
#include<map>
#include<memory>
#include<iostream>
#include<string>
#include<algorithm>
#include<numeric>
#include "../tree/tree.h"
#include<cmath>
#include "serializer.hpp"
#include "transformer.hpp"

template <typename T>
class Compressor : public Transformer<T> {
    using Transformer<T>::istream;
    using Transformer<T>::ostream;

    std::unordered_map<T, int> frequency;
    std::map<T, std::pair<uint32_t, int>> dictionnary;
    std::vector<std::vector<char>> segments;
    std::unique_ptr<TreeNode<T>> tree_root;

public:
    void __compute_frequency();
    void __compute_tree();
    void __compute_dict();
    void __compute_segments();
    void __write_early_segments();
    void __traversal();
    void __write();
    void __flush_buffer(size_t length, uint64_t buffer);
    Compressor(std::shared_ptr<std::basic_istream<T> > s) : Transformer<T>(s){};

    void run() override;
};

template<typename T>
void Compressor<T>::run(){
    __compute_frequency();
    __compute_tree();
    __compute_segments();
    __write_early_segments();
    __compute_dict();
    __write();
}

template<typename T>
void Compressor<T>::__compute_frequency(){
    T c;
    while (istream->peek() != EOF) {
        c = istream->get();
        frequency[c]++; 
    }
}

template<typename T>
void print(const std::vector<std::unique_ptr<TreeNode<T>>>& heap) {
    for(auto i = heap.begin(); i < heap.end(); i ++) {
        std::cout << (*i)->value << " ";
    }
    std::cout << std::endl;
}

template<typename T>
void Compressor<T>::__compute_tree(){

    std::vector<std::unique_ptr<TreeNode<T>>> heap;
    double count = std::accumulate(frequency.begin(), frequency.end(), 0., [](const int& acc, const auto entry) {
        return entry.second + acc;
    });
    for (auto &entry : frequency) {
        double f = entry.second / count;
        heap.push_back(std::make_unique<TreeNode<T>>(entry.first, f, false));
    }

    auto cmp = [](auto& lhs, auto& rhs){ return lhs->frequency > rhs->frequency;};

    std::make_heap(heap.begin(), heap.end(), cmp);

    while(heap.size() > 1) {

        std::pop_heap(heap.begin(), heap.end(), cmp);
        auto a = std::move(heap.back());
        heap.pop_back();

        std::pop_heap(heap.begin(), heap.end(), cmp);
        auto b = std::move(heap.back());
        heap.pop_back();

        auto merged_f = (a->frequency + b->frequency);
        heap.push_back(std::unique_ptr<TreeNode<T>>(new TreeNode<char>('-',merged_f, true)));
        heap.back()->set_left(std::move(b));
        heap.back()->set_right(std::move(a));
    
        std::push_heap(heap.begin(), heap.end(), cmp);
    }

    tree_root = std::move(heap.front());

}

template<typename T>
void Compressor<T>::__compute_segments() {
    segments = serialize(*tree_root, 'x');
}

template<typename T>
void Compressor<T>::__compute_dict() {
    __traversal();
}

template<typename T>
void Compressor<T>::__traversal() {
    int phrase = 0;
    __backtrack(dictionnary, 0, tree_root, 0);
}

template<typename T>
void __backtrack(std::map<T, std::pair<uint32_t, int>>& dict, size_t depth, std::unique_ptr<TreeNode<T>>& node, uint32_t phrase) {
    if( ! node->internal ) {
        dict[node->value] = std::make_pair(phrase, depth);
        return;
    }

    if (node->left.get() != nullptr ) {
        __backtrack(dict, depth + 1, node->left, phrase);
        node->left.release();
    }

    if (node->right.get() != nullptr ) {
        __backtrack(dict, depth + 1, node->right, phrase + (1 << depth));
        node->right.release();
    }
}

constexpr size_t WRITER_BUFFER_LENGTH = 4 * 10;


template<typename T>
void  Compressor<T>::__write_early_segments() {
    for (auto& segment : segments) {
        ostream->write((const char *) segment.data(), segment.size());
    }
}

template<typename T>
void Compressor<T>::__write() {

    // Rewind
    istream->seekg(0);
    T c;

    size_t offset = 0;

    uint64_t buffer = 0;

    constexpr size_t buffer_bits_n = sizeof(buffer) * 8;

    while(istream->peek() != EOF) {
        c = istream->get();
        auto encoding = dictionnary[c];
        auto phrase = encoding.first;
        auto len = encoding.second;

        if (offset + len >= buffer_bits_n) {
            // Fill gap with first bits
            size_t gap_len = buffer_bits_n - offset;
            buffer |= static_cast<decltype(buffer)>(phrase) << offset;
            offset = buffer_bits_n;
            
            // Flush
            __flush_buffer(offset, buffer);

            // Set the remaining bits as first bits
            buffer = static_cast<decltype(buffer)>(phrase) >> gap_len;
            offset = len - gap_len;

        } else {
            buffer |= static_cast<decltype(buffer)>(phrase) << offset;
            offset += len;
        }
    }
    __flush_buffer(offset, buffer);
}

template<typename T>
void inline Compressor<T>::__flush_buffer(size_t length, uint64_t buffer) {
    if (length == sizeof(buffer) * 8) {
        ostream->write((const char * ) &buffer, length / 8);
    } else {
        ostream->write((const char * ) &buffer, length / 8 + (length % 8 != 0));
    }
};
