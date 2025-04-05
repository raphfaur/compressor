#pragma once
#include "../tree/tree.h"
#include "serializer.hpp"
#include "transformer.hpp"
#include <bitset>
#include <cassert>
#include <memory>

template <typename T> class Inflator : public Transformer<T> {
  using Transformer<T>::istream;
  using Transformer<T>::ostream;

  std::unique_ptr<TreeNode<T>> tree;
  TreeNode<T> *current_node;

  void _advance(bool bit);

public:
  Inflator(std::shared_ptr<std::basic_istream<T>> s) : Transformer<T>(s){};
  Inflator() = default;

  void run() override {
    tree = std::move(deserialize(istream));
    current_node = tree.get();
    while (istream->peek() != EOF) {
      auto c = istream->get();
      std::bitset<8> bits(c);
      for (size_t i = 0; i < 8; ++i) {
        _advance(bits[i]);
      }
    }
  }
};

template <typename T> void Inflator<T>::_advance(bool bit) {
  if (!current_node->internal) {
    ostream->put(current_node->value);
    current_node = tree.get();
  }
  if (bit) {
    assert(current_node->right);
    current_node = current_node->right.get();
  } else {
    assert(current_node->left);
    current_node = current_node->left.get();
  }
}
