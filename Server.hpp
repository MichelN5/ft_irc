/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mnaouss <mnaouss@student.42beirut.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/01 20:43:18 by mnaouss           #+#    #+#             */
/*   Updated: 2026/09/04 13:31:48 by mnaouss          ###   ########.fr       */
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
#include "Channel.hpp"
#include <map>
#include <utility>
#include <cctype>
#include <sstream>

class Server
{
private:
    int serverFd;
    std::map<int, Client> clients;
    std::map<std::string, Channel> channels;
    int         port;
    std::string password;
    
    std::vector<struct pollfd>  pollFds;
    bool setNonBlocking(int fd);
    void acceptClient();
    bool readClient(std::size_t index);
    void disconnectClient(
        std::size_t index,
        const std::string &reason
    );
    void removeClientFromChannels(
        Client &client,
        const std::string &reason
    );
    bool setupSocket();
    void processMessage(int clientFd, const std::string &message);
    void broadcastToChannel(
        Channel &channel,
        const std::string &message
    );
    void sendNames(Client &client, const Channel &channel);
    std::string getClientPrefix(const Client &client) const;

public:
    Server(int serverPort, const std::string &serverPassword);
    ~Server();

    int run();
    
    void handlePass(
        Client &client,
        const std::vector<std::string> &parameters
        );
    void handleNick(
        Client &client,
        const std::vector<std::string> &parameters
    );

    bool isNicknameInUse(
        const std::string &nickname,
        int currentFd
    ) const;

    bool isValidNickname(
        const std::string &nickname
    ) const;

    std::string normalizeName(
        const std::string &name
    ) const;

    void handleUser(
        Client &client,
        const std::vector<std::string> &parameters
    );
    void handleJoin(
        Client &client,
        const std::vector<std::string> &parameters
    );
    void handlePart(
        Client &client,
        const std::vector<std::string> &parameters
    );
    void handleQuit(
        Client &client,
        const std::vector<std::string> &parameters
    );
    void handlePing(
        Client &client,
        const std::vector<std::string> &parameters
    );
    void handleCap(
        Client &client,
        const std::vector<std::string> &parameters
    );

    void tryRegister(Client &client);

    void queueReply(
        Client &client,
        const std::string &message
    );

    void sendNumeric(
        Client &client,
        const std::string &code,
        const std::string &parameters
    );

    bool writeClient(std::size_t index);


};

#endif
