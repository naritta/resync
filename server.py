import socket
import select
import os
import sys
import argparse
import logging

APIKEY = "apikey1234"
CONNECT_PORT = 7777
SYNC_PORT = 7778
MAX_LISTEN = 32

def create_server_socket(port):
    server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_sock.bind(('', port))
    server_sock.listen(MAX_LISTEN)

    logging.info('Server listen port:{}'.format(port))
    return server_sock

def authenticate(sock):
    apikey = sock.recv(32)
    if APIKEY.encode() in apikey:
        sock.send("OK".encode())
        return True
    else:
        sock.send("FAIL".encode())
        return False

def accept_loop(connect_sock, sync_sock):
    logging.info('Ready for accept')
    descriptors = [connect_sock, sync_sock]
    connect_sockets = []
    sync_sockets = []

    while True:
        r, _, _ = select.select(descriptors, [], [])
        for sock in r:
            if sock == connect_sock:
                if len(descriptors) > MAX_LISTEN:
                    logging.info("the number of sockets is over limit.")
                    sock.close()
                new_sock, (remote_host, remote_remport) = sock.accept()
                logging.info('[FD:{}]Accept:{}:{}'.format(new_sock.fileno(), remote_host, remote_remport))

                if authenticate(new_sock):
                    descriptors.append(new_sock)
                    # Add to connect_sockets, and use this for broadcasting.
                    connect_sockets.append(new_sock)
                    logging.info("authentication succeeded.")
                else:
                    new_sock.close()
                    logging.info("authentication failed.")
            elif sock == sync_sock:
                if len(descriptors) > MAX_LISTEN:
                    logging.info("the number of sockets is over limit.")
                    sock.close()
                new_sock, (remote_host, remote_remport) = sock.accept()
                logging.info('[FD:{}]Accept:{}:{}'.format(new_sock.fileno(), remote_host, remote_remport))

                descriptors.append(new_sock)
                sync_sockets.append(new_sock)
            elif sock in sync_sockets:
                logging.info("sync starts.")
                data = sync(sock, connect_sockets)
                if not data:
                    sock.close()
                    descriptors.remove(sock)

# Sync client sends all data for directries and files, and save them in data_list
def sync(sock, connect_sockets):
    data_list = []
    while True:
        header = sock.recv(RECV_SIZE_MAX)
        header_str = header
        if "DONE".encode() in header_str:
            logging.info("all file received.\n")
            break
        logging.info("header received.")
        sock.send(header)
        data = None
        while True:
            rec_data = sock.recv(RECV_SIZE_MAX)
            if data:
                data += rec_data
            else:
                data = rec_data
            if len(rec_data)<RECV_SIZE_MAX:
                break
        data_list.append((header, data))
        logging.info("data received.")
        sock.send("data received.".encode())

    # broadcast to other connecting client
    logging.info("start broadcast.")
    for connect_socket in connect_sockets:
        for header, data in data_list:
            connect_socket.send(header)
            sign = connect_socket.recv(RECV_SIZE_MAX)
            if "OK".encode() in sign:
                logging.info("sending data.")
                connect_socket.send(data)
            else:
                connect_socket.send("dummy".encode())
                logging.info("continue sync.")
            sign = connect_socket.recv(RECV_SIZE_MAX)
            logging.info("one file sync done.")
    logging.info("broadcast done.\n")

def main(daemon=False):
    descriptors = []
    connect_sock = create_server_socket(CONNECT_PORT)
    sync_sock = create_server_socket(SYNC_PORT)
    descriptors.append(connect_sock)
    descriptors.append(sync_sock)

    try:
        accept_loop(connect_sock, sync_sock)
    except KeyboardInterrupt:
        logging.info("finished.")
        connect_sock.close()
        sync_sock.close()

def daemonize():
    pid = os.fork()
    if pid > 0:
        pid_file = open('python_daemon.pid','w')
        pid_file.write(str(pid)+"\n")
        pid_file.close()
        print("daemon process starts in PID:", pid)
        sys.exit()
    if pid == 0:
        main(True)

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("--daemonize", help="daemonize this app.", action="store_true")
    args = parser.parse_args()

    file_handler = logging.FileHandler(f"log.txt")
    file_handler.setFormatter(logging.Formatter("%(asctime)s@ %(name)s [%(levelname)s] %(funcName)s: %(message)s"))
    stdout_handler = logging.StreamHandler(stream=sys.stdout)

    if args.daemonize:
        logging.basicConfig(level=logging.INFO, handlers=[file_handler])
        daemonize()
    else:
        logging.basicConfig(level=logging.INFO, handlers=[stdout_handler, file_handler])
        main()
