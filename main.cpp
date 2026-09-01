/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mnaouss <mnaouss@student.42beirut.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/24 20:54:20 by mnaouss           #+#    #+#             */
/*   Updated: 2026/08/24 23:09:56 by mnaouss          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <sys/socket.h>
#include <iostream>
#include <unistd.h>
#include <netinet/in.h>
#include <cstring>

int main()
{
    int serverFd = socket(AF_INET, SOCK_STREAM, 0);

    if (serverFd == -1)
    {
        std::cerr << "socket() failed" << std::endl;
        return 1;
    }

    std::cout << "Socket created. fd = " << serverFd << std::endl;

    struct sockaddr_in address;

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
    int clientFd = accept(serverFd, NULL, NULL);

    if (clientFd == -1)
    {
        std::cerr << "accept failed" << std::endl;
        return 1;
    }

    std::cout << "Client connected! fd = "
          << clientFd << std::endl;

    char buffer[1024];

    while(true)
    {
        int bytesReceived = recv(clientFd, buffer, sizeof(buffer) - 1, 0);

        if (bytesReceived > 0)
        {
            buffer[bytesReceived] = '\0';

            std::cout << "Received: " << buffer;

            const char *message = "Server received your message!\n";

            send(clientFd, message, strlen(message), 0);
        }
        else if (bytesReceived == 0)
        {
            std::cout << "Client disconnected" << std::endl;
            break;
        }
        else
        {
            std::cerr << "recv failed" << std::endl;
            break;
        }
    }


    close(clientFd);
    close(serverFd);

    return 0;
}
