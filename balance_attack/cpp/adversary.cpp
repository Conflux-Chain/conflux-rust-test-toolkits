#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics.hpp>

#include "all.h"

void AdversaryStrategy::setup_network(
    int num_nodes,
    const std::vector<std::vector<time_ms_t>> &all_pairs_distances) {
  if (num_nodes < 2) {
    return;
  }

  for (int i = 0; i < num_nodes; i += 2) {
    this->nodes_to_keep_left.push_back(i);
  }
  for (int i = 1; i < num_nodes; i += 2) {
    this->nodes_to_keep_right.push_back(i);
  }

  // initialize latency_cross_partition
  auto latency_cross_partition = std::vector<time_ms_t>((size_t)num_nodes, 0.0);
  int a_node_left = this->nodes_to_keep_left[0];
  int a_node_right = this->nodes_to_keep_right[0];
  for (int node : this->nodes_to_keep_left) {
    latency_cross_partition[(size_t)node] =
        all_pairs_distances[(size_t)a_node_right][(size_t)node];
  }
  for (int node : this->nodes_to_keep_right) {
    latency_cross_partition[(size_t)node] =
        all_pairs_distances[(size_t)a_node_left][(size_t)node];
  }
  // actually compute latency_cross_partition
  for (int source : this->nodes_to_keep_right) {
    for (int dest : this->nodes_to_keep_right) {
      latency_cross_partition[(size_t)dest] =
          std::min(latency_cross_partition[(size_t)dest],
                   all_pairs_distances[(size_t)source][(size_t)dest]);
    }
  }
  this->latency_cross_partition = latency_cross_partition;
}

void StrategyRandomLatency::start_attack() {
  for (node_id_t peer : this->get_nodes_to_keep_right()) {
    this->receive_block_from_peer(NodeLocalView::RIGHT_SIDE, 0, peer);
  }
}

void StrategyRandomLatency::adversary_mined(char side, block_id_t block_id,
                                            time_ms_t time_ms) {
  auto &withhold_queue = (side == NodeLocalView::LEFT_SIDE)
                             ? this->left_withheld_blocks_queue
                             : this->right_withheld_blocks_queue;
  withhold_queue.push_back(std::make_pair(time_ms, block_id));
}

char StrategyRandomLatency::adversary_side_to_mine() const {
  if (this->left_withheld_blocks_queue.size() +
          (size_t)this->adv_view.get_left_subtree_weight() <
      this->right_withheld_blocks_queue.size() +
          (size_t)this->adv_view.get_right_subtree_weight()) {
    return NodeLocalView::LEFT_SIDE;
  } else {
    return NodeLocalView::RIGHT_SIDE;
  }
}

std::tuple<int, std::deque<std::pair<time_ms_t, block_id_t>> &>
StrategyRandomLatency::adversary_strategy(
    time_ms_t time_ms,
    Ref<std::vector<std::pair<char, block_id_t>>> blocks_to_send) {
  int extra_send = this->extra_send(*this);
  auto average_weight_diff = [this](const std::vector<node_id_t> &nodes) {
    boost::accumulators::accumulator_set<
        double, boost::accumulators::features<boost::accumulators::tag::mean>>
        accumulator;
    for (node_id_t peer : nodes) {
      accumulator(this->honest_node_views[(size_t)peer].weight_diff());
    }
    return boost::accumulators::mean(accumulator);
  };
  double approx_left_target_subtree_weight_diff =
      average_weight_diff(this->get_nodes_to_keep_left());
  double approx_right_target_subtree_weight_diff =
      average_weight_diff(this->get_nodes_to_keep_right());

  double left_send_count = -approx_left_target_subtree_weight_diff + extra_send;
  double right_send_count =
      approx_right_target_subtree_weight_diff + 1 + extra_send;
  if (this->adv_view.get_side() == NodeLocalView::LEFT_SIDE) {
    left_send_count = 0;
  } else {
    right_send_count = 0;
  };

  int blocks_to_borrow = 0;
  for (int i = 0; i < left_send_count; ++i) {
    if (!this->pop_withheld_block<NodeLocalView::LEFT_SIDE>(time_ms,
                                                            blocks_to_send)) {
      ++blocks_to_borrow;
    }
  }
  for (int i = 0; i < right_send_count; ++i) {
    if (!this->pop_withheld_block<NodeLocalView::RIGHT_SIDE>(time_ms,
                                                             blocks_to_send)) {
      ++blocks_to_borrow;
    }
  }

  if (left_send_count > 0) {
    this->left_borrowed_blocks += blocks_to_borrow;
    return std::make_pair(blocks_to_borrow,
                          std::ref(this->left_withheld_blocks_queue));
  } else {
    this->right_borrowed_blocks += blocks_to_borrow;
    return std::make_pair(blocks_to_borrow,
                          std::ref(this->right_withheld_blocks_queue));
  }
}

template <char SIDE>
bool StrategyRandomLatency::pop_withheld_block(
    time_ms_t time_ms,
    Ref<std::vector<std::pair<char, block_id_t>>> blocks_to_send) {
  // FIXME: maintain and log the withhold period.
  auto &withheld_blocks_queue = (SIDE == NodeLocalView::LEFT_SIDE)
                                    ? this->left_withheld_blocks_queue
                                    : this->right_withheld_blocks_queue;
  if (!withheld_blocks_queue.empty()) {
    block_id_t block_id;
    std::tie(std::ignore, block_id) = withheld_blocks_queue.front();
    withheld_blocks_queue.pop_front();
    this->adv_view.deliver_block(block_id, SIDE);
    blocks_to_send.get().push_back(std::make_pair(SIDE, block_id));

    return true;
  } else {
    return false;
  }
}

void StrategyRandomLatency::receive_block_from_peer(char side,
                                                    block_id_t block_id,
                                                    node_id_t peer) {
  this->honest_node_views[(size_t)peer].deliver_block(block_id, side);
  this->adv_view.deliver_block(block_id, side);
}

int StrategyRandomLatency::extra_send_calc(
    const StrategyRandomLatency &strategy) {
  int global_subtree_weight_diff = strategy.adv_view.weight_diff();
  int absolute_weight_diff = (global_subtree_weight_diff < 0)
                                 ? -global_subtree_weight_diff
                                 : global_subtree_weight_diff + 1;
  return (absolute_weight_diff < 3) ? -1 : (absolute_weight_diff == 3) ? 0 : 1;
}