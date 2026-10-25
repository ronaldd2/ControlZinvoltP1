import socket
import struct

MCAST_GRP = "224.0.0.163"
MCAST_PORT = 3333

def main():
    # Create UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)

    # Allow multiple listeners on same machine
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    # Bind to the port
    sock.bind(("", MCAST_PORT))

    # Join multicast group
    mreq = struct.pack("4sl", socket.inet_aton(MCAST_GRP), socket.INADDR_ANY)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)

    print(f"Listening for Shelly EM3 Gen3 UDP telegrams on {MCAST_GRP}:{MCAST_PORT} ...\n")

    while True:
        data, addr = sock.recvfrom(65535)
        print(f"From {addr}:")
        print(data.decode(errors="ignore"))
        print("-" * 60)

if __name__ == "__main__":
    main()
