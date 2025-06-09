#include "all.h"

Simulator::Simulator(const TestEnvironment &env)
    : env(env),
      adversary(std::make_unique<StrategyRandomLatency>(
          env.num_nodes, StrategyRandomLatency::extra_send_calc)),
      network_graph(NetworkGraph::make_graph(env)) {}

bool Simulator::is_chain_merged(time_ms_t now) {
  if (this->nodes_converge.has_value()) {
    if (now - std::get<0>(*this->nodes_converge) > CONVERGE_TIME_MS_CRITERIA) {
      return true;
    }
  } else {
    char side = this->nodes[0].get_side();
    //std::cout << "is_chain_merged on side " << side << "?\n";
    /* // debug only code
    std::cout << "weight diff [";
    for (const NodeLocalView &peer : this->nodes) {
      std::cout << peer.weight_diff() << ", ";
    }
    std::cout << "]\nsides [";
    for (const NodeLocalView &peer : this->nodes) {
      std::cout << peer.get_side() << ", ";
    }
    std::cout << "]\n";
     */
    for (const NodeLocalView &peer : this->nodes) {
      if (peer.get_side() != side) {
        return false;
      }
    }
    // Now all nodes will mine the same side.
    this->nodes_converge = std::make_pair(now, side);
    std::cout << "At " << now << " ms all nodes mine on side " << side << "\n";
  }
  return false;
}

std::tuple<time_ms_t, SimulatorEvent> Simulator::pop_event() {
  auto ret = this->event_queue.top();
  this->event_queue.pop();
  return ret;
}

void Simulator::push_event(time_ms_t time, SimulatorEvent event) {
  this->event_queue.push(std::make_tuple(time, event));
}

void Simulator::setup_chain() {
  //std::cout << "Initializing the chain for all nodes.\n";
  for (int i = 0; i < this->env.num_nodes; ++i) {
    this->nodes.emplace_back(i);
  }
}

void Simulator::setup_network() {
  std::cout << "Adversary to setup network partition.\n";
  this->adversary->setup_network(this->network_graph.get_num_nodes(),
                                 this->network_graph.get_all_pairs_distances());
}

void Simulator::honest_node_broadcast_block(time_ms_t time_ms, int node,
                                            char side, block_id_t block) {
  auto peer_distances_ms = this->network_graph.get_connectivity(node);
  for (node_id_t peer = 0; peer < this->num_nodes(); ++peer) {
    if (peer == node) {
      continue;
    }
    auto &latency_ms = peer_distances_ms[(size_t)peer];
    auto deliver_time_ms = time_ms + latency_ms;
    this->push_event(deliver_time_ms, SimulatorEvent::new_block_deliver_event(
                                          peer, side, block));
  }
}
