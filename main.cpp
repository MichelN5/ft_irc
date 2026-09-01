/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mnaouss <mnaouss@student.42beirut.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/24 20:54:20 by mnaouss           #+#    #+#             */
/*   Updated: 2026/09/01 17:56:41 by mnaouss          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <sys/socket.h>
#include <iostream>
#include <unistd.h>
#include <netinet/in.h>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <vector>

int main()
{
    int serverFd = socket(AF_INET, SOCK_STREAM, 0);

    if (serverFd == -1)
    {
        std::cerr << "socket failed: " << strerror(errno) << std::endl;
        return 1;
    }

    std::cout << "Socket created. fd = " << serverFd << std::endl;

    int flags = fcntl(serverFd, F_GETFL, 0);

    if (flags == -1)
    {
        std::cerr << "fcntl F_GETFL failed: "
                << strerror(errno) << std::endl;
        close(serverFd);
        return 1;
    }

    if (fcntl(serverFd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        std::cerr << "fcntl F_SETFL failed: "
                << strerror(errno) << std::endl;
        close(serverFd);
        return 1;
    }


    int option = 1;

    if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR,
                &option, sizeof(option)) == -1)
    {
        std::cerr << "setsockopt failed: "
                << strerror(errno) << std::endl;
        close(serverFd);
        return 1;
    }

    struct sockaddr_in address;
    std::memset(&address, 0, sizeof(address));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(6667);

    if (bind(serverFd, (struct sockaddr *)&address, sizeof(address)) == -1)
    {
        std::cerr << "bind failed" << std::endl;
        close(serverFd);
        return 1;
    }

    std::cout << "Bound to port 6667" << std::endl;




    if(listen(serverFd, 10) == -1)
    {
        std::cerr << "listen failed" << std::endl;
        close(serverFd);
        return 1;
    }

    std::cout << "Waiting for a client..." << std::endl;

    std::vector<struct pollfd> fds;

    struct pollfd serverPoll;

    serverPoll.fd = serverFd;
    serverPoll.events = POLLIN;
    serverPoll.revents = 0;

    fds.push_back(serverPoll);

    while (true)
    {
        int pollResult = poll(&fds[0], fds.size(), -1);

        if (pollResult == -1)
        {
            std::cerr << "poll failed: "
                    << strerror(errno) << std::endl;
            close(serverFd);
            return 1;
        }

        for (std::size_t i = 0; i < fds.size(); i++)
        {
            if (fds[i].revents & POLLIN)
            {
                if (fds[i].fd == serverFd)
                {

                    int clientFd = accept(serverFd, NULL, NULL);

                    if (clientFd == -1)
                    {
                        if (errno != EAGAIN && errno != EWOULDBLOCK)
                        {
                            std::cerr << "accept failed: "
                                    << strerror(errno) << std::endl;
                        }
                        continue;
                    }

                    int clientFlags = fcntl(clientFd, F_GETFL, 0);

                    if (clientFlags == -1 ||
                        fcntl(clientFd, F_SETFL,
                            clientFlags | O_NONBLOCK) == -1)
                    {
                        close(clientFd);
                        continue;
                    }

                    struct pollfd clientPoll;

                    clientPoll.fd = clientFd;
                    clientPoll.events = POLLIN;
                    clientPoll.revents = 0;

                    fds.push_back(clientPoll);

                    std::cout << "Client connected! fd = "
                            << clientFd << std::endl;
                }

                else
                {
                    char buffer[1024];

                    int bytesReceived = recv(
                        fds[i].fd,
                        buffer,
                        sizeof(buffer) - 1,
                        0
                    );

                    if (bytesReceived > 0)
                    {
                        buffer[bytesReceived] = '\0';

                        std::cout << "Client "
                                << fds[i].fd
                                << " sent: "
                                << buffer;
                    }
                    else if (bytesReceived == 0)
                    {
                        std::cout << "Client "
                                << fds[i].fd
                                << " disconnected"
                                << std::endl;

                        close(fds[i].fd);
                        fds.erase(fds.begin() + i);
                        i--;
                    }
                    else if (errno != EAGAIN && errno != EWOULDBLOCK)
                    {
                        std::cerr << "recv failed: "
                                << strerror(errno)
                                << std::endl;
                    }

                }
            }
        }
    }


    close(serverFd);

    return 0;
}
