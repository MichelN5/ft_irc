/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mnaouss <mnaouss@student.42beirut.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/01 22:24:16 by mnaouss           #+#    #+#             */
/*   Updated: 2026/09/02 17:54:51 by mnaouss          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Client.hpp"

Client::Client(int clientFd)
    : fd(clientFd),
      inputBuffer(),
      outputBuffer(),
      passwordAccepted(false),
      nickname(),
      username(),
      realname(),
      registered(false)
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

bool Client::isPasswordAccepted() const
{
    return passwordAccepted;
}

void Client::setPasswordAccepted(bool accepted)
{
    passwordAccepted = accepted;
}


const std::string &Client::getNickname() const
{
    return nickname;
}

void Client::setNickname(const std::string &newNickname)
{
    nickname = newNickname;
}

const std::string &Client::getUsername() const
{
    return username;
}

const std::string &Client::getRealname() const
{
    return realname;
}

void Client::setUserInfo(
    const std::string &newUsername,
    const std::string &newRealname
)
{
    username = newUsername;
    realname = newRealname;
}

bool Client::isRegistered() const
{
    return registered;
}

void Client::setRegistered(bool value)
{
    registered = value;
}


void Client::queueMessage(const std::string &message)
{
    outputBuffer += message;
}

bool Client::hasPendingOutput() const
{
    return !outputBuffer.empty();
}

const std::string &Client::getOutputBuffer() const
{
    return outputBuffer;
}

void Client::removeSentData(std::size_t length)
{
    outputBuffer.erase(0, length);
}
