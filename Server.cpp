/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mnaouss <mnaouss@student.42beirut.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/01 20:44:33 by mnaouss           #+#    #+#             */
/*   Updated: 2026/09/02 18:15:20 by mnaouss          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include <unistd.h>

Server::Server(
    int serverPort,
    const std::string &serverPassword
)
    : serverFd(-1),
      port(serverPort),
      password(serverPassword)
{
}


Server::~Server()
{
    for (std::size_t i = 0; i < pollFds.size(); i++)
    {
        if (pollFds[i].fd != serverFd && pollFds[i].fd != -1)
            close(pollFds[i].fd);
    }

    if (serverFd != -1)
        close(serverFd);
}

int Server::run()
{
    if (!setupSocket())
        return 1;

    while (true)
    {
        int pollResult = poll(&pollFds[0], pollFds.size(), -1);

        if (pollResult == -1)
        {
            std::cerr << "poll failed: "
                    << strerror(errno) << std::endl;
            return 1;
        }

        for (std::size_t i = 0; i < pollFds.size();)
        {
            short readyEvents = pollFds[i].revents;
            bool removed = false;

            if (readyEvents & POLLIN)
            {
                if (pollFds[i].fd == serverFd)
                {
                    acceptClient();
                }
                else if (!readClient(i))
                {
                    removed = true;
                }
            }

            if (!removed &&
                pollFds[i].fd != serverFd &&
                (readyEvents & POLLOUT))
            {
                if (!writeClient(i))
                    removed = true;
            }

            if (!removed)
                i++;
        }
    }


    return 0;
}



bool Server::setNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);

    if (flags == -1)
    {
        std::cerr << "fcntl F_GETFL failed: "
                << strerror(errno) << std::endl;
        return false;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        std::cerr << "fcntl F_SETFL failed: "
                << strerror(errno) << std::endl;
        return false;
    }

    return true;

}


void Server::acceptClient()
{
    int clientFd = accept(serverFd, NULL, NULL);

    if (clientFd == -1)
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            std::cerr << "accept failed: "
                      << strerror(errno) << std::endl;
        }
        return;
    }

    if (!setNonBlocking(clientFd))
    {
        close(clientFd);
        return;
    }

    struct pollfd clientPoll;

    clientPoll.fd = clientFd;
    clientPoll.events = POLLIN;
    clientPoll.revents = 0;

    pollFds.push_back(clientPoll);
    clients.insert(std::make_pair(clientFd, Client(clientFd)));

    std::cout << "Client connected! fd = "
              << clientFd << std::endl;
}

void Server::disconnectClient(std::size_t index)
{
    int clientFd = pollFds[index].fd;
    std::cout << "Client "
              << pollFds[index].fd
              << " disconnected"
              << std::endl;

    close(clientFd);
    clients.erase(clientFd);
    pollFds.erase(pollFds.begin() + index);
}


bool Server::readClient(std::size_t index)
{

    int clientFd = pollFds[index].fd;

    std::map<int, Client>::iterator clientIt =
        clients.find(clientFd);

    if (clientIt == clients.end())
    {
        std::cerr << "Client state not found for fd "
                << clientFd << std::endl;

        disconnectClient(index);
        return false;
    }

    
    char buffer[1024];

    ssize_t bytesReceived = recv(
        clientFd,
        buffer,
        sizeof(buffer) - 1,
        0
    );

    if (bytesReceived > 0)
    {
        clientIt->second.appendData(
            buffer,
            static_cast<std::size_t>(bytesReceived)
        );

        while (clientIt->second.hasCompleteMessage())
        {
            std::string message =
                clientIt->second.extractMessage();

            processMessage(clientFd, message);
        }

        buffer[bytesReceived] = '\0';

        std::cout << "Client "
                  << pollFds[index].fd
                  << " sent: "
                  << buffer;

        return true;
    }

    if (bytesReceived == 0)
    {
        disconnectClient(index);
        return false;
    }

    if (errno != EAGAIN && errno != EWOULDBLOCK)
    {
        std::cerr << "recv failed: "
                  << strerror(errno)
                  << std::endl;

        disconnectClient(index);
        return false;
    }

    return true;
}

bool Server::setupSocket()
{
    serverFd = socket(AF_INET, SOCK_STREAM, 0);

    if (serverFd == -1)
    {
        std::cerr << "socket failed: "
                  << strerror(errno) << std::endl;
        return false;
    }

    if (!setNonBlocking(serverFd))
        return false;

    int option = 1;

    if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR,
                   &option, sizeof(option)) == -1)
    {
        std::cerr << "setsockopt failed: "
                  << strerror(errno) << std::endl;
        return false;
    }

    struct sockaddr_in address;

    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(serverFd,
             reinterpret_cast<struct sockaddr *>(&address),
             sizeof(address)) == -1)
    {
        std::cerr << "bind failed: "
                  << strerror(errno) << std::endl;
        return false;
    }

    if (listen(serverFd, 10) == -1)
    {
        std::cerr << "listen failed: "
                  << strerror(errno) << std::endl;
        return false;
    }

    struct pollfd serverPoll;

    serverPoll.fd = serverFd;
    serverPoll.events = POLLIN;
    serverPoll.revents = 0;

    pollFds.push_back(serverPoll);

    std::cout << "Server listening on port " << port << std::endl;

    return true;
}

void Server::processMessage(
    int clientFd,
    const std::string &message
)
{
    std::istringstream stream(message);
    std::string command;

    if (!(stream >> command))
        return;

    for (std::size_t i = 0; i < command.size(); i++)
    {
        command[i] = std::toupper(
            static_cast<unsigned char>(command[i])
        );
    }

    std::vector<std::string> parameters;
    std::string parameter;

    while (stream >> parameter)
    {
        if (parameter[0] == ':')
        {
            std::string trailing;

            std::getline(stream, trailing);
            parameters.push_back(
                parameter.substr(1) + trailing
            );
            break;
        }

        parameters.push_back(parameter);
    }

    std::cout << "Client " << clientFd
              << " command: " << command
              << std::endl;

    for (std::size_t i = 0; i < parameters.size(); i++)
    {
        std::cout << "parameter[" << i << "]: "
                  << parameters[i]
                  << std::endl;
    }

    std::map<int, Client>::iterator clientIt =
    clients.find(clientFd);

    if (clientIt == clients.end())
        return;

    if (command == "PASS")
        handlePass(clientIt->second, parameters);
    else if (command == "NICK")
        handleNick(clientIt->second, parameters);
    else if (command == "USER")
        handleUser(clientIt->second, parameters);
}


void Server::handlePass(
    Client &client,
    const std::vector<std::string> &parameters
)
{
    if (parameters.empty())
    {
        std::cout << "PASS requires a password"
                  << std::endl;
        return;
    }

    if (client.isPasswordAccepted())
    {
        std::cout << "Client "
                  << client.getFd()
                  << " already provided the password"
                  << std::endl;
        return;
    }

    if (parameters[0] != password)
    {
        std::cout << "Incorrect password from client "
                  << client.getFd()
                  << std::endl;
        return;
    }

    client.setPasswordAccepted(true);

    std::cout << "Password accepted for client "
              << client.getFd()
              << std::endl;
    tryRegister(client);
}

bool Server::isNicknameInUse(
    const std::string &nickname,
    int currentFd
) const
{
    std::map<int, Client>::const_iterator it;

    for (it = clients.begin(); it != clients.end(); it++)
    {
        if (it->first != currentFd &&
            it->second.getNickname() == nickname)
        {
            return true;
        }
    }

    return false;
}


void Server::handleNick(
    Client &client,
    const std::vector<std::string> &parameters
)
{
    if (!client.isPasswordAccepted())
    {
        std::cout << "Client must provide PASS first"
                  << std::endl;
        return;
    }

    if (parameters.empty() || parameters[0].empty())
    {
        std::cout << "NICK requires a nickname"
                  << std::endl;
        return;
    }

    if (isNicknameInUse(parameters[0], client.getFd()))
    {
        std::cout << "Nickname already in use: "
                  << parameters[0]
                  << std::endl;
        return;
    }

    client.setNickname(parameters[0]);

    std::cout << "Nickname set for client "
              << client.getFd()
              << ": "
              << client.getNickname()
              << std::endl;
    tryRegister(client);
}

void Server::handleUser(
    Client &client,
    const std::vector<std::string> &parameters
)
{
    if (!client.isPasswordAccepted())
    {
        std::cout << "Client must provide PASS first"
                  << std::endl;
        return;
    }

    if (client.isRegistered())
    {
        std::cout << "Client is already registered"
                  << std::endl;
        return;
    }

    if (parameters.size() < 4)
    {
        std::cout << "USER requires four parameters"
                  << std::endl;
        return;
    }

    client.setUserInfo(parameters[0], parameters[3]);

    tryRegister(client);
}

void Server::tryRegister(Client &client)
{
    if (client.isRegistered())
        return;

    if (!client.isPasswordAccepted())
        return;

    if (client.getNickname().empty())
        return;

    if (client.getUsername().empty())
        return;

    client.setRegistered(true);

    std::string welcome =
        ":ircserv 001 " + client.getNickname() +
        " :Welcome to the Internet Relay Network " +
        client.getNickname();

    queueReply(client, welcome);
}

void Server::queueReply(
    Client &client,
    const std::string &message
)
{
    client.queueMessage(message + "\r\n");

    for (std::size_t i = 0; i < pollFds.size(); i++)
    {
        if (pollFds[i].fd == client.getFd())
        {
            pollFds[i].events |= POLLOUT;
            return;
        }
    }
}


bool Server::writeClient(std::size_t index)
{
    int clientFd = pollFds[index].fd;

    std::map<int, Client>::iterator clientIt =
        clients.find(clientFd);

    if (clientIt == clients.end())
    {
        disconnectClient(index);
        return false;
    }

    Client &client = clientIt->second;

    if (!client.hasPendingOutput())
    {
        pollFds[index].events &= ~POLLOUT;
        return true;
    }

    const std::string &output =
        client.getOutputBuffer();

    ssize_t bytesSent = send(
        clientFd,
        output.c_str(),
        output.size(),
        MSG_NOSIGNAL
    );

    if (bytesSent > 0)
    {
        client.removeSentData(
            static_cast<std::size_t>(bytesSent)
        );

        if (!client.hasPendingOutput())
            pollFds[index].events &= ~POLLOUT;

        return true;
    }

    if (bytesSent == -1 &&
        (errno == EAGAIN || errno == EWOULDBLOCK))
    {
        return true;
    }

    std::cerr << "send failed for client "
              << clientFd << ": "
              << strerror(errno)
              << std::endl;

    disconnectClient(index);
    return false;
}
