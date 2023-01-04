import socket
import select
import os
import sys
import argparse
import logging

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

if __name__ == '__main__':
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