#ifndef HelpLib_hpp
#define HelpLib_hpp

#include <arpa/inet.h>
#include <limits.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

std::string getMyIp()
{
    std::string MyIP;
    const char* statistics_server = "172.104.236.166";
    int statistics_port = 5797;

    struct sockaddr_in serv {
    };

    int sock = socket(AF_INET, SOCK_STREAM, 0);

    //Socket could not be created
    if (sock < 0) {
        perror("Socket error");
    }

    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = inet_addr(statistics_server);
    serv.sin_port = htons(statistics_port);

    connect(sock, (const struct sockaddr*)&serv, sizeof(serv));

    struct sockaddr_in name {
    };
    socklen_t namelen = sizeof(name);
    getsockname(sock, (struct sockaddr*)&name, &namelen);

    char buffer[100];
    const char* p = inet_ntop(AF_INET, &name.sin_addr, buffer, 100);

    if (p != nullptr) {
        MyIP = std::string(buffer);
        //        std::cout << MyIP << std::endl;
    } else {
        MyIP = "0.0.0.0";
    }

    close(sock);

    return MyIP;
}

std::string getHostName()
{
    char hostname[HOST_NAME_MAX];
    gethostname(hostname, HOST_NAME_MAX);
    return std::string(hostname);
}

bool check_addr(const std::string& addr)
{
    if (addr[0] == '0' && addr[1] == 'x') {
        for (uint i = 2; i < addr.length(); i++) {
            if (!isxdigit(addr[i])) {
                return false;
            }
        }
    } else {
        return false;
    }
    return true;
}

#endif /* HelpLib_hpp */
