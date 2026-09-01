/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mnaouss <mnaouss@student.42beirut.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/01 22:24:16 by mnaouss           #+#    #+#             */
/*   Updated: 2026/09/01 22:52:41 by mnaouss          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Client.hpp"

Client::Client(int clientFd)
    : fd(clientFd), inputBuffer()
{
}

int Client::getFd() const
{
    return fd;
}

void Client::appendData(const char *data, std::size_t length)
{
    inputBuffer.append(data, length);
}

const std::string &Client::getInputBuffer() const
{
    return inputBuffer;
}

bool Client::hasCompleteMessage() const
{
    return inputBuffer.find("\r\n") != std::string::npos;
}

std::string Client::extractMessage()
{
    std::size_t end = inputBuffer.find("\r\n");

    if (end == std::string::npos)
        return "";

    std::string message = inputBuffer.substr(0, end);

    inputBuffer.erase(0, end + 2);

    return message;
}