#!/usr/bin/env python3
import sys, os
sys.path.insert(1, os.path.dirname(sys.path[0]))
from eth_utils import decode_hex
from rlp.sedes import Binary, BigEndianInt

from conflux import utils
from conflux.rpc import RpcClient
from conflux.utils import encode_hex, bytes_to_int, priv_to_addr, parse_as_int
from test_framework.blocktools import create_block
from test_framework.test_framework import ConfluxTestFramework
from test_framework.mininode import *
from test_framework.util import *

FORK_HEIGHT = 20

class HardForkTest(ConfluxTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.conf_parameters = {
            "tanzanite_transition_height": f"{FORK_HEIGHT}",
        }

    def setup_network(self):
        self.setup_nodes()
        self.add_nodes(1)
        node_index = len(self.nodes) - 1
        initialize_datadir(self.options.tmpdir, node_index, {})
        self.start_node(node_index)
        start_p2p_connection(self.nodes)
        connect_sample_nodes(self.nodes, self.log, sample=2)

    def run_test(self):
        new_node = RpcClient(self.nodes[0])
        new_node2 = RpcClient(self.nodes[1])
        old_node = RpcClient(self.nodes[len(self.nodes) - 1])
        old_node.generate_empty_blocks(FORK_HEIGHT - 1)
        sync_blocks(self.nodes)
        old_fork = old_node.generate_empty_blocks(FORK_HEIGHT)
        new_fork = new_node.generate_empty_blocks(2 * FORK_HEIGHT)
        wait_until(lambda: old_node.get_block_count() == 4 * FORK_HEIGHT)
        wait_until(lambda: new_node.get_block_count() == 3 * FORK_HEIGHT)
        wait_until(lambda: new_node2.get_block_count() == 3 * FORK_HEIGHT)
        for h in new_fork:
            b = new_node.block_by_hash(h)
            assert_equal(int(b["blame"], 0), 0)
        for h in old_fork:
            assert_equal(new_node.block_by_hash(h), None)
            assert_equal(new_node2.block_by_hash(h), None)


if __name__ == "__main__":
    HardForkTest().main()
