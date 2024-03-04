import redis
import logging
import json
import os
import time
import const
import subprocess
import psutil

logging.basicConfig(format='%(asctime)s: %(levelname)s [%(filename)s:%(lineno)s]: %(message)s',
                    level=logging.INFO, datefmt='%H:%M:%S')

global worker_name
global redis_config
global redis_inst

def worker_init():
    global redis_config
    global redis_inst
    global worker_name
    
    print(os.path.join(const.exp_root, const.redis_config_file))
    with open(os.path.join(const.exp_root, const.redis_config_file), "r") as f:
        redis_config = json.load(f)
    redis_inst = redis.Redis(
        host = redis_config["redis_host"],
        port = redis_config["redis_port"],
        db = redis_config["redis_db"],
        password = redis_config["redis_pass"],
        decode_responses=True
    )
    worker_name = redis_config["worker_name"]

def worker_ack(exp_start_time):
    global redis_inst
    global worker_name

    redis_inst.hset(const.WORKER_ACK_GROUP, worker_name, exp_start_time)
    logging.info("Acked to manager")

def worker_run_exp(exp_start_time, config_f):
    global worker_name
    global redis_config
    global redis_inst
    # start a .server process, keep its pid
    # wait until exp_stat != "RUNNING"
    # push health status periodly
    server_out_dir = os.path.join(const.exp_root, exp_start_time)
    if not os.path.exists(server_out_dir):
        os.mkdir(server_out_dir)

    server_process_cmd = "export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib && " + os.path.join(const.exp_root, "server")
    server_process_cmd += " {} {} {}".format(worker_name.split("_")[-1], config_f, os.path.join(server_out_dir, "{}.log".format(worker_name)))
    logging.info("Starting process: {}".format(server_process_cmd))
    server_stdout = open(os.path.join(server_out_dir, "{}.stdout".format(worker_name)), "w")
    server_stderr = open(os.path.join(server_out_dir, "{}.stderr".format(worker_name)), "w")
    server_process = subprocess.Popen(server_process_cmd,
                                      shell=True,
                                      stdout=server_stdout,
                                      stderr=server_stderr)
    worker_ack(exp_start_time)

    worker_wait_exp_init()
    
    logging.info("Start heartbeating")
    while worker_check_exp_running():
        worker_report_health()
        time.sleep(redis_config["health_report_interval"])

    server_process.kill()
    server_stdout.flush()
    server_stderr.flush()
    server_stdout.close()
    server_stderr.close()
    if redis_inst.get(const.EXP_STAT_KEY) == "END": # exp ends normally
        logging.info("Exp {} ends normally".format(exp_start_time))
        worker_ack(exp_start_time)
    else:
        logging.error("Exp stop unexpectly!")
        exit(0)

def worker_report_health():
    global worker_name
    
    cpu_percent = psutil.cpu_percent(interval=2)
    total_core = psutil.cpu_count()
    mem = psutil.virtual_memory()
    total_mem_gb, avail_mem_gb = mem.total / (1024*1024*1024), mem.available / (1024*1024*1024)
    total_core, used_core = total_core, cpu_percent / 100 * total_core
    used_mem_gb = total_mem_gb - avail_mem_gb
    report_timestamp = int(time.time())
    
    health_info = "{} {} health info: used_core/total_core: {:.2f}/{:.2f} used_mem_gb/total_mem_gb: {:.2f}/{:.2f}".format(
        worker_name,
        report_timestamp,
        used_core,
        total_core,
        used_mem_gb,
        total_mem_gb
    )
    logging.info(health_info)
    health_str = "{}:{:.2f}:{:.2f}:{:.2f}:{:.2f}".format(report_timestamp, used_core, total_core, used_mem_gb, total_mem_gb)
    redis_inst.hset(const.WORKER_HEALTH_GROUP, worker_name, health_str)

def worker_wait_exp_init():
    global redis_config

    while redis_inst.get(const.EXP_STAT_KEY) == "INIT":
        time.sleep(redis_config["ack_wait_interval"])

    assert redis_inst.get(const.EXP_STAT_KEY) == "RUNNING"

def worker_check_exp_running():
    exp_stat = redis_inst.get(const.EXP_STAT_KEY)
    if exp_stat is None:
        logging.error("Exp status key not exist")
        raise RuntimeError
    return exp_stat == "RUNNING"

# def worker_start_exp(exp_start_time, exp_config):
#     configs_dir = os.path.join(const.exp_root, "configs")
#     if not os.path.exists(configs_dir):
#         os.mkdir(configs_dir)
#     exp_config_f = os.path.join(configs_dir, "config_{}.json".format(exp_start_time))
#     with open(exp_config_f, "w") as f:
#         json.dump(exp_config, f)
#     worker_run_exp(exp_start_time, exp_config_f)

def worker_run():
    global redis_inst
    global worker_name

    while True:
        exp_config = redis_inst.hlen(const.EXP_CONFIG_GROUP)
        if exp_config == 0:
            logging.info("Waiting for exp assign...")
        elif exp_config == 1:
            for exp_start_time, exp_config_f in redis_inst.hscan_iter(const.EXP_CONFIG_GROUP):
                logging.info("Catch an experiment: {}".format(exp_start_time))
                worker_run_exp(exp_start_time, exp_config_f)
        else:
            raise NotImplementedError
        time.sleep(redis_config["exp_interval_between_query"])

if __name__ == "__main__":
    worker_init()
    worker_run()