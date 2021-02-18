#ifndef CPP_SIMULATOR_HPP
#define CPP_SIMULATOR_HPP

#include "all.h"
#include "types.h"

template <typename RandGen>
time_ms_t Simulator::simulate(RandGen &rng) {
  this->setup_chain();
  this->setup_network();
  return this->run_test(rng);
}

template <typename RandGen>
time_ms_t Simulator::run_test(RandGen &rng) {
  // Initialize the target's GHOST tree.
  block_id_t block_id = 0;
  for (node_id_t node : this->adversary->get_nodes_to_keep_right()) {
    this->nodes[(size_t)node].deliver_block(block_id,
                                            NodeLocalView::RIGHT_SIDE);
    this->honest_node_broadcast_block(0, node, NodeLocalView::RIGHT_SIDE,
                                      block_id);
  }

  // Start attack.
  this->adversary->start_attack();

  // Run simulation.
  time_ms_t timestamp_ms = 0;
  std::exponential_distribution<double> mining_time_dist(
      1 / this->env.average_block_period_ms);
  std::uniform_real_distribution<double> adv_mined_dist(0, 1);
  std::uniform_int_distribution<node_id_t> honest_mined_node_dist(
      0, this->num_nodes() - 1);
  this->push_event(timestamp_ms, SimulatorEvent::new_mine_block_event());
  this->push_event(timestamp_ms + 10, SimulatorEvent::new_check_merge_event());
  for (block_id = 1; timestamp_ms < this->env.termination_time_ms;) {
    SimulatorEvent event;
    std::tie(timestamp_ms, event) = this->pop_event();

    switch (event.type) {
      case SimulatorEventType::MINE_BLOCK: {
        // Insert the next mining event.
        time_ms_t time_to_next_block = (time_ms_t)mining_time_dist(rng);
        this->push_event(timestamp_ms + time_to_next_block,
                         SimulatorEvent::new_mine_block_event());

        bool is_adversary_mined = adv_mined_dist(rng) < this->env.evil_rate;
        if (is_adversary_mined) {
          std::cout << "Mined " << block_id << " by adversary at "
                    << timestamp_ms << "ms.\n";
          char side = this->adversary->adversary_side_to_mine();
          this->adversary->adversary_mined(side, block_id, timestamp_ms);
        } else {
          node_id_t miner = honest_mined_node_dist(rng);
          char side = this->nodes[(size_t)miner].get_side();
          std::cout << "Mined " << block_id << " " << side << " by node "
                    << miner << " at " << timestamp_ms << "ms.\n";

          // Update miner and attacker's view.
          this->nodes[(size_t)miner].deliver_block(block_id, side);
          this->honest_node_broadcast_block(timestamp_ms, miner, side,
                                            block_id);

          this->push_event(timestamp_ms + this->env.one_way_latency_ms,
                           SimulatorEvent::new_adv_received_block_event(
                               side, block_id, miner));
        }
        ++block_id;
        break;
      }
      case SimulatorEventType::ADV_RECEIVED_BLOCK: {
        block_id_t block;
        char side;
        node_id_t peer;
        std::tie(side, block, peer) = event.event.adv_receive_block_event;
        this->adversary->receive_block_from_peer(side, block, peer);

        // Trigger adversary strategy.
        std::vector<std::pair<char, block_id_t>> blocks_to_send_by_adv;
        int debug_borrow_blocks_count;
        std::optional<std::reference_wrapper<
            std::deque<std::pair<time_ms_t, block_id_t>>>>
            debug_borrow_withhold_queue;
        std::tie(debug_borrow_blocks_count, debug_borrow_withhold_queue) =
            this->adversary->adversary_strategy(
                timestamp_ms, Ref(std::ref(blocks_to_send_by_adv)));
        if (this->env.debug_allow_borrow) {
          block_id_t total_borrowed_blocks =
              (block_id_t)this->adversary->get_total_borrowed_blocks();
          for (block_id_t i = debug_borrow_blocks_count; i > 0; --i) {
            debug_borrow_withhold_queue->get().push_back(std::make_pair(
                timestamp_ms,
                // Use the negative block_id for "borrowed" blocks
                i - 1 - total_borrowed_blocks));
          }
          this->adversary->adversary_strategy(
              timestamp_ms, Ref(std::ref(blocks_to_send_by_adv)));
        }
        // TODO: log the blocks to send for debugging.
        time_ms_t time_delivery = timestamp_ms + this->env.one_way_latency_ms;
        for (std::pair<char, block_id_t> &item : blocks_to_send_by_adv) {
          char side;
          block_id_t block_id;
          std::tie(side, block_id) = item;
          std::cout << "At " << timestamp_ms << "ms adversary send block "
                    << block_id << " on side " << side << "\n";
          const auto &cross_partition_distance =
              this->adversary->get_latency_cross_partition();
          const auto &same_side_peers =
              (side == NodeLocalView::LEFT_SIDE)
                  ? this->adversary->get_nodes_to_keep_left()
                  : this->adversary->get_nodes_to_keep_right();
          const auto &other_side_peers =
              (side == NodeLocalView::RIGHT_SIDE)
                  ? this->adversary->get_nodes_to_keep_left()
                  : this->adversary->get_nodes_to_keep_right();

          for (node_id_t peer : same_side_peers) {
            this->push_event(
                time_delivery,
                SimulatorEvent::new_block_deliver_event(peer, side, block_id));
          }
          for (node_id_t peer : other_side_peers) {
            this->push_event(
                time_delivery + cross_partition_distance[(size_t)peer],
                SimulatorEvent::new_block_deliver_event(peer, side, block_id));
          }
        }
        break;
      }
      case SimulatorEventType::CHECK_MERGE: {
        if (this->is_chain_merged(timestamp_ms)) {
          std::cout << "Chain merged after " << timestamp_ms << " ms.\n";
          return timestamp_ms;
        } else {
          this->push_event(timestamp_ms + 50,
                           SimulatorEvent::new_check_merge_event());
        }
        break;
      }
      case SimulatorEventType::QUEUE_EMPTY:
        // Can't happen because of mining.
        break;
      case SimulatorEventType::BLOCK_DELIVER: {
        // Trigger adversary strategy just after each block delivery event.

        // In this experiment we assume that each block is delivered after its
        // parent.
        node_id_t receiver;
        block_id_t received_block;
        char side;
        std::tie(receiver, side, received_block) =
            event.event.block_deliver_event;
        if (this->nodes[(size_t)receiver].deliver_block(received_block, side)) {
          // TODO: maybe add a event to check for merge. (to help debugging)

          // Each peer forward the block to adversary.
          this->push_event(timestamp_ms + this->env.one_way_latency_ms,
                           SimulatorEvent::new_adv_received_block_event(
                               side, received_block, receiver));
        }
        break;
      }
      default:
        throw std::logic_error("non_exhausive switch class");
    }
  }
  return this->env.termination_time_ms;
}

#endif  // CPP_SIMULATOR_HPP
