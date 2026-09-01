/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mnaouss <mnaouss@student.42beirut.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/01 20:44:33 by mnaouss           #+#    #+#             */
/*   Updated: 2026/09/01 23:25:34 by mnaouss          ###   ########.fr       */
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

        for (std::size_t i = 0; i < pollFds.size(); i++)
        {
            if (pollFds[i].revents & POLLIN)
            {
                if (pollFds[i].fd == serverFd)
                {

                    acceptClient();
                }

                else if(!readClient(i))
                {
                    i--;
                    
                }

                }
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
}
