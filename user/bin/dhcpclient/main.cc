#include <stdio.h>
#include <sys/socket.h>

int main()
{
    int s = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
}
