#include <iostream>

int send_helo() { return 0; }

int recv_helo() { return 0; }

int send_keepalive(int msg_num) { return 0; }

int get_msg(int group_id) { return 0; }

int send_msg(int to_group, int from_group) { return 0; }

int req_status() { return 0; }

int resp_status() { return 0; }

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Port argument must be specified\n";
    return -1;
  }
  char *port = argv[1];
  std::cout << port << "\n";

  return 0;
}
