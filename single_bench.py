#!/usr/bin/env python3
import sys, os

sys.path.insert(1, os.path.dirname(sys.path[0]))

from eth_utils import decode_hex
import conflux.config

'''
This is the state root for pre-generated genesis accounts in `genesis_secrets.txt`.
'''

STORAGE = os.environ.get("CONFLUX_DEV_STORAGE", "dmpt")

SECRET = "seq_secrets.txt"
if STORAGE == "amt":
    GENESIS_ROOT = "0x5c307b154444a6188037b5f835577e5860d39a942e35f404e1d16aad45b452a5"
elif STORAGE == "mpt":
    GENESIS_ROOT = "0x05b1ba2c15838e58b054ced4497db8bca54053a018f54a5ae5283bcf6a34d5cb"
else:
    GENESIS_ROOT = "0xf7d1918d912bb8d47d34c48297735bcef3220e16efec980ca4a9cb47e90905a9"

conflux.config.default_config["GENESIS_STATE_ROOT"] = decode_hex(GENESIS_ROOT)

BASE_PATH = os.path.join(os.path.dirname(os.path.realpath(__file__)), "../..")

if STORAGE == "amt":
    BINARY = os.path.join(BASE_PATH, "target/amt-db/release/conflux")
elif STORAGE == "mpt":
    BINARY = os.path.join(BASE_PATH, "target/mpt-db/release/conflux")
else:
    BINARY = os.path.join(BASE_PATH, "target/release/conflux")

from test_framework.block_gen_thread import BlockGenThread
from test_framework.test_framework import ConfluxTestFramework
from test_framework.mininode import *
from test_framework.util import *
from conflux.rpc import RpcClient
import pickle
import shutil
from produce_tx import transaction

sys.modules['transaction'] = transaction
from tools.metrics_echarts import generate_metric_chart

from pathlib import Path


class LoadTask:
    def __init__(self, options):
        self.accounts_n = parse_num(options.keys)
        self.tx_n = parse_num(options.tx_num)
        self.warmup_n = parse_num(options.warmup_n)
        assert_greater_than(self.warmup_n, self.accounts_n)
        self.bench_name = options.bench_name

    def warmup_transaction(self):
        yield from []

    def bench_transaction(self):
        return []


class Native(LoadTask):
    def warmup_transaction(self):
        path = os.path.join(BASE_PATH, f"experiment_data/transfer/distribute")
        yield load(path, max(self.accounts_n, self.warmup_n))

    def bench_transaction(self):
        path = os.path.join(BASE_PATH, f"experiment_data/transfer/random_{self.accounts_n}")
        return load(path, self.tx_n)


class NativeLessSender(LoadTask):
    def warmup_transaction(self):
        if self.warmup_n > 20_000:
            path = os.path.join(BASE_PATH, f"experiment_data/transfer/distribute")
            yield load(path, self.warmup_n)

    def bench_transaction(self):
        path = os.path.join(BASE_PATH, f"experiment_data/transfer/less_sender_{self.accounts_n}")
        return load(path, self.tx_n)


class Erc20(LoadTask):
    def warmup_transaction(self):
        path = os.path.join(BASE_PATH, f"experiment_data/erc20/deploy")
        yield load(path, 1)

        path = os.path.join(BASE_PATH, f"experiment_data/erc20/distribute")
        yield load(path, max(self.accounts_n, self.warmup_n))

    def bench_transaction(self):
        path = os.path.join(BASE_PATH, f"experiment_data/erc20/random_{self.accounts_n}")
        return load(path, self.tx_n)


class Erc20LessSender(LoadTask):
    def warmup_transaction(self):
        path = os.path.join(BASE_PATH, f"experiment_data/erc20/deploy")
        yield load(path, 1)

        path = os.path.join(BASE_PATH, f"experiment_data/erc20/distribute")
        yield load(path, max(10_000, self.warmup_n))

    def bench_transaction(self):
        path = os.path.join(BASE_PATH, f"experiment_data/erc20/less_sender_{self.accounts_n}")
        return load(path, self.tx_n)


class SingleBench(ConfluxTestFramework):
    def __init__(self):
        super().__init__()

    def set_test_params(self):
        self.num_nodes = 1
        self.rpc_timewait = 600
        # The file can be downloaded from `https://s3-ap-southeast-1.amazonaws.com/conflux-test/genesis_secrets.txt`
        genesis_file_path = os.path.join(os.path.dirname(os.path.realpath(__file__)), SECRET)
        amt_public_params_path = os.path.join(BASE_PATH, "pp")
        log_config = os.path.join(BASE_PATH, "run/log.yaml")
        self.conf_parameters = dict(
            tx_pool_size=10_000_000,
            heartbeat_timeout_ms=10000000000,
            record_tx_index="false",
            node_type="'archive'",
            executive_trace="true",
            genesis_secrets=f"\"{genesis_file_path}\"",
            amt_public_params=f"'{amt_public_params_path}'",
            log_level="'debug'",
            # log_conf=f"'{log_config}'",
            storage_delta_mpts_cache_size=4_000_000,
            storage_delta_mpts_cache_start_size=2_000_000,
            storage_delta_mpts_slab_idle_size=2_000_000,
        )

    def add_options(self, parser):
        parser.add_argument(
            "--bench-keys",
            dest="keys",
            default="10k",
            type=str)

        parser.add_argument(
            "--warmup-keys",
            dest="warmup_n",
            default="0",
            type=str)

        parser.add_argument(
            "--bench-txs",
            dest="tx_num",
            default="10k",
            type=str)

        parser.add_argument(
            "--bench-mode",
            dest="bench_mode",
            default="normal",
            type=str
        )

        parser.add_argument(
            "--bench-token",
            dest="bench_token",
            default="native",
            type=str
        )

    def setup_network(self):
        self.setup_nodes()

    def setup_nodes(self, binary=None):
        """Override this method to customize test node setup"""
        self.add_nodes(self.num_nodes, binary=[BINARY] * self.num_nodes)
        stdout = None
        # stdout = sys.stdout
        self.start_nodes(stdout=stdout)

    def run_test(self):
        self.log.info(f"Run with backend {STORAGE}, keys {self.options.keys}, txs {self.options.tx_num}")

        # Start mininode connection
        self.node = self.nodes[0]
        n_connections = 5
        p2p_connections = []
        for node in range(n_connections):
            conn = DefaultNode()
            p2p_connections.append(conn)
            self.node.add_p2p_connection(conn)
        network_thread_start()

        for p2p in p2p_connections:
            p2p.wait_for_status()

        # accounts_n = parse_num(self.options.keys)
        # tx_n = parse_num(self.options.tx_num)

        metric_folder = "same-task-1m"

        if self.options.bench_mode == "slow-exec":
            num_txs = 1000
            interval_fixed = 0.1
        else:
            num_txs = 10000
            interval_fixed = 0.02

        block_gen_thread = BlockGenThread([self.node], self.log, num_txs=num_txs, interval_fixed=interval_fixed)
        block_gen_thread.start()

        # path = os.path.join(BASE_PATH, f"experiment_data/transactions/test_erc20")
        # test_txs, hashes = load(path, 2, log=self.log.info)
        # test_txs = test_txs[0].encoded
        # tx_hash1 = hashes[0]
        # tx_hash2 = hashes[1]
        # self.rpc = RpcClient(self.node)
        # print("Send tx")
        # self.node.p2ps[0].send_protocol_packet(test_txs + int_to_bytes(TRANSACTIONS))
        # print("Wait receipt 1")
        # self.rpc.wait_for_receipt(tx_hash1)
        # print("Wait receipt 2")
        # self.rpc.wait_for_receipt(tx_hash2)
        # print("Get tx")
        # # print(self.rpc.get_tx(tx_hash1))
        # # print(self.rpc.get_tx(tx_hash2))
        # print("Get receipt")
        # print(self.rpc.get_transaction_receipt(tx_hash1))
        # print(self.rpc.get_transaction_receipt(tx_hash2))
        # exit(0)

        #  and self.options.bench_token=="native"

        if self.options.bench_token == "native":
            if self.options.bench_mode == "normal":
                loader = Native(self.options)
            elif self.options.bench_mode == "less-sender":
                loader = NativeLessSender(self.options)
            else:
                raise Exception("Unrecognized bench mode")
        elif self.options.bench_token == "erc20":
            if self.options.bench_mode == "normal":
                loader = Erc20(self.options)
            elif self.options.bench_mode == "less-sender":
                loader = Erc20LessSender(self.options)
            else:
                raise Exception("Unrecognized bench mode")
        else:
            raise Exception("Unrecognized bench token")

        def send_tx(i, encoded):
            self.node.p2ps[i % n_connections].send_protocol_packet(encoded.encoded + int_to_bytes(
                TRANSACTIONS))

        base = 0
        for warmup_transaction_batch in loader.warmup_transaction():
            size = send_transaction_with_goodput(warmup_transaction_batch, send_tx, self.node, base=base,
                                                 log=self.log.info)
            wait_transaction_with_goodput(base + size, self.node, log=self.log.info)
            base += size

        if base > 0:
            self.log.info("Waiting 0%")
            for i in range(100):
                if i % 10 == 0:
                    self.log.info(f"Waiting {i}%")
                time.sleep(0.5)

        size = send_transaction_with_goodput(loader.bench_transaction(), send_tx, self.node, base=base,
                                             log=self.log.info)
        wait_transaction_with_goodput(base + size, self.node,
                                      log=self.log.info)

        # send_transaction_with_tps(encoded_transactions, send_tx, send_tps, log=self.log.info)
        # wait_transaction_with_tps(self.node, log=self.log.info)

        metric_file_path = os.path.join(self.options.tmpdir, "node0",
                                        conflux.config.small_local_test_conf["metrics_output_file"][1:-1])
        # tmp_name = os.path.basename(self.options.tmpdir)
        log_name = f"{self.options.bench_mode}-{STORAGE}-{self.options.keys}"
        Path(f"experiment_data/metrics/{metric_folder}").mkdir(parents=True, exist_ok=True)
        output_path = os.path.join(BASE_PATH, f"experiment_data/metrics/{metric_folder}/{log_name}.log")
        shutil.copyfile(metric_file_path, output_path)

        # output_path = os.path.join("/var/www/html/amt-bench/", tmp_name)
        # os.mkdir(output_path)
        # generate_metric_chart(metric_file_path, "system_metrics", output_path)
        # print(f"Goto webpage https://www.cfx.chenxing.li:12643/amt-bench/{tmp_name}/metrics.log.system_metrics.html")

        block_gen_thread.stop()
        block_gen_thread.join()
        self.node.stop()

    def check_account(self, k, target_balance):
        addr = eth_utils.encode_hex(k)
        try:
            balance = RpcClient(self.node).get_balance(addr)
        except Exception as e:
            self.last_fail = time.time()
            self.continue_success = 0
            self.log.info("Fail to get balance, error=%s", str(e))
            time.sleep(0.1)
            return False
        if balance == target_balance:
            self.continue_success += 1
            return True
        else:
            self.last_fail = time.time()
            self.continue_success = 0
            self.log.info("Remote balance:%d, local balance:%d", balance, target_balance)
            return False


# self.node.p2ps[i % n_connections].send_protocol_packet(encoded.encoded + int_to_bytes(
#     TRANSACTIONS))

# def send_transaction_with_tps(encoded_transactions, send, send_tps, log=print):
#     start_time = time.time()
#
#     i = 0
#     batch_size = BATCH_SIZE
#     for encoded in encoded_transactions:
#         i += 1
#         if i * batch_size % send_tps == 0:
#             time_used = time.time() - start_time
#             time.sleep(i * batch_size / send_tps - time_used)
#         send(i, encoded)
#         if i * batch_size % 10000 == 0:
#             log("Sent %d transactions", i * batch_size)
#
#     time_used = time.time() - start_time
#     log(f"Time used: {time_used}")
#
#
# def wait_transaction_with_tps(node, log=print):
#     suc_empty = 0
#     while suc_empty < 10:
#         block_hash = node.cfx_getBestBlockHash()
#         block = node.cfx_getBlockByHash(block_hash, True)
#         tx_num = len(block["transactions"])
#         if tx_num == 0:
#             suc_empty += 1
#         else:
#             suc_empty = 0
#         log("Block height {}, txs {}".format(int(block["height"], 0), tx_num))
#         log(node.getgoodput())
#
#         time.sleep(1)


def send_transaction_with_goodput(encoded_transactions, send, node, base=0, log=print):
    if log is None:
        log = print

    start_time = time.time()

    tasks = enumerate(encoded_transactions)
    i, encoded = next(tasks)
    size = 0
    complete = False

    while not complete:
        time.sleep(1)
        goodtps = int(node.getgoodput())
        log(f"Current goodput: {goodtps}")
        while i * BATCH_SIZE + base < int(goodtps) + 50000:
            send(i, encoded)
            size += encoded.length
            try:
                i, encoded = next(tasks)
            except StopIteration:
                complete = True
                break

        log("Sent %d transactions", i * BATCH_SIZE)

    time_used = time.time() - start_time
    log(f"Time used: {time_used}")
    return size


def wait_transaction_with_goodput(target, node, log=print):
    while True:
        goodput = int(node.getgoodput())
        if goodput == target:
            log(f"Good put reach target {target}")
            break
        else:
            log(f"Good put: {goodput}/{target}")
        time.sleep(1)


def load(path, tx_n):
    if tx_n == 0:
        return []

    if not os.path.isfile(path):
        raise Exception(f"File {path} does not exist")

    f = open(path, "rb")
    size = 0
    encoded_transactions = []
    while size < tx_n:
        encoded_transactions_batch = pickle.load(f)
        for rpc_batch in encoded_transactions_batch:
            size += rpc_batch.length
            encoded_transactions.append(rpc_batch)
            if size >= tx_n:
                break

    return encoded_transactions


def parse_num(input: str):
    if input[-1] in ["k", "K"]:
        return int(input[:-1]) * 1000
    elif input[-1] in ["m", "M"]:
        return int(input[:-1]) * 1000000
    elif input[-1] in ["g", "G"]:
        return int(input[:-1]) * 1000000000
    else:
        return int(input)


if __name__ == "__main__":
    SingleBench().main()
