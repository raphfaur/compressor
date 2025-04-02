#pragma once
#include "../tree/tree.h"
#include <memory>
#include <istream>

/*
The layout of the memory is the following

---------------------------------------------
         1st segment        |   2nd segment  |
---------------------------------------------
 size of the flattened tree | Flattened tree |
---------------------------------------------
          4 bytes           |  Dynamic size  |
---------------------------------------------

*/
template <typename T>
std::vector<std::vector<char>> serialize(TreeNode<T> &root, T default_v)
{
    std::vector<std::vector<char>> segments;
    auto flattened_tree = TreeNode<T>::flatten(default_v, root);


    // Compute size segment
    std::vector<char> size_segment;
    int32_t size = flattened_tree.size() * sizeof(flattened_tree[0]);
    auto size_d = (char *)&size;
    size_segment.insert(size_segment.end(), size_d, size_d + sizeof(size));

    // Compute tree segment
    auto tree_data = (char *)flattened_tree.data();
    std::vector<char> tree_segment(tree_data, tree_data + size);

    // Push segments
    segments.push_back(size_segment);
    segments.push_back(tree_segment);

    return segments;
}

template <typename T>
std::unique_ptr<TreeNode<T>> deserialize(std::shared_ptr<std::basic_istream<T>> istream)
{

    // Compute size of the flattened tree
    int32_t tree_segment_size;
    istream->read(reinterpret_cast<char *>(&tree_segment_size), sizeof(tree_segment_size));

    auto tmp_buffer = new char[tree_segment_size];

    istream->read(tmp_buffer, tree_segment_size);
    std::vector<_TreeNode<T>> flattened_tree(reinterpret_cast<_TreeNode<T> *>(tmp_buffer), reinterpret_cast<_TreeNode<T> *>(tmp_buffer + tree_segment_size));
    std::cout << tree_segment_size << std::endl;
    std::cout << flattened_tree.size() << std::endl;

    return TreeNode<T>::inflate(flattened_tree);

}