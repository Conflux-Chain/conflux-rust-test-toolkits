#ifndef CPP_ADVERSARY_H
#define CPP_ADVERSARY_H

#include <vector>

#include "node_local_view.h"
#include "types.h"

class AdversaryStrategy {
  std::vector<node_id_t> nodes_to_keep_left;
  std::vector<node_id_t> nodes_to_keep_right;
  std::vector<time_ms_t> latency_cross_partition;

 public:
  virtual ~AdversaryStrategy() = default;
  virtual void adversary_mined(char side, block_id_t block_id,
                               time_ms_t time_ms) = 0;
  virtual char adversary_side_to_mine() const = 0;
  virtual std::tuple<int, std::deque<std::pair<time_ms_t, block_id_t>> &>
  adversary_strategy(
      time_ms_t time_ms,
      Ref<std::vector<std::pair<char, block_id_t>>> blocks_to_send) = 0;
  virtual void receive_block_from_peer(char side, block_id_t block_id,
                                       node_id_t peer) = 0;

  const std::vector<time_ms_t> &get_latency_cross_partition() const {
    return this->latency_cross_partition;
  }
  const std::vector<node_id_t> &get_nodes_to_keep_left() const {
    return this->nodes_to_keep_left;
  }
  const std::vector<node_id_t> &get_nodes_to_keep_right() const {
    return this->nodes_to_keep_right;
  }
  virtual int get_total_borrowed_blocks() const = 0;

  void setup_network(
      int num_nodes,
      const std::vector<std::vector<time_ms_t>> &all_pairs_distances);
  virtual void start_attack() = 0;
};

// This is the strategy which works in basic network environment.
// TODO: copy the impl over.
/*
class StrategyFixedPeerLatency : public AdversaryStrategy {
 private:
  // State fields.
  int left_subtree_weight;
  int right_subtree_weight;
  std::vector<int> left_withheld_blocks_queue;
  std::vector<int> right_withheld_blocks_queue;
  int left_borrowed_blocks;
  int right_borrowed_blocks;
  // The number of recent blocks mined under left side sent to the network.
  std::deque<int> adv_left_recent_sent_blocks;
  std::deque<int> adv_right_recent_sent_blocks;
  std::deque<int> honest_left_recent_sent_blocks;
  std::deque<int> honest_right_recent_sent_blocks;
};
*/

class StrategyRandomLatency : public AdversaryStrategy {
 private:
  // State fields.
  NodeLocalView adv_view;
  std::deque<std::pair<time_ms_t, block_id_t>> left_withheld_blocks_queue;
  std::deque<std::pair<time_ms_t, block_id_t>> right_withheld_blocks_queue;
  int left_borrowed_blocks;
  int right_borrowed_blocks;
  std::vector<NodeLocalView> honest_node_views;

  std::function<int(const StrategyRandomLatency &)> extra_send;

 public:
  StrategyRandomLatency(
      node_id_t num_nodes,
      std::function<int(const StrategyRandomLatency &)> extra_send)
      : adv_view(-1),
        left_borrowed_blocks(0),
        right_borrowed_blocks(0),
        extra_send(extra_send) {
    for (node_id_t i = 0; i < num_nodes; ++i) {
      this->honest_node_views.emplace_back(i);
    }
  }
  virtual ~StrategyRandomLatency() = default;

  virtual void adversary_mined(char side, block_id_t block_id,
                               time_ms_t time_ms);
  virtual char adversary_side_to_mine() const;
  virtual std::tuple<int, std::deque<std::pair<time_ms_t, block_id_t>> &>
  adversary_strategy(
      time_ms_t time_ms,
      Ref<std::vector<std::pair<char, block_id_t>>> blocks_to_send);

  virtual int get_total_borrowed_blocks() const {
    return this->left_borrowed_blocks + this->right_borrowed_blocks;
  }

  template <char SIDE>
  bool pop_withheld_block(
      time_ms_t time_ms,
      Ref<std::vector<std::pair<char, block_id_t>>> blocks_to_send);

  virtual void receive_block_from_peer(char side, block_id_t block_id,
                                       node_id_t peer);
  virtual void start_attack();

  static int extra_send_calc(const StrategyRandomLatency &);
};

#endif  // CPP_ADVERSARY_H
