/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mnaouss <mnaouss@student.42beirut.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/01 20:43:18 by mnaouss           #+#    #+#             */
/*   Updated: 2026/09/01 23:24:24 by mnaouss          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVER_HPP
#define SERVER_HPP

#include <sys/socket.h>
#include <iostream>
#include <unistd.h>
#include <netinet/in.h>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <vector>
#include "Client.hpp"
#include <map>
#include <utility>
#include <cctype>
#include <sstream>

class Server
{
private:
    int serverFd;
    std::map<int, Client> clients;
    int         port;
    std::string password;
    
    std::vector<struct pollfd>  pollFds;
    bool setNonBlocking(int fd);
    void acceptClient();
    bool readClient(std::size_t index);
    void disconnectClient(std::size_t index);
    bool setupSocket();
    void processMessage(int clientFd, const std::string &message);

public:
    Server(int serverPort, const std::string &serverPassword);
    ~Server();

    int run();
};

#endif
