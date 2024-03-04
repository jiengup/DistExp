##################### defult setting ############################
default_exp_config = "../exp_configs/cloudlab_config.json"
redis_config_file = "redis_config.json"

#################### ssh relate ##################################
username = "root"
exp_root = "/root/exp"

############# storage node binary file path ######################
woker_binary = "../build/basic_server"

#################### redis hash or key relate ####################
# group: _exp_config_group
# key: exp start timestamp
# value: json config
EXP_CONFIG_GROUP = "_exp_config_group"

# group: _ack_hash_group
# key: worker id
# value: exp start timestamp
WORKER_ACK_GROUP = "_worker_ack_group"

# key: _exp_stat_key
# value: "INIT" | "RUNNING" | "END" | "ERROR"
EXP_STAT_KEY = "_exp_stat_key"

# group: _worker_health_group
# key: worker name
# value: report_timestamp:used_core:total_core:user_mem_gb:total_mem_gb
WORKER_HEALTH_GROUP = "_worker_health_group"

