#ifndef CPP_NODE_LOCAL_VIEW_H
#define CPP_NODE_LOCAL_VIEW_H

#include <cassert>

#include "types.h"

class NodeLocalView {
 public:
  static const char LEFT_SIDE = 'L';
  static const char RIGHT_SIDE = 'R';

 private:
  node_id_t node_id;
  char side;
  block_id_t left_subtree_weight;
  block_id_t right_subtree_weight;
  std::set<block_id_t> received;

 public:
  NodeLocalView(node_id_t node_id)
      : node_id(node_id), left_subtree_weight(0), right_subtree_weight(0) {
    this->update_side();
  }

  bool deliver_block(block_id_t block_id, char side) {
    bool inserted;
    std::tie(std::ignore, inserted) = this->received.insert(block_id);
    if (inserted) {
      if (side == LEFT_SIDE) {
        ++this->left_subtree_weight;
      } else {
        ++this->right_subtree_weight;
      }
      this->update_side();
    }
    return inserted;
  }

  int get_blocks_received() const { return (int)this->received.size(); }
  char get_side() const { return this->side; }
  node_id_t get_node_id() const { return this->node_id; }
  int get_left_subtree_weight() const { return this->left_subtree_weight; }
  int get_right_subtree_weight() const { return this->right_subtree_weight; }
  int weight_diff() const {
    return this->left_subtree_weight - this->right_subtree_weight;
  }

 private:
  void update_side() {
    if (this->left_subtree_weight >= this->right_subtree_weight) {
      this->side = LEFT_SIDE;
    } else {
      this->side = RIGHT_SIDE;
    }
  }
};

#endif  // CPP_NODE_LOCAL_VIEW_H
