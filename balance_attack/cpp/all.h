#ifndef CPP_ALL_H
#define CPP_ALL_H

#include <iostream>
#include <memory>
#include <queue>
#include <random>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "adversary.h"
#include "node_local_view.h"
#include "types.h"

enum class SimulatorEventType {
  BLOCK_DELIVER = 1,
  MINE_BLOCK = 0,
  ADV_RECEIVED_BLOCK = 2,
  CHECK_MERGE = 3,
  QUEUE_EMPTY = 4,
};

struct SimulatorEvent {
  SimulatorEventType type;
  // TODO: add access helper method to retrieve union based on the type.
  union Event {
    std::tuple<node_id_t, char, block_id_t> block_deliver_event;
    std::tuple<> mine_block_event;
    std::tuple<> check_merge_event;
    std::tuple<char, block_id_t, node_id_t> adv_receive_block_event;
    std::tuple<> queue_empty_event;
  } event;

  SimulatorEvent()
      : type(SimulatorEventType::QUEUE_EMPTY),
        event({.queue_empty_event = {}}) {}
  SimulatorEvent(SimulatorEventType type, Event event)
      : type(type), event(event) {}
  SimulatorEvent(const SimulatorEvent &x) = default;

  SimulatorEvent &operator=(const SimulatorEvent &x) {
    this->type = x.type;
    switch (this->type) {
      case SimulatorEventType::BLOCK_DELIVER:
        this->event.block_deliver_event = x.event.block_deliver_event;
        break;
      case SimulatorEventType::MINE_BLOCK:
        this->event.mine_block_event = x.event.mine_block_event;
        break;
      case SimulatorEventType::ADV_RECEIVED_BLOCK:
        this->event.adv_receive_block_event = x.event.adv_receive_block_event;
        break;
      case SimulatorEventType::CHECK_MERGE:
        this->event.check_merge_event = x.event.check_merge_event;
        break;
      case SimulatorEventType::QUEUE_EMPTY:
        this->event.queue_empty_event = x.event.queue_empty_event;
        break;
      default:
        throw std::logic_error("non_exhausive switch");
    }

    return *this;
  };

  bool operator>(const SimulatorEvent &rhs) const {
    return this->type > rhs.type;
  }
  bool operator<(const SimulatorEvent &rhs) const {
    return this->type < rhs.type;
  }

  static SimulatorEvent new_adv_received_block_event(char side,
                                                     block_id_t block_id,
                                                     node_id_t receiver) {
    return SimulatorEvent(
        SimulatorEventType::ADV_RECEIVED_BLOCK,
        {.adv_receive_block_event = std::make_tuple(side, block_id, receiver)});
  }

  static SimulatorEvent new_block_deliver_event(node_id_t receiver, char side,
                                                block_id_t block) {
    return SimulatorEvent(
        SimulatorEventType::BLOCK_DELIVER,
        {.block_deliver_event = std::make_tuple(receiver, side, block)});
  }

  static SimulatorEvent new_check_merge_event() {
    return SimulatorEvent{SimulatorEventType ::CHECK_MERGE,
                          {.mine_block_event = std::make_tuple()}};
  }

  static SimulatorEvent new_mine_block_event() {
    return SimulatorEvent{SimulatorEventType ::MINE_BLOCK,
                          {.mine_block_event = std::make_tuple()}};
  }

  std::optional<std::reference_wrapper<std::tuple<int, char, block_id_t>>>
  get_block_deliver_event() {
    if (this->type == SimulatorEventType::BLOCK_DELIVER) {
      return std::nullopt;
    } else {
      return {std::ref(this->event.block_deliver_event)};
    }
  }

  std::optional<std::reference_wrapper<std::tuple<>>> get_mine_block_event() {
    if (this->type == SimulatorEventType::MINE_BLOCK) {
      return std::nullopt;
    } else {
      return {std::ref(this->event.mine_block_event)};
    }
  }
};

std::ostream &operator<<(std::ostream &os, SimulatorEvent);

static const std::vector<std::vector<double>> GEO_DELAY_MS = {
    {1,       296.928, 36.986,  20.409,  24.794, 114.291, 124.786,
     7.158,   141.355, 27.041,  41.92,   6.151,  204.77,  281.861,
     183.197, 21.62,   276.805, 263.036, 93.053, 91.225},
    {286.701, 1,       295.947, 293.476, 299.945, 183.301, 403.763,
     275.475, 162.868, 290.705, 325.031, 280.406, 310.881, 275.683,
     122.325, 309.163, 27.954,  191.476, 200.025, 208.471},
    {37.02,   295.885, 1,       64.691,  44.147,  147.641, 298.107,
     32.911,  154.864, 33.321,  79.28,   28.58,   229.974, 283.125,
     283.911, 64.068,  349.435, 308.048, 114.397, 111.466},
    {21.655,  292.504, 64.448,  1,       44.213,  128.031, 166.586,
     25.746,  154.795, 22.578,  48.598,  24.474,  226.75,  228.028,
     202.137, 40.721,  329.261, 247.137, 115.047, 102.718},
    {24.645,  300.208, 44.117,  45.836,  1,       127.607, 153.503,
     18.473,  159.794, 29.343,  43.855,  24.118,  220.427, 274.238,
     187.711, 10.267,  350.329, 257.589, 110.576, 104.095},
    {114.348, 177.724, 156.003, 135.368, 128.56,  1,       226.789,
     106.173, 29.91,   127.173, 167.252, 116.563, 139.161, 306.019,
     202.181, 143.448, 182.98,  134.586, 43.34,   36.941},
    {124.853, 398.625, 298.227, 170.584, 153.574, 224.058, 1,
     232.507, 250.196, 134.52,  175.183, 153.978, 321.26,  339.943,
     84.054,  141.563, 268.929, 261.68,  215.679, 205.258},
    {7.069,   275.129, 32.877,  24.828,  18.435, 106.066, 232.099,
     1,       132.723, 23.269,  50.843,  5.334,  197.446, 275.735,
     198.735, 28.293,  294.679, 234.943, 83.542, 73.496},
    {141.236, 162.596, 154.723, 153.674, 159.786, 29.904,  247.623,
     132.782, 1,       153.371, 185.834, 144.435, 173.217, 195.546,
     179.019, 159.125, 156.547, 107.611, 58.763,  69.354},
    {27.11,  290.308, 33.351,  30.656,  29.415,  127.132, 134.475,
     23.425, 154.048, 1,       56.165,  21.329,  218.702, 342.649,
     267.18, 38.005,  381.607, 249.651, 115.757, 101.378},
    {42.321,  324.974, 79.044,  55.598,  43.859,  167.47,  175.968,
     50.674,  185.869, 55.931,  1,       56.673,  248.073, 148.801,
     204.403, 21.157,  342.171, 294.165, 133.209, 129.044},
    {9.209,   279.948, 28.734,  22.704,  28.07,   117.135, 130.943,
     5.48,    149.376, 17.61,   61.736,  1,       204.172, 279.17,
     260.415, 42.758,  365.667, 252.355, 114.698, 91.369},
    {204.529, 310.635, 237.215, 222.165, 214.811, 135.386, 316.096,
     190.667, 173.16,  223.296, 252.358, 229.861, 1,       356.349,
     349.441, 228.036, 314.972, 266.768, 137.577, 126.636},
    {284.393, 276.257, 286.52,  227.285, 274.416, 212.749, 342.619,
     277.733, 153.516, 336.035, 148.591, 271.151, 366.362, 1,
     235.712, 284.305, 148.917, 74.656,  216.897, 259.348},
    {180.516, 118.915, 283.852, 202.361, 197.584, 202.168, 84.107,
     181.458, 179.024, 267.165, 204.386, 251.497, 349.76,  235.791,
     1,       202.265, 93.559,  66.873,  241.053, 242.352},
    {21.591,  309.057, 64.05,   41.096,  10.33,   143.388, 141.558,
     28.256,  159.239, 38.228,  21.291,  29.722,  228.183, 284.382,
     202.297, 1,       362.993, 288.137, 126.377, 115.36},
    {295.71,  25.877,  349.888, 360.115, 317.624, 251.018, 270.281,
     294.423, 156.664, 317.537, 374.436, 355.073, 315.271, 148.589,
     93.654,  393.391, 1,       114.007, 220.726, 231.18},
    {265.392, 191.89,  308,     247.975, 257.657, 134.503, 261.22,
     235.604, 107.616, 248.756, 294.027, 235.052, 266.815, 73.862,
     67.057,  288.337, 114.908, 1,       172.82,  157.804},
    {92.821,  199.057, 110.957, 115.542, 117.678, 43.363,  212.605,
     83.698,  58.722,  115.425, 131.442, 101.325, 139.016, 216.807,
     241.289, 126.485, 219.442, 172.79,  1,       21.106},
    {94.04,   208.082, 111.349, 103.343, 104.072, 38.178,  205.549,
     73.288,  69.407,  105.464, 129.584, 82.41,   126.681, 239.586,
     246.168, 114.586, 262.434, 160.829, 21.182,  1}};

struct TestEnvironment {
  // Graph params
  int num_nodes;
  int degree;
  time_ms_t latency_ms;
  std::string graph_type;

  // Chain params
  double average_block_period_ms;

  // Attacker params
  double evil_rate;
  // The one way latency between attacker and other nodes.
  time_ms_t one_way_latency_ms;
  bool debug_allow_borrow;

  // Simulation params
  time_ms_t termination_time_ms;
};

class NetworkGraph {
 private:
  std::vector<std::vector<time_ms_t>> all_pairs_distances;
  // Do not calculate cut between two node groups,
  // to have greater flexibility on adversary strategy.
 public:
  static constexpr time_ms_t UNREACHABLE = -1;
  static constexpr time_ms_t CONNECTED = 1;

  NetworkGraph(const std::vector<std::vector<time_ms_t>> &edge_map)
      : all_pairs_distances(compute_apsp(edge_map)) {
    std::cout << "NetworkGraph generated. Diameter = "
              << compute_diameter(this->all_pairs_distances) << ".\n";
  };
  NetworkGraph &operator=(const NetworkGraph &) = default;
  NetworkGraph &operator=(NetworkGraph &&) = default;

  static NetworkGraph make_random_graph(int num_nodes, int degree,
                                        time_ms_t latency);
  static NetworkGraph make_random_geodelay_graph(int num_nodes, int degree,
                                                 time_ms_t queueing_latency);
  static NetworkGraph make_graph(const TestEnvironment &env);

  node_id_t get_num_nodes() {
    return (node_id_t)this->all_pairs_distances.size();
  }
  const std::vector<std::vector<time_ms_t>> get_all_pairs_distances() const {
    return this->all_pairs_distances;
  }

  const std::vector<time_ms_t> &get_connectivity(node_id_t node) const {
    return this->all_pairs_distances[(size_t)node];
  }

 private:
  template <typename RandGen>
  std::vector<std::vector<time_ms_t>> static generate_connectivity(
      RandGen rand_gen, node_id_t num_nodes, int degree);
  template <typename RandGen>
  static void assign_random_latency(
      RandGen rand_gen,
      Ref<std::vector<std::vector<time_ms_t>>> edge_map_ms_ref,
      time_ms_t latency_ms);
  template <typename RandGen>
  static void embed_geolatency(
      RandGen rand_gen,
      Ref<std::vector<std::vector<time_ms_t>>> edge_map_ms_ref,
      time_ms_t queueing_latency_ms);
  static std::vector<std::vector<time_ms_t>> compute_apsp(
      const std::vector<std::vector<time_ms_t>> &edge_map_cref);
  static time_ms_t compute_diameter(
      const std::vector<std::vector<time_ms_t>> &all_pair_distances);
};

class Simulator {
 private:
  TestEnvironment env;
  std::unique_ptr<AdversaryStrategy> adversary;
  std::priority_queue<std::tuple<time_ms_t, SimulatorEvent>,
                      std::vector<std::tuple<time_ms_t, SimulatorEvent>>,
                      std::greater<std::tuple<time_ms_t, SimulatorEvent>>>
      event_queue;
  std::vector<NodeLocalView> nodes;
  NetworkGraph network_graph;
  std::optional<std::pair<time_ms_t, char>> nodes_converge;

 public:
  static const time_ms_t CONVERGE_TIME_MS_CRITERIA = 50'000;

  Simulator(const TestEnvironment &env);

  template <typename RandGen>
  time_ms_t simulate(RandGen &rng);

 private:
  void honest_node_broadcast_block(time_ms_t time_ms, int node, char side,
                                   block_id_t block);
  bool is_chain_merged(time_ms_t now);
  node_id_t num_nodes() { return this->network_graph.get_num_nodes(); }
  std::tuple<time_ms_t, SimulatorEvent> pop_event();
  void push_event(time_ms_t time, SimulatorEvent event);
  template <typename RandGen>
  time_ms_t run_test(RandGen &rng);
  void setup_chain();
  void setup_network();
};

#endif  // CPP_ALL_H
