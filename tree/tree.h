#pragma once

#include <functional>
#include <type_traits>

template <typename T> struct _TreeNode {
  T value;
  bool empty;
};

template <typename T> class TreeNode {
public:
  T value;
  std::unique_ptr<TreeNode<T>> left;
  std::unique_ptr<TreeNode<T>> right;
  double frequency;
  bool internal;

  TreeNode(T value, double frequency, std::unique_ptr<TreeNode<T>> &&left,
           std::unique_ptr<TreeNode<T>> &&right)
      : frequency(frequency), value(value) {
    left = std::forward(left);
    right = std::forward(right);
  }

  TreeNode(T value, double frequency, bool internal)
      : value(value), frequency(frequency), internal(internal){};

  void set_left(std::unique_ptr<TreeNode<T>> &&node) {
    left = std::forward<std::unique_ptr<TreeNode<T>>>(node);
  }

  void set_right(std::unique_ptr<TreeNode<T>> &&node) {
    right = std::forward<std::unique_ptr<TreeNode<T>>>(node);
  }

  static std::vector<_TreeNode<T>> flatten(T default_value, TreeNode &root) {

    // Compute depth of tree
    int depth = TreeNode<T>::compute_depth(root);

    // Compute size of vector
    int node_n = (int)(-1 * (1 - std::pow(2, depth)));

    // Default value for empty node
    _TreeNode<T> default_node = {default_value, true};

    // Init vector
    std::vector<_TreeNode<T>> nodes(node_n, default_node);

    root._flatten(nodes, 0);

    return nodes;
  }

  static int _compute_depth(TreeNode &root, int depth) {
    int ld = depth + 1;
    int rd = depth + 1;

    if (root.left)
      ld = _compute_depth(*(root.left), ld);
    if (root.right)
      rd = _compute_depth(*(root.right), rd);

    return std::max(ld, rd);
  }

  static int compute_depth(TreeNode &node) { return _compute_depth(node, 0); }

  void _flatten(std::vector<_TreeNode<T>> &nodes, size_t i) {
    nodes[i].value = value;
    if (left)
      left->_flatten(nodes, 2 * i + 1);
    if (right)
      right->_flatten(nodes, 2 * (i + 1));
    if (!left && !right)
      nodes[i].empty = false;
  }

  static std::unique_ptr<TreeNode> inflate(std::vector<_TreeNode<T>>& nodes) {
    return _inflate(nodes, 0);
  }

  static std::unique_ptr<TreeNode> _inflate(std::vector<_TreeNode<T>>& nodes,
                                            int i) {
    
    auto _node = nodes[i];
    auto node = std::make_unique<TreeNode>(_node.value, 0, _node.empty);
    // Check if left child exists
    if (2 * i+1 < nodes.size() ) node->left = std::move(_inflate(nodes, 2*i + 1));
    // Check if right child exists
    if (2 * (i+1) < nodes.size() ) node->right = std::move(_inflate(nodes, 2* (i + 1)));

    return node;
  }
};