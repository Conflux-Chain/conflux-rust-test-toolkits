#include <boost/asio.hpp>
#include <boost/asio/thread_pool.hpp>
#include <thread>

#include "all.h"
#include "simulator.hpp"

int main() {
  unsigned int cpu_count = std::thread::hardware_concurrency();
  size_t repeats = 100;
  repeats = 1;
  std::cout << "Repeats " << repeats << ".\n";
  boost::asio::thread_pool thread_pool(cpu_count);
  std::vector<time_ms_t> outputs((size_t)repeats);
  std::vector<std::tuple<int, int, time_ms_t, std::string>> graph_params_list{
      {2000, 3, 200, "geolatency"}};
  for (const auto &graph_params : graph_params_list) {
    int num_nodes;
    int degree;
    time_ms_t latency_ms;
    std::string graph_type;
    std::tie(num_nodes, degree, latency_ms, graph_type) = graph_params;

    TestEnvironment env{
        .num_nodes = num_nodes,
        .degree = degree,
        .latency_ms = latency_ms,
        .graph_type = graph_type,
        .average_block_period_ms = 500,
        .evil_rate = 0.2,
        //.evil_rate = 0,
        .one_way_latency_ms = 10,
        // debug only, to find out a good evil rate.
        // .debug_allow_borrow = true,
        .debug_allow_borrow = false,
        .termination_time_ms = 5'400'000,
    };

    auto time_start = std::chrono::steady_clock::now();
    for (size_t i = 0; i < repeats; ++i) {
      boost::asio::post(
          thread_pool, [&outputs, i, &env = std::as_const(env)]() {
            Simulator simulator(env);
            std::default_random_engine rng{std::random_device()()};
            auto termination_time_ms = simulator.simulate(rng);
            std::cout << "result of run " << i << ": " << termination_time_ms
                      << ".\n";
            outputs[i] = termination_time_ms;
          });
    }
    thread_pool.join();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - time_start);
    std::cout << "executed " << repeats << " attack simulations in "
              << duration_ms.count() << "ms.\n";
  }
}
