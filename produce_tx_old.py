#!/usr/bin/env python3
import os
import sys

sys.path.insert(1, os.path.dirname(sys.path[0]))

from conflux.utils import priv_to_addr, encode_hex, decode_hex, normalize_key, ecsign
from test_framework.blocktools import create_transaction, UnsignedTransaction, DEFAULT_PY_TEST_CHAIN_ID, Transaction
from test_framework.util import random
from test_framework.mininode import Transactions
import pickle
import rlp
from datetime import datetime
from multiprocessing import Pool
from web3 import Web3

BASE_PATH = os.path.join(os.path.dirname(os.path.realpath(__file__)), "../..")
account_map = {}





def create_transaction_from_kwargs(input):
    (kwargs, key, sender) = input
    unsigned_tx = UnsignedTransaction(**kwargs)

    rawhash = unsigned_tx.get_rawhash()
    key = normalize_key(key)

    v, r, s = ecsign(rawhash, key)
    v = v - 27

    return dict(v=v, r=r, s=s)





def generate_txs(from_list, to_list, tx_num=None, **kwargs):
    value = kwargs.get("value", 1)
    log = kwargs.get("log", print)

    from_list = list(from_list)
    to_list = list(to_list)

    log(datetime.now().time(), "Build tx_param")
    if tx_num is None:
        tx_param = [(random.choice(from_list), i) for i in to_list]
    else:
        tx_param = [(random.choice(from_list), random.choice(to_list)) for _ in range(tx_num)]

    log(datetime.now().time(), "Assign nonce")
    all_tx_params = [account_map[sender].make_unsigned_transaction(account_map[receiver], value) for
                     (sender, receiver) in tx_param]

    log(datetime.now().time(), "Sign")
    with Pool(6) as p:
        sigs = p.map(create_transaction_from_kwargs, all_tx_params)

    def make_transaction(input, sig):
        (kwargs, key, sender) = input
        unsigned_tx = UnsignedTransaction(**kwargs)
        tx = Transaction(transaction=unsigned_tx, **sig)
        tx._sender = sender
        return tx

    print(datetime.now().time(), "Orignaize")

    all_txs = [make_transaction(param, sig) for (param, sig) in zip(all_tx_params, sigs)]
    print(datetime.now().time(), "Encode")
    encoded_tx = encode_tx_with_meta(all_txs, 200)
    print(datetime.now().time(), "Done")

    return all_txs





# def build_balance_map(encoded_txs, init_range=20000):
#     init_balance = 10000000000000000000000
#     balance_map = dict()
#     for i in range(init_range):
#         balance_map[priv_to_addr((i + 1).to_bytes(32, "big"))] = init_balance
#     for batch_tx in encoded_txs:
#         for (sender, receiver, value, gas) in batch_tx.metas:
#             assert balance_map[sender] > 0
#             balance_map[sender] = balance_map.get(sender, 0) - value - 1 * gas
#             balance_map[receiver] = balance_map.get(receiver, 0) + value
#     return balance_map


def dump_transactions(path, transactions, batch_size):
    encoded_transactions = encode_tx_with_meta(transactions, batch_size)
    with open(path, "wb") as f:
        pickle.dump(batch_size, f)
        pickle.dump(encoded_transactions, f)


if __name__ == "__main__":
    log = lambda x: print(datetime.now().time(), x)

    # txs = generate_txs(range(20_000), range(10_020_000), value=1_000_000_000_000_000_000)
    # dump_transactions(os.path.join(BASE_PATH, "experiment_data/distribute_tx"), txs, 200)

    # for accounts in [3_000_000,5_000_000,10_000_000]:
    #     print(f"Task for {accounts}")
    #     txs = generate_txs(range(20_000, 20_000 + accounts), range(20_000, 20_000 + accounts), tx_num=30_000_000)
    #     dump_transactions(os.path.join(BASE_PATH, f"experiment_data/random_tx_{accounts}"), txs, 200)
    #
    # for accounts in [ 10_000_000,]: #, 1_000_000, 10_000_000]:
    #     print(f"Task for {accounts}")
    #     txs = generate_txs(range(10_000), range(accounts), tx_num=30_000_000)
    #     dump_transactions(os.path.join(BASE_PATH, f"experiment_data/less_sender_{accounts}"), txs, 200)
