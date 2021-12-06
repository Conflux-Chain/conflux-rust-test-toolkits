#!/usr/bin/env python3
import os, sys

sys.path.insert(1, os.path.join(sys.path[0], '../..'))
from conflux.config import default_config
from conflux.messages import NewBlock
from test_framework.blocktools import create_chain_of_blocks
from test_framework.mininode import start_p2p_connection


from test_framework.test_framework import ConfluxTestFramework
from test_framework.util import *


'''
An attacker keeps mining a fork chain at a fixed point.
'''
class ForkChainTest(ConfluxTestFramework):
    def set_test_params(self):
        self.num_nodes = 1

    def setup_network(self):
        self.setup_nodes()

    def run_test(self):
        start_p2p_connection(self.nodes)
        n_generate_batch = 1000
        n_attack_blocks = 15000
        self.log.info(f"Attacker start to prepare {n_attack_blocks} blocks")
        attack_blocks = create_chain_of_blocks(parent_hash=self.nodes[0].p2p.genesis, parent_height=0, count=n_attack_blocks)
        self.log.info("Honest node generate")
        for _ in range(int(20000/n_generate_batch)):
            batch_generate(self.nodes[0], n_generate_batch, self.log)
        cnt = self.nodes[0].getblockcount()
        self.log.info("Honest node block count: " + str(cnt))
        for b in attack_blocks:
            self.nodes[0].p2p.send_protocol_msg(NewBlock(b))
        honest_cnt = 20000 + 1
        for _ in range(1000):
            batch_generate(self.nodes[0], n_generate_batch, self.log)
            honest_cnt += n_generate_batch
            cnt = self.nodes[0].getblockcount()
            self.log.info("Honest node block count: " + str(cnt) + " " + str(honest_cnt))
            if honest_cnt + n_attack_blocks == cnt:
                self.log.info("All attack blocks are processed!")
                break


def batch_generate(node, n_blocks, log):
    start = time.time()
    node.generate_empty_blocks(n_blocks)
    elapsed = time.time() - start
    log.info(f"process {n_blocks} blocks with {elapsed} seconds")


if __name__ == '__main__':
    ForkChainTest().main()
