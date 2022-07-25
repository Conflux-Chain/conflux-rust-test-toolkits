import multiprocessing as mp


def pool():
    return mp.get_context("spawn").Pool(6)
