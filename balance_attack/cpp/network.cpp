#include <random>
#include <set>

#include "all.h"

void log_edge_map(const std::vector<std::vector<time_ms_t>> &edge_map_cref) {
  return;
  auto log_edge = [](time_ms_t edge) {
    if (edge > NetworkGraph::UNREACHABLE) {
      std::cout << edge;
    } else {
      std::cout << " ";
    }
  };

  size_t num_nodes = edge_map_cref.size();
  std::cout << "[";
  for (size_t i = 0; i < num_nodes; ++i) {
    std::cout << "  [";
    log_edge(edge_map_cref[i][0]);
    for (size_t j = 1; j < num_nodes; ++j) {
      std::cout << ", ";
      log_edge(edge_map_cref[i][j]);
    }
    std::cout << "],\n";
  }
  std::cout << "]\n";
}

NetworkGraph NetworkGraph::make_random_graph(int num_nodes, int degree,
                                             time_ms_t latency) {
  std::default_random_engine rand_gen{std::random_device()()};
  auto edge_map = generate_connectivity(rand_gen, num_nodes, degree);
  assign_random_latency(rand_gen, std::ref(edge_map), latency);

  return NetworkGraph(std::cref(edge_map));
}

NetworkGraph NetworkGraph::make_random_geodelay_graph(
    int num_nodes, int degree, time_ms_t queueing_latency) {
  std::default_random_engine rand_gen{std::random_device()()};
  auto edge_map = generate_connectivity(rand_gen, num_nodes, degree);
  embed_geolatency(rand_gen, std::ref(edge_map), queueing_latency);

  return NetworkGraph(std::cref(edge_map));
}

NetworkGraph NetworkGraph::make_graph(const TestEnvironment &env) {
  if (env.graph_type == "geolatency") {
    return NetworkGraph::make_random_geodelay_graph(env.num_nodes, env.degree,
                                                    env.latency_ms);
  } else {
    return NetworkGraph::make_random_graph(env.num_nodes, env.degree,
                                           env.latency_ms);
  }
}

template <typename RandGen>
std::vector<std::vector<time_ms_t>> NetworkGraph::generate_connectivity(
    RandGen rand_gen, node_id_t num_nodes, int degree) {
  std::uniform_int_distribution<> random_node(0, num_nodes - 1);

  for (int iteration_count = 0; true; ++iteration_count) {
    std::cout << "generate_connectivity iteration " << iteration_count << "\n";
    std::vector<int> degrees((size_t)num_nodes);
    // negative edge distance means unreachable.
    std::vector<std::vector<time_ms_t>> edge_map(
        (size_t)num_nodes,
        std::vector<time_ms_t>((size_t)num_nodes, UNREACHABLE));

    for (node_id_t i = 0; i < num_nodes; ++i) {
      std::set<node_id_t> peers;
      while (peers.size() < (size_t)num_nodes && degree > degrees[(size_t)i]) {
        node_id_t peer = random_node(rand_gen);
        peers.insert(peer);
        if (peer == i || degrees[(size_t)peer] == degree ||
            edge_map[(size_t)i][(size_t)peer] > UNREACHABLE) {
          continue;
        }
        edge_map[(size_t)i][(size_t)peer] = CONNECTED;
        edge_map[(size_t)peer][(size_t)i] = CONNECTED;
        degrees[(size_t)i] += 1;
        degrees[(size_t)peer] += 1;
      }
    }

    std::vector<bool> nodes_reachable((size_t)num_nodes, false);
    nodes_reachable[0] = true;
    std::vector<int> visited_nodes{0};
    for (size_t i = 0; i < visited_nodes.size(); ++i) {
      int node = visited_nodes[i];
      for (node_id_t j = 0; j < num_nodes; ++j) {
        if (!nodes_reachable[(size_t)j] &&
            edge_map[(size_t)node][(size_t)j] > UNREACHABLE) {
          nodes_reachable[(size_t)j] = true;
          visited_nodes.push_back(j);
        }
      }
    }

    // The graph is connected.
    if ((node_id_t)visited_nodes.size() == num_nodes) {
      log_edge_map(edge_map);
      return edge_map;
    }
  }
}

template <typename RandGen>
void NetworkGraph::assign_random_latency(
    RandGen rand_gen, Ref<std::vector<std::vector<time_ms_t>>> edge_map_ms_ref,
    time_ms_t latency_ms) {
  std::cout << "assign_random_latency with latency " << latency_ms << "ms.\n";
  auto &edge_map_ms = edge_map_ms_ref.get();
  std::uniform_real_distribution<> random_latency_ms(0.75 * latency_ms,
                                                     1.25 * latency_ms);

  size_t num_nodes = edge_map_ms.size();
  for (size_t i = 0; i < num_nodes; ++i) {
    for (size_t j = i + 1; j < num_nodes; ++j) {
      if (edge_map_ms[i][j] > UNREACHABLE) {
        time_ms_t latency_ms = (time_ms_t)random_latency_ms(rand_gen);
        edge_map_ms[(size_t)i][(size_t)j] = latency_ms;
        edge_map_ms[(size_t)j][(size_t)i] = latency_ms;
      }
    }
  }
}

template <typename RandGen>
void NetworkGraph::embed_geolatency(
    RandGen rand_gen, Ref<std::vector<std::vector<time_ms_t>>> edge_map_ms_ref,
    time_ms_t queueing_latency_ms) {
  std::cout << "embed_geolatency with queueing_latency_ms "
            << queueing_latency_ms << "ms.\n";
  auto &edge_map_ms = edge_map_ms_ref.get();

  std::uniform_real_distribution<> random_latency_ms(0.8 * queueing_latency_ms,
                                                     1.2 * queueing_latency_ms);
  std::uniform_int_distribution<> random_city(0, (int)GEO_DELAY_MS.size() - 1);

  size_t num_nodes = edge_map_ms.size();
  std::vector<size_t> city(num_nodes, 0);
  for (size_t i = 0; i < num_nodes; ++i) {
    city[i] = (size_t)random_city(rand_gen);
  }

  for (size_t i = 0; i < num_nodes; ++i) {
    for (size_t j = i + 1; j < num_nodes; ++j) {
      if (edge_map_ms[i][j] > UNREACHABLE) {
        double queueing_latency_ms = random_latency_ms(rand_gen);
        edge_map_ms[i][j] =
            (time_ms_t)(GEO_DELAY_MS[city[i]][city[j]] + queueing_latency_ms);
        edge_map_ms[j][i] =
            (time_ms_t)(GEO_DELAY_MS[city[j]][city[i]] + queueing_latency_ms);
      }
    }
  }
}

std::vector<std::vector<time_ms_t>> NetworkGraph::compute_apsp(
    const std::vector<std::vector<time_ms_t>> &edge_map_cref) {
  size_t num_nodes = edge_map_cref.size();

  // init distances by copying edge_map_cref
  auto distances = edge_map_cref;

  // Corner case for the distance of each node to itself.
  for (size_t i = 0; i < num_nodes; ++i) {
    distances[i][i] = 0.0;
  }

  for (size_t k = 0; k < num_nodes; ++k) {
    for (size_t i = 0; i < num_nodes; ++i) {
      for (size_t j = 0; j < num_nodes; ++j) {
        if (distances[i][k] > UNREACHABLE && distances[k][j] > UNREACHABLE) {
          auto through_k = distances[i][k] + distances[k][j];
          auto &d_ij = distances[i][j];
          if (d_ij <= UNREACHABLE || through_k < d_ij) {
            d_ij = through_k;
          }
        }
      }
    }
  }

  log_edge_map(distances);
  return distances;
}

time_ms_t NetworkGraph::compute_diameter(
    const std::vector<std::vector<time_ms_t>> &all_pair_distances) {
  size_t num_nodes = all_pair_distances.size();
  time_ms_t diameter = 0;
  for (size_t i = 0; i < num_nodes; ++i) {
    for (size_t j = i + 1; j < num_nodes; ++j) {
      if (all_pair_distances[i][j] > UNREACHABLE) {
        diameter = std::max(diameter, all_pair_distances[i][j]);
      } else {
        diameter = std::numeric_limits<time_ms_t>::max();
      }
    }
  }

  return diameter;
}
