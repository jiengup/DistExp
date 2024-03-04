import logging
import subprocess
import sys

def ssh(username, host, cmd, quiet=False):
    if quiet:
        ssh_cmd = 'ssh -q -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no ' + \
            username + '@' + host + ' \"' + cmd + '\"'
    else:
        ssh_cmd = 'ssh -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no ' + \
            username + '@' + host + ' \"' + cmd + '\"'
    logging.info(ssh_cmd)
    p = subprocess.Popen(ssh_cmd, shell=True, stdout=sys.stdout, stderr=sys.stderr)
    return p

def scp(source, dest, username, host, identity_file='', tranverse=False, quiet=False):
    _stdout = None
    _stderr = None
    if quiet:
        _stdout = subprocess.DEVNULL
        _stderr = subprocess.DEVNULL
    target = "{}@{}:{}".format(username, host, dest)
    if identity_file != '':
        if tranverse:
            cmd = 'scp -r -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null ' + \
                '-i ' + str(identity_file) + ' ' + str(target) + ' ' + str(source)
        else:
            cmd = 'scp -r -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null ' + \
                '-i ' + str(identity_file) + ' ' + str(source) + ' ' + str(target)
        logging.info(cmd)
        subprocess.run(cmd, shell=True, stdout=_stdout, stderr=_stderr)
    else:
        if tranverse:
            cmd = 'scp -r -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null ' + \
                str(target) + ' ' + str(source)
        else:
            cmd = 'scp -r -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null ' + \
                str(source) + ' ' + str(target)
        logging.info(cmd)
        subprocess.run(cmd, shell=True, stdout=_stdout, stderr=_stderr)