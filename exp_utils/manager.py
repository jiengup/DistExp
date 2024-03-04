import logging
import sys
import os
import json
import redis
from utils import *
import const
import time

logging.basicConfig(format='%(asctime)s: %(levelname)s [%(filename)s:%(lineno)s]: %(message)s',
                    level=logging.INFO, datefmt='%H:%M:%S')
global worker_ip_name_table
global redis_inst
global redis_config
global worker_health_info
global log_dir

def init_exp():
    global redis_inst
    global worker_ip_name_table
    global worker_health_info

    if redis_inst.get(const.EXP_STAT_KEY) != "END":
        logging.warning("Last exp stop unexceptly, please check")
    redis_inst.flushall()
    logging.info("redis initialized")
    worker_ip_name_table = {}
    worker_health_info = {}

def manager_init_worker(worker_ip):
    pip_install_cmd = "sudo apt-get update && sudo apt-get -y install python3-pip"
    ssh(const.username, worker_ip, pip_install_cmd, quiet=True)
    time.sleep(10)
    
    redis_install_cmd = "sudo pip3 install redis psutil"
    ssh(const.username, worker_ip, redis_install_cmd, quiet=True)

    worker_log_dir = os.path.join(const.exp_root, "log")
    mkdir_exp_cmd = "mkdir -p {}".format(worker_log_dir)
    ssh(const.username, worker_ip, mkdir_exp_cmd, quiet=True)

    worker_config_dir = os.path.join(const.exp_root, "configs")
    mkdir_exp_cmd = "mkdir -p {}".format(worker_config_dir)
    ssh(const.username, worker_ip, mkdir_exp_cmd, quiet=True)

def manager_start_worker(worker_ip, exp_start_time, exp_config_file):
    worker_script_file = os.path.join(const.exp_root, "worker.py")
    scp("worker.py", os.path.join(const.exp_root, "worker.py"), const.username, worker_ip)
    scp("const.py", os.path.join(const.exp_root, "const.py"), const.username, worker_ip)
    scp(const.woker_binary, os.path.join(const.exp_root, "server"), const.username, worker_ip)
    scp(const.redis_config_file, os.path.join(const.exp_root, "redis_config.json"), const.username, worker_ip)

    worker_name_cmd = r"sed -i 's/\"worker_name\": \".*\"/\"worker_name\": \"{}\"/g' {}".format(worker_ip_name_table[worker_ip], os.path.join(const.exp_root, "redis_config.json"))
    ssh(const.username, worker_ip, worker_name_cmd, quiet=True)

    worker_log_dir = os.path.join(const.exp_root, "log")
    screen_worker_cmd = "screen -S worker -L -Logfile {} -dm python3 {}".format(os.path.join(worker_log_dir, "{}_{}".format(worker_ip_name_table[worker_ip], exp_start_time)), worker_script_file)
    ssh(const.username, worker_ip, screen_worker_cmd, quiet=True)

    worker_config_dir = os.path.join(const.exp_root, "configs")
    scp(exp_config_file, os.path.join(worker_config_dir, "config_{}.json".format(exp_start_time)), const.username, worker_ip)

def manager_wait_ack(exp_start_time):
    global worker_ip_name_table
    global redis_inst

    ack_timeout_round = redis_config["ack_timeout"] // redis_config["ack_wait_interval"]
    ack_now_round = 0
    worker_name = worker_ip_name_table.values()
    while True:
        ack_pool_kvs = redis_inst.hgetall(const.WORKER_ACK_GROUP)
        logging.info("Waiting workers ack {}/{}".format(str(len(ack_pool_kvs)), len(worker_name)))
        if len(ack_pool_kvs) == len(worker_name):
            worker_set = set(worker_name)
            ack_set = set(ack_pool_kvs.keys())
            if worker_set != ack_set:
                logging.error("Incosistent worker ack")
                manager_abort_exp()
            for ack_worker_name, ack_timestamp in ack_pool_kvs.items():
                if ack_timestamp != exp_start_time:
                    print(ack_timestamp)
                    print(exp_start_time)
                    logging.error("Found inconsistent ACK")
                    manager_abort_exp()
                redis_inst.hdel(const.WORKER_ACK_GROUP, ack_worker_name)
        
            assert redis_inst.hlen(const.WORKER_ACK_GROUP) == 0
            break
        ack_now_round += 1
        if ack_now_round > ack_timeout_round:
            logging.error("Exp ACK wait timeout")
            manager_abort_exp()
        time.sleep(redis_config["ack_wait_interval"])

def manager_start_exp(exp_config_file, init_worker=False):
    global redis_inst
    global worker_ip_name_table
    global redis_config
    global log_dir

    with open(const.redis_config_file, "r") as f:
        redis_config = json.load(f)
    with open(exp_config_file, "r") as f:
        exp_config = json.load(f)

    worker_node_ips = exp_config["storage_node_ip"]
    redis_inst = redis.Redis(
        host = redis_config["redis_host"],
        port = redis_config["redis_port"],
        db = redis_config["redis_db"],
        password = redis_config["redis_pass"],
        decode_responses=True
    )

    init_exp()

    redis_inst.set(const.EXP_STAT_KEY, "INIT")

    exp_start_time = time.strftime("%Y-%m-%d-%H-%M-%S", time.localtime())
    logging.info("Starting exp: {} at {}".format(exp_config["exp_name"], exp_start_time))

    log_dir = os.path.join("../logs", exp_start_time)
    if not os.path.exists(log_dir):
        os.makedirs(log_dir)
    
    for idx, worker_node_ip in enumerate(worker_node_ips):
        worker_name = "worker_" + str(idx)
        worker_ip_name_table[worker_node_ip] = worker_name
        if init_worker:
            manager_init_worker(worker_node_ip)
        manager_start_worker(worker_node_ip, exp_start_time, exp_config_file)
        worker_health_info[worker_name] = {}
        worker_health_info[worker_name]["miss_cnt"] = 0
        worker_health_info[worker_name]["last_report_ts"] = int(time.time())

    redis_inst.hset(const.EXP_CONFIG_GROUP, exp_start_time, os.path.join(os.path.join(const.exp_root, "configs"), "config_{}.json".format(exp_start_time)))

    manager_wait_ack(exp_start_time)
    redis_inst.hdel(const.EXP_CONFIG_GROUP, exp_start_time)
    assert redis_inst.hlen(const.EXP_CONFIG_GROUP) == 0

    manager_exp_run(exp_config_file, exp_start_time)

def manager_check_health():
    global redis_config
    global worker_health_info
    global redis_inst
    
    for worker_name in worker_health_info.keys():
        worker_health = redis_inst.hget(const.WORKER_HEALTH_GROUP, worker_name)
        max_wait_tolaration = 10
        now_wait_round = 0
        while worker_health is None:
            logging.info("Waiting for {} health report".format(worker_name))
            now_wait_round += 1
            if now_wait_round > max_wait_tolaration:
                logging.error("Not found {} health report in the pool".format(worker_name))
                manager_abort_exp()
            worker_health = redis_inst.hget(const.WORKER_HEALTH_GROUP, worker_name)
            time.sleep(1)

        report_timestamp, used_core, total_core, used_mem_gb, total_mem_gb = worker_health.split(":")
        if int(report_timestamp) <= worker_health_info[worker_name]["last_report_ts"]:
            worker_health_info[worker_name]["miss_cnt"] += 1
            logging.warning("{} health info not update. Last report: {} Now catch: {}".format(worker_name, worker_health_info[worker_name]["last_report_ts"], report_timestamp))

            if worker_health_info[worker_name]["miss_cnt"] > redis_config["health_miss_toleration"]:
                logging.error("{} health info miss update reach toleraion".format(worker_name))
                manager_abort_exp()
        
        else:
            worker_health_info[worker_name]["miss_cnt"] = 0
            worker_health_info[worker_name]["last_report_ts"] = int(report_timestamp)
        
        health_info = "{} {} is reporting health info: used_core/total_core: {}/{} used_mem_gb/total_mem_gb: {}/{}".format(
                       worker_name,
                       report_timestamp,
                       used_core,
                       total_core,
                       used_mem_gb,
                       total_mem_gb)
        logging.info(health_info)
    

def manager_exp_run(exp_config_file, exp_start_time):
    global redis_config
    redis_inst.set(const.EXP_STAT_KEY, "RUNNING")
    # TODO:
    # 1. manager start client&mds processes
    # 3. manager spin while exp is over(trace read EOF or exp timeout)
    # 2. manager start a health check thread to print all worker's health per 5s
    start_time = time.time()
    # while time.time() - start_time < 30: # simulate the true exp
    try:
        while True:
            manager_check_health()
            time.sleep(redis_config["health_check_interval"])
    except KeyboardInterrupt:
        manager_exp_end(exp_start_time)

def manager_exp_end(exp_start_time):
    global worker_ip_name_table
    global log_dir

    # 0. kill the client&mds processes
    # 1. set the exp stat to END
    redis_inst.set(const.EXP_STAT_KEY, "END")
    # TODO
    # 2. wait all the worker to ack
    manager_wait_ack(exp_start_time)
    
    # 3. kill all the worker process
    for worker_ip in worker_ip_name_table.keys():
        logging.info("Stop worker screen at ip {}".format(worker_ip))
        stop_screen_cmd = "killall screen"
        ssh(const.username, worker_ip, stop_screen_cmd)
    
    # 4. copy result files to local filesystem
    for worker_ip in worker_ip_name_table.keys():
        target_dir = os.path.join(const.exp_root, exp_start_time)
        target = os.path.join(target_dir, "*")
        scp(log_dir+"/", target, const.username, worker_ip, tranverse=True)

def manager_abort_exp():
    global worker_ip_name_table
    # TODO: kill the server&mds process

    redis_inst.set(const.EXP_STAT_KEY, "ERROR")

    raise RuntimeError

if __name__ == "__main__":

    from argparse import ArgumentParser
    from distutils.util import strtobool

    parser = ArgumentParser(description="manage task and worker")

    parser.add_argument("--config",
                        type=str,
                        default=const.default_exp_config,
                        help="Exp json config file")
    parser.add_argument("--init_worker",
                        type=lambda x: bool(strtobool(x)),
                        default=False,
                        help="whether init worker",
                        )

    ap = parser.parse_args()
    
    # if len(sys.argv) == 1:
    #     logging.info("Loading experiemnt list: {}".format(const.default_exp_list))
    #     if not os.path.exists(const.default_exp_list):
    #         raise FileNotFoundError(const.default_exp_list)
    #     with open(const.default_exp_list, "r") as f:
    #         exp_config_files = f.readlines()
    #     for exp_conf_f in exp_config_files:
    #         manager_start_exp(exp_conf_f)
    
    logging.info("Single experiment with config: {}".format(ap.config))
    manager_start_exp(ap.config)

    # logging.info("Loading experiemnt list: {}".format(config_arg))
    # if not os.path.exists(config_arg):
    #     raise FileNotFoundError(config_arg)
    # with open(config_arg, "r") as f:
    #     exp_config_files = f.readlines()
    # for exp_conf_f in exp_config_files:
    #     manager_start_exp(exp_conf_f)