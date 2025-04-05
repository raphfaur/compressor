#include "../computing/worker.h"
#include "../tree/tree.h"
#include "../utils/profiling.hpp"
#include "serializer.hpp"
#include "transformer.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <istream>
#include <map>
#include <memory>
#include <numeric>
#include <ostream>
#include <string>

#define PARALLELIZATION

template <typename T, size_t size>
void compute_chunk(int n, std::shared_ptr<char[]> data,
                   std::array<std::atomic<int>, size> *lock_free_array) {
  std::array<int, size> tmp_array = {0};

  auto begin = (std::make_unsigned_t<T> *)data.get();

  std::for_each_n(begin, n, [&tmp_array](const std::make_unsigned_t<T> c) {
    tmp_array[c] += 1;
  });

  for (int i = 0; i < tmp_array.size(); i++) {
    (*lock_free_array)[i].fetch_add(tmp_array[i], std::memory_order_relaxed);
  }
}

template <typename buffer_t> struct out_segment_info {
  std::shared_ptr<buffer_t[]> data;
  int size;
  bool last;
  bool available;
};

template <typename T> class Compressor : public Transformer<T> {
  using Transformer<T>::istream;
  using Transformer<T>::ostream;

  std::unordered_map<T, int> frequency;
  std::map<T, std::pair<uint32_t, int>> dictionnary;
  std::array<T, UCHAR_MAX + 1> size_dict;
  std::array<T, UCHAR_MAX + 1> word_dict;
  std::vector<std::vector<char>> segments;
  std::unique_ptr<TreeNode<T>> tree_root;

public:
  void __compute_frequency_single_threaded();
  void __compute_tree();
  void __compute_dict();
  void __compute_segments();
  void __write_early_segments();
  void __traversal();
  void __write_single_thread();
  void __flush_buffer(size_t length, uint64_t buffer);
  Compressor(std::shared_ptr<std::basic_istream<T>> s) : Transformer<T>(s){};

  // Parallzlization utils
  void __write_parallelized();
  void __compute_frequency_parallelized();
  std::pair<int, int> __seek_cut(std::shared_ptr<T[]> buffer, int chunk_s);

  template <typename buffer_t>
  static void
  __translate(int in_buffer_s, std::shared_ptr<T[]> in_buffer,
              std::shared_ptr<buffer_t[]> out_buffer,
              std::array<T, UCHAR_MAX + 1> size_dict,
              std::array<T, UCHAR_MAX + 1> word_dict,
              std::mutex *out_segments_m,
              std::condition_variable *out_segments_cv,
              std::shared_ptr<out_segment_info<buffer_t>> out_segment);

  template <typename buffer_t>
  static void __write_out(
      std::shared_ptr<std::basic_ostream<T>> ostream,
      std::vector<std::shared_ptr<out_segment_info<buffer_t>>> *out_segments,
      std::mutex *out_segments_m, std::condition_variable *out_segments_cv);
  void run() override;
};

template <typename T> void Compressor<T>::run() {
  #ifdef PARALLELIZATION
  PROFILE(__compute_frequency_parallelized())
  #else
  PROFILE(__compute_frequency_single_threaded())
  #endif

  PROFILE(__compute_tree())
  PROFILE(__compute_segments())
  PROFILE(__write_early_segments())
  PROFILE(__compute_dict())

  #ifdef PARALLELIZATION
  PROFILE(__write_parallelized())
  #else
  PROFILE(__write_single_thread())
  #endif
}

#ifdef PARALLELIZATION

template <typename T> void Compressor<T>::__compute_frequency_parallelized() {
    int CHUNK_SIZE = 1000000;

  LoadDispatcher<char> dispatcher(0, CHUNK_SIZE);

  assert(std::atomic_int::is_always_lock_free);

  std::array<std::atomic<int>, UCHAR_MAX + 1> free_array = {0};

  while (!istream->eof()) {
    auto worker = dispatcher.request_worker();
    auto buffer = reinterpret_cast<char *>(worker->get_buffer().get());
    istream->read(buffer, CHUNK_SIZE);
    auto n = istream->gcount();
    auto data = worker->get_buffer();
    worker->run(compute_chunk<char, UCHAR_MAX + 1>, n, data, &free_array);
  };

  dispatcher.join();

  for (auto i = 0; i < free_array.size(); i++) {
    if (free_array[i].load())
      this->frequency[i] = free_array[i].load();
  }

  istream->clear();
}


/*
This function reads the input stream and find the highest number of char that
can be read while ensuring the translation would generate an out_buff such that
`out_buff.size() % 8 == 0` This will help when merging buffer to the output
stream.
*/
template <typename T>
std::pair<int, int> Compressor<T>::__seek_cut(std::shared_ptr<T[]> buffer,
                                              int chunk_s) {
  int virtual_out_size = 0;
  int best_fitting = 0;

  auto begin = buffer.get();

  istream->read(begin, chunk_s);

  auto count = istream->gcount();

  int best_virtual_out_size = 0;

  for (int i = 0; i < count; ++i) {
    virtual_out_size += size_dict[*(begin + i)];
    best_fitting = (virtual_out_size % 64 == 0) ? i : best_fitting;
    best_virtual_out_size =
        (virtual_out_size % 64 == 0) ? virtual_out_size : best_virtual_out_size;
  }

  // Last segment of the file
  if (istream->eof())
    return {count, virtual_out_size};

  return {best_fitting + 1, best_virtual_out_size};
}

template <typename T>
template <typename buffer_t>
void Compressor<T>::__translate(
    int in_buffer_s, std::shared_ptr<T[]> in_buffer,
    std::shared_ptr<buffer_t[]> out_buffer,
    std::array<T, UCHAR_MAX + 1> size_dict,
    std::array<T, UCHAR_MAX + 1> word_dict, std::mutex *out_segments_m,
    std::condition_variable *out_segments_cv,
    std::shared_ptr<out_segment_info<buffer_t>> out_segment) {

  T c;

  // Current offset in bits buffer
  size_t offset = 0;
  // Current offset in out_buffer
  size_t out_offset = 0;

  // Buffer that will contain bits
  buffer_t buffer = 0;

  constexpr size_t buffer_bits_n = sizeof(buffer) * 8;

  for (int i = 0; i < in_buffer_s; i++) {

    c = in_buffer.get()[i];

    auto phrase = word_dict[c];
    auto len = size_dict[c];

    if (offset + len >= buffer_bits_n) {
      // Fill gap with first bits
      size_t gap_len = buffer_bits_n - offset;
      buffer |= static_cast<decltype(buffer)>(phrase) << offset;
      offset = buffer_bits_n;

      // Flush
      out_buffer[out_offset++] = buffer;

      // Set the remaining bits as first bits
      buffer = static_cast<decltype(buffer)>(phrase) >> gap_len;
      offset = len - gap_len;
    } else {
      buffer |= static_cast<decltype(buffer)>(phrase) << offset;
      offset += len;
    }
  }
  // Handle last segment
  if (offset)
    out_buffer[out_offset] = buffer;

  // Post segment to write_out thread
  out_segments_m->lock();
  if (offset)
    out_segment->size =
        out_offset * sizeof(buffer_t) + offset / 8 + (offset % 8 != 0);
  else
    out_segment->size = out_offset * sizeof(buffer_t);
  out_segment->available = true;
  out_segments_m->unlock();
  out_segments_cv->notify_one();
}

template <typename T>
template <typename buffer_t>
void Compressor<T>::__write_out(
    std::shared_ptr<std::basic_ostream<T>> ostream,
    std::vector<std::shared_ptr<out_segment_info<buffer_t>>> *out_segments,
    std::mutex *out_segments_m, std::condition_variable *out_segments_cv) {
  int next_segment_to_write = 0;
  bool last = false;

  while (!last) {
    std::unique_lock lk(*out_segments_m);
    // Wait for the next segment to be ready
    out_segments_cv->wait(lk, [&out_segments, next_segment_to_write]() {
      return (out_segments->size() > next_segment_to_write &&
              (*out_segments)[next_segment_to_write]->available);
    });
    // Write all available consecutive segments
    while (!last && out_segments->size() > next_segment_to_write &&
           (*out_segments)[next_segment_to_write]->available) {
      DEBUG("Writing segment : " << next_segment_to_write);
      lk.unlock();
      auto out_segment_info = (*out_segments)[next_segment_to_write];
      auto raw_data = (char *)out_segment_info->data.get();
      ostream->write(raw_data, out_segment_info->size);
      next_segment_to_write++;
      last = out_segment_info->last;
      lk.lock();
    }
  }
}

template <typename T> void Compressor<T>::__write_parallelized() {

  constexpr int CHUNK_SIZE = 1000000;
  constexpr int OUT_CHUNK_SIZE = sizeof(uint64_t);

  std::vector<std::shared_ptr<out_segment_info<uint64_t>>> out_segments;
  std::mutex out_segments_m;
  std::condition_variable out_segments_cv;

  LoadDispatcher<char> dispatcher(0, CHUNK_SIZE);

  istream->seekg(0);

  // Flusher worker
  auto flusher_w = dispatcher.request_worker();
  flusher_w->run(Compressor<T>::__write_out<uint64_t>, ostream, &out_segments,
                 &out_segments_m, &out_segments_cv);

  int tot_size = 0;
  while (istream->peek() != EOF) {
    auto worker = dispatcher.request_worker();
    auto prev_g = istream->tellg();
    auto metadata = __seek_cut(worker->get_buffer(), CHUNK_SIZE);
    DEBUG("Size : " << metadata.first
                    << " Expected out size : " << metadata.second / 8);
    // + 1 becasue we handle the case where we can't achieve multiple of
    // OUT_CHUNK_SIZE
    auto out_buf = std::make_shared<uint64_t[]>(metadata.second / 8 + 1);

    // Create nex segment metadata
    auto bob = std::shared_ptr<out_segment_info<uint64_t>>(
        new out_segment_info<uint64_t>);
    bob->available = false;
    bob->data = out_buf;
    bob->last = istream->eof();

    // Append new sgement info to segments list
    out_segments_m.lock();
    out_segments.push_back(bob);
    out_segments_m.unlock();

    // Run translator on the segment
    worker->run(Compressor::__translate<uint64_t>, metadata.first,
                worker->get_buffer(), out_buf, size_dict, word_dict,
                &out_segments_m, &out_segments_cv, bob);

    // Seek to the right position
    istream->seekg((int)prev_g + metadata.first);
    tot_size += metadata.second / 8;
  }
  dispatcher.join();
}


#else

template <typename T> void Compressor<T>::__compute_frequency_single_threaded() {
  T c;
  size_t n = 100000;
  auto buffer = std::make_unique<char[]>(n);
  while (!istream->eof()) {
    istream->read(buffer.get(), n);
    auto count = istream->gcount();
    std::for_each_n(buffer.get(), count,
                    [this](char &c) { this->frequency[c]++; });
  }

  istream->clear();
}

template <typename T> void Compressor<T>::__write_single_thread() {

  // Rewind
  istream->seekg(0);
  T c;

  size_t offset = 0;

  uint64_t buffer = 0;

  constexpr size_t buffer_bits_n = sizeof(buffer) * 8;

  while (istream->peek() != EOF) {
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

template <typename T>
void inline Compressor<T>::__flush_buffer(size_t length, uint64_t buffer) {
  if (length == sizeof(buffer) * 8) {
    ostream->write((const char *)&buffer, length / 8);
  } else {
    ostream->write((const char *)&buffer, length / 8 + (length % 8 != 0));
  }
};


#endif


constexpr size_t WRITER_BUFFER_LENGTH = 4 * 10;

template <typename T> void Compressor<T>::__write_early_segments() {
  for (auto &segment : segments) {
    ostream->write((const char *)segment.data(), segment.size());
  }
}


template <typename T>
void print(const std::vector<std::unique_ptr<TreeNode<T>>> &heap) {
  for (auto i = heap.begin(); i < heap.end(); i++) {
    std::cout << (*i)->value << " ";
  }
  std::cout << std::endl;
}

template <typename T> void Compressor<T>::__compute_tree() {

  std::vector<std::unique_ptr<TreeNode<T>>> heap;
  double count = std::accumulate(
      frequency.begin(), frequency.end(), 0.,
      [](const int &acc, const auto entry) { return entry.second + acc; });
  for (auto &entry : frequency) {
    double f = entry.second / count;
    heap.push_back(std::make_unique<TreeNode<T>>(entry.first, f, false));
  }

  auto cmp = [](auto &lhs, auto &rhs) {
    return lhs->frequency > rhs->frequency;
  };

  std::make_heap(heap.begin(), heap.end(), cmp);

  while (heap.size() > 1) {

    std::pop_heap(heap.begin(), heap.end(), cmp);
    auto a = std::move(heap.back());
    heap.pop_back();

    std::pop_heap(heap.begin(), heap.end(), cmp);
    auto b = std::move(heap.back());
    heap.pop_back();

    auto merged_f = (a->frequency + b->frequency);
    heap.push_back(
        std::unique_ptr<TreeNode<T>>(new TreeNode<char>('-', merged_f, true)));
    heap.back()->set_left(std::move(b));
    heap.back()->set_right(std::move(a));

    std::push_heap(heap.begin(), heap.end(), cmp);
  }

  tree_root = std::move(heap.front());
}

template <typename T> void Compressor<T>::__compute_segments() {
  segments = serialize(*tree_root, 'x');
}

template <typename T> void Compressor<T>::__compute_dict() { __traversal(); }

template <typename T> void Compressor<T>::__traversal() {
  int phrase = 0;
  __backtrack(dictionnary, 0, tree_root, 0);

  // Speed up by turnin map<pair<int,int>> into two int[]
  for (auto &r : dictionnary) {
    word_dict[r.first] = r.second.first;
    size_dict[r.first] = r.second.second;
  }
}

template <typename T>
void __backtrack(std::map<T, std::pair<uint32_t, int>> &dict, size_t depth,
                 std::unique_ptr<TreeNode<T>> &node, uint32_t phrase) {
  if (!node->internal) {
    dict[node->value] = std::make_pair(phrase, depth);
    return;
  }

  if (node->left.get() != nullptr) {
    __backtrack(dict, depth + 1, node->left, phrase);
    node->left.release();
  }

  if (node->right.get() != nullptr) {
    __backtrack(dict, depth + 1, node->right, phrase + (1 << depth));
    node->right.release();
  }
}