/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mnaouss <mnaouss@student.42beirut.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/01 20:44:33 by mnaouss           #+#    #+#             */
/*   Updated: 2026/09/04 13:36:59 by mnaouss          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include <unistd.h>

static std::vector<std::string> splitCommaSeparated(
    const std::string &value
)
{
    std::vector<std::string> result;
    std::size_t start = 0;

    while (start <= value.size())
    {
        std::size_t comma = value.find(',', start);

        if (comma == std::string::npos)
        {
            result.push_back(value.substr(start));
            break;
        }

        result.push_back(value.substr(start, comma - start));
        start = comma + 1;
    }

    return result;
}

static bool isValidChannelName(const std::string &name)
{
    if (name.size() < 2 || (name[0] != '#' && name[0] != '&'))
        return false;

    for (std::size_t i = 1; i < name.size(); i++)
    {
        unsigned char character =
            static_cast<unsigned char>(name[i]);

        if (character <= 32 || character == 7 || name[i] == ',')
            return false;
    }

    return true;
}

bool Server::running = true;

void Server::handleSignal(int signum)
{
    (void)signum;
    Server::running = false;
}

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

    while (Server::running)
    {
        int pollResult = poll(&pollFds[0], pollFds.size(), -1);

        if (pollResult == -1)
        {
            if (!Server::running)
                break;
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

            if (!removed &&
                pollFds[i].fd != serverFd &&
                (readyEvents & (POLLERR | POLLHUP | POLLNVAL)))
            {
                disconnectClient(i, "Connection closed");
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
    if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
        return false;

    return true;
}


void Server::acceptClient()
{
    int clientFd = accept(serverFd, NULL, NULL);

    if (clientFd == -1)
        return;

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

void Server::disconnectClient(
    std::size_t index,
    const std::string &reason
)
{
    int clientFd = pollFds[index].fd;
    std::cout << "Client "
              << pollFds[index].fd
              << " disconnected"
              << std::endl;

    std::map<int, Client>::iterator clientIt = clients.find(clientFd);

    if (clientIt != clients.end())
    {
        removeClientFromChannels(clientIt->second, reason);
        clients.erase(clientIt);
    }

    close(clientFd);
    pollFds.erase(pollFds.begin() + index);
}

void Server::removeClientFromChannels(
    Client &client,
    const std::string &reason
)
{
    std::set<int> recipients;
    std::map<std::string, Channel>::iterator channelIt = channels.begin();

    while (channelIt != channels.end())
    {
        Channel &channel = channelIt->second;
        channel.removeInvitation(client.getFd());

        if (channel.hasMember(client.getFd()))
        {
            const std::set<int> &members = channel.getMembers();

            for (std::set<int>::const_iterator memberIt = members.begin();
                 memberIt != members.end(); ++memberIt)
            {
                if (*memberIt != client.getFd())
                    recipients.insert(*memberIt);
            }

            channel.removeMember(client.getFd());
        }

        if (channel.isEmpty())
        {
            std::map<std::string, Channel>::iterator emptyChannel = channelIt;
            ++channelIt;
            channels.erase(emptyChannel);
            continue;
        }

        if (!channel.hasOperators())
            channel.addOperator(*channel.getMembers().begin());

        ++channelIt;
    }

    if (client.getNickname().empty())
        return;

    std::string message = getClientPrefix(client) + " QUIT :" + reason;

    for (std::set<int>::const_iterator it = recipients.begin();
         it != recipients.end(); ++it)
    {
        std::map<int, Client>::iterator recipient = clients.find(*it);

        if (recipient != clients.end())
            queueReply(recipient->second, message);
    }
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

        disconnectClient(index, "Internal server error");
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

            if (message.size() > 510)
            {
                sendNumeric(
                    clientIt->second,
                    "417",
                    ":Input line was too long"
                );
                continue;
            }

            processMessage(clientFd, message);

            if (clientIt->second.isQuitRequested())
            {
                disconnectClient(index, clientIt->second.getQuitReason());
                return false;
            }
        }

        if (clientIt->second.getInputBuffer().size() > 510)
        {
            disconnectClient(index, "Input line too long");
            return false;
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
        disconnectClient(index, "Connection closed");
        return false;
    }

    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
        return true;

    std::cerr << "recv failed for client "
              << clientFd << ": "
              << strerror(errno)
              << std::endl;

    disconnectClient(index, "Connection error");
    return false;
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
    {
        handlePass(clientIt->second, parameters);
    }
    else if (command == "NICK")
    {
        handleNick(clientIt->second, parameters);
    }
    else if (command == "USER")
    {
        handleUser(clientIt->second, parameters);
    }
    else if (command == "JOIN")
    {
        handleJoin(clientIt->second, parameters);
    }
    else if (command == "PART")
    {
        handlePart(clientIt->second, parameters);
    }
    else if (command == "QUIT")
    {
        handleQuit(clientIt->second, parameters);
    }
    else if (command == "PING")
    {
        handlePing(clientIt->second, parameters);
    }
    else if (command == "PONG")
    {
        return;
    }
    else if (command == "CAP")
    {
        handleCap(clientIt->second, parameters);
    }
    else if (command == "PRIVMSG")
    {
        handlePrivmsg(clientIt->second, parameters);
    }
    else if (command == "KICK")
    {
        handleKick(clientIt->second, parameters);
    }
    else if (command == "INVITE")
    {
        handleInvite(clientIt->second, parameters);
    }
    else if (command == "TOPIC")
    {
        handleTopic(clientIt->second, parameters);
    }
    else if (command == "MODE")
    {
        handleMode(clientIt->second, parameters);
    }
    else
    {
        sendNumeric(
            clientIt->second,
            "421",
            command + " :Unknown command"
        );
    }
}

void Server::handleJoin(
    Client &client,
    const std::vector<std::string> &parameters
)
{
    if (!client.isRegistered())
    {
        sendNumeric(client, "451", ":You have not registered");
        return;
    }

    if (parameters.empty() || parameters[0].empty())
    {
        sendNumeric(client, "461", "JOIN :Not enough parameters");
        return;
    }

    std::vector<std::string> channelNames =
        splitCommaSeparated(parameters[0]);
    std::vector<std::string> keys;

    if (parameters.size() > 1)
        keys = splitCommaSeparated(parameters[1]);

    for (std::size_t i = 0; i < channelNames.size(); i++)
    {
        const std::string &requestedName = channelNames[i];

        if (!isValidChannelName(requestedName))
        {
            sendNumeric(
                client,
                "403",
                requestedName + " :No such channel"
            );
            continue;
        }

        std::string normalizedName = normalizeName(requestedName);
        std::map<std::string, Channel>::iterator channelIt =
            channels.find(normalizedName);

        if (channelIt == channels.end())
        {
            channels.insert(std::make_pair(
                normalizedName,
                Channel(requestedName)
            ));
            channelIt = channels.find(normalizedName);
        }

        Channel &channel = channelIt->second;

        if (channel.hasMember(client.getFd()))
            continue;

        if (channel.isInviteOnly() &&
            !channel.isInvited(client.getFd()))
        {
            sendNumeric(
                client,
                "473",
                channel.getName() + " :Cannot join channel (+i)"
            );
            continue;
        }

        std::string suppliedKey;
        if (i < keys.size())
            suppliedKey = keys[i];

        if (channel.hasKey() && channel.getKey() != suppliedKey)
        {
            sendNumeric(
                client,
                "475",
                channel.getName() + " :Cannot join channel (+k)"
            );
            continue;
        }

        if (channel.hasUserLimit() &&
            channel.getMemberCount() >= channel.getUserLimit())
        {
            sendNumeric(
                client,
                "471",
                channel.getName() + " :Cannot join channel (+l)"
            );
            continue;
        }

        bool firstMember = channel.isEmpty();
        channel.addMember(client.getFd());
        channel.removeInvitation(client.getFd());

        if (firstMember)
            channel.addOperator(client.getFd());

        broadcastToChannel(
            channel,
            getClientPrefix(client) + " JOIN :" + channel.getName()
        );

        if (channel.getTopic().empty())
        {
            sendNumeric(
                client,
                "331",
                channel.getName() + " :No topic is set"
            );
        }
        else
        {
            sendNumeric(
                client,
                "332",
                channel.getName() + " :" + channel.getTopic()
            );
        }

        sendNames(client, channel);
    }
}

void Server::handlePart(
    Client &client,
    const std::vector<std::string> &parameters
)
{
    if (!client.isRegistered())
    {
        sendNumeric(client, "451", ":You have not registered");
        return;
    }

    if (parameters.empty() || parameters[0].empty())
    {
        sendNumeric(client, "461", "PART :Not enough parameters");
        return;
    }

    std::vector<std::string> channelNames =
        splitCommaSeparated(parameters[0]);
    std::string reason = client.getNickname();

    if (parameters.size() > 1 && !parameters[1].empty())
        reason = parameters[1];

    for (std::size_t i = 0; i < channelNames.size(); i++)
    {
        std::string normalizedName = normalizeName(channelNames[i]);
        std::map<std::string, Channel>::iterator channelIt =
            channels.find(normalizedName);

        if (channelIt == channels.end())
        {
            sendNumeric(
                client,
                "403",
                channelNames[i] + " :No such channel"
            );
            continue;
        }

        Channel &channel = channelIt->second;

        if (!channel.hasMember(client.getFd()))
        {
            sendNumeric(
                client,
                "442",
                channel.getName() + " :You're not on that channel"
            );
            continue;
        }

        broadcastToChannel(
            channel,
            getClientPrefix(client) + " PART " +
                channel.getName() + " :" + reason
        );

        channel.removeMember(client.getFd());

        if (channel.isEmpty())
        {
            channels.erase(channelIt);
        }
        else if (!channel.hasOperators())
        {
            channel.addOperator(*channel.getMembers().begin());
        }
    }
}

void Server::handleQuit(
    Client &client,
    const std::vector<std::string> &parameters
)
{
    std::string reason = "Client Quit";

    if (!parameters.empty() && !parameters[0].empty())
        reason = parameters[0];

    client.requestQuit(reason);
}

void Server::handlePing(
    Client &client,
    const std::vector<std::string> &parameters
)
{
    if (parameters.empty() || parameters[0].empty())
    {
        sendNumeric(client, "409", ":No origin specified");
        return;
    }

    queueReply(
        client,
        ":ircserv PONG ircserv :" + parameters[0]
    );
}

void Server::handleCap(
    Client &client,
    const std::vector<std::string> &parameters
)
{
    if (parameters.empty())
    {
        sendNumeric(client, "461", "CAP :Not enough parameters");
        return;
    }

    std::string subcommand = parameters[0];

    for (std::size_t i = 0; i < subcommand.size(); i++)
    {
        subcommand[i] = static_cast<char>(std::toupper(
            static_cast<unsigned char>(subcommand[i])
        ));
    }

    std::string target = client.getNickname();
    if (target.empty())
        target = "*";

    if (subcommand == "LS")
    {
        queueReply(client, ":ircserv CAP " + target + " LS :");
    }
    else if (subcommand == "REQ")
    {
        std::string requestedCapabilities;
        if (parameters.size() > 1)
            requestedCapabilities = parameters[1];

        queueReply(
            client,
            ":ircserv CAP " + target + " NAK :" +
                requestedCapabilities
        );
    }
    else if (subcommand == "END")
    {
        return;
    }
}

void Server::handlePrivmsg(
    Client &client,
    const std::vector<std::string> &parameters
)
{
    if (!client.isRegistered())
    {
        sendNumeric(client, "451", ":You have not registered");
        return;
    }

    if (parameters.empty() || parameters[0].empty())
    {
        sendNumeric(client, "411", ":No recipient given (PRIVMSG)");
        return;
    }

    if (parameters.size() < 2 || parameters[1].empty())
    {
        sendNumeric(client, "412", ":No text to send");
        return;
    }

    std::vector<std::string> targets = splitCommaSeparated(parameters[0]);
    const std::string &messageText = parameters[1];

    for (std::size_t i = 0; i < targets.size(); i++)
    {
        const std::string &target = targets[i];
        if (target.empty())
            continue;

        if (target[0] == '#' || target[0] == '&')
        {
            std::string normalizedChan = normalizeName(target);
            std::map<std::string, Channel>::iterator chanIt = channels.find(normalizedChan);

            if (chanIt == channels.end())
            {
                sendNumeric(client, "403", target + " :No such channel");
                continue;
            }

            Channel &channel = chanIt->second;

            if (!channel.hasMember(client.getFd()))
            {
                sendNumeric(client, "404", channel.getName() + " :Cannot send to channel");
                continue;
            }

            std::string fullMsg = getClientPrefix(client) + " PRIVMSG " + channel.getName() + " :" + messageText;
            const std::set<int> &members = channel.getMembers();

            for (std::set<int>::const_iterator mit = members.begin(); mit != members.end(); ++mit)
            {
                if (*mit != client.getFd())
                {
                    std::map<int, Client>::iterator cit = clients.find(*mit);
                    if (cit != clients.end())
                        queueReply(cit->second, fullMsg);
                }
            }
        }
        else
        {
            std::string normalizedTarget = normalizeName(target);
            Client *recipient = NULL;

            for (std::map<int, Client>::iterator cit = clients.begin(); cit != clients.end(); ++cit)
            {
                if (cit->second.isRegistered() && normalizeName(cit->second.getNickname()) == normalizedTarget)
                {
                    recipient = &(cit->second);
                    break;
                }
            }

            if (!recipient)
            {
                sendNumeric(client, "401", target + " :No such nick/channel");
                continue;
            }

            std::string fullMsg = getClientPrefix(client) + " PRIVMSG " + recipient->getNickname() + " :" + messageText;
            queueReply(*recipient, fullMsg);
        }
    }
}

void Server::handleKick(
    Client &client,
    const std::vector<std::string> &parameters
)
{
    if (!client.isRegistered())
    {
        sendNumeric(client, "451", ":You have not registered");
        return;
    }

    if (parameters.size() < 2 || parameters[0].empty() || parameters[1].empty())
    {
        sendNumeric(client, "461", "KICK :Not enough parameters");
        return;
    }

    const std::string &channelName = parameters[0];
    const std::string &targetNick = parameters[1];
    std::string reason = client.getNickname();
    if (parameters.size() > 2 && !parameters[2].empty())
        reason = parameters[2];

    std::string normChan = normalizeName(channelName);
    std::map<std::string, Channel>::iterator chanIt = channels.find(normChan);

    if (chanIt == channels.end())
    {
        sendNumeric(client, "403", channelName + " :No such channel");
        return;
    }

    Channel &channel = chanIt->second;

    if (!channel.hasMember(client.getFd()))
    {
        sendNumeric(client, "442", channel.getName() + " :You're not on that channel");
        return;
    }

    if (!channel.isOperator(client.getFd()))
    {
        sendNumeric(client, "482", channel.getName() + " :You're not channel operator");
        return;
    }

    std::string normTarget = normalizeName(targetNick);
    Client *targetClient = NULL;

    for (std::map<int, Client>::iterator cit = clients.begin(); cit != clients.end(); ++cit)
    {
        if (cit->second.isRegistered() && normalizeName(cit->second.getNickname()) == normTarget)
        {
            targetClient = &(cit->second);
            break;
        }
    }

    if (!targetClient || !channel.hasMember(targetClient->getFd()))
    {
        sendNumeric(client, "441", targetNick + " " + channel.getName() + " :They aren't on that channel");
        return;
    }

    std::string kickMsg = getClientPrefix(client) + " KICK " + channel.getName() + " " + targetClient->getNickname() + " :" + reason;
    broadcastToChannel(channel, kickMsg);

    channel.removeMember(targetClient->getFd());

    if (channel.isEmpty())
    {
        channels.erase(chanIt);
    }
    else if (!channel.hasOperators())
    {
        channel.addOperator(*channel.getMembers().begin());
    }
}

void Server::handleInvite(
    Client &client,
    const std::vector<std::string> &parameters
)
{
    if (!client.isRegistered())
    {
        sendNumeric(client, "451", ":You have not registered");
        return;
    }

    if (parameters.size() < 2 || parameters[0].empty() || parameters[1].empty())
    {
        sendNumeric(client, "461", "INVITE :Not enough parameters");
        return;
    }

    const std::string &targetNick = parameters[0];
    const std::string &channelName = parameters[1];

    std::string normChan = normalizeName(channelName);
    std::map<std::string, Channel>::iterator chanIt = channels.find(normChan);

    if (chanIt == channels.end())
    {
        sendNumeric(client, "403", channelName + " :No such channel");
        return;
    }

    Channel &channel = chanIt->second;

    if (!channel.hasMember(client.getFd()))
    {
        sendNumeric(client, "442", channel.getName() + " :You're not on that channel");
        return;
    }

    if (!channel.isOperator(client.getFd()))
    {
        sendNumeric(client, "482", channel.getName() + " :You're not channel operator");
        return;
    }

    std::string normTarget = normalizeName(targetNick);
    Client *targetClient = NULL;

    for (std::map<int, Client>::iterator cit = clients.begin(); cit != clients.end(); ++cit)
    {
        if (cit->second.isRegistered() && normalizeName(cit->second.getNickname()) == normTarget)
        {
            targetClient = &(cit->second);
            break;
        }
    }

    if (!targetClient)
    {
        sendNumeric(client, "401", targetNick + " :No such nick/channel");
        return;
    }

    if (channel.hasMember(targetClient->getFd()))
    {
        sendNumeric(client, "443", targetClient->getNickname() + " " + channel.getName() + " :is already on channel");
        return;
    }

    channel.invite(targetClient->getFd());

    sendNumeric(client, "341", targetClient->getNickname() + " " + channel.getName());

    std::string inviteNotice = getClientPrefix(client) + " INVITE " + targetClient->getNickname() + " :" + channel.getName();
    queueReply(*targetClient, inviteNotice);
}

void Server::handleTopic(
    Client &client,
    const std::vector<std::string> &parameters
)
{
    if (!client.isRegistered())
    {
        sendNumeric(client, "451", ":You have not registered");
        return;
    }

    if (parameters.empty() || parameters[0].empty())
    {
        sendNumeric(client, "461", "TOPIC :Not enough parameters");
        return;
    }

    const std::string &channelName = parameters[0];
    std::string normChan = normalizeName(channelName);
    std::map<std::string, Channel>::iterator chanIt = channels.find(normChan);

    if (chanIt == channels.end())
    {
        sendNumeric(client, "403", channelName + " :No such channel");
        return;
    }

    Channel &channel = chanIt->second;

    if (!channel.hasMember(client.getFd()))
    {
        sendNumeric(client, "442", channel.getName() + " :You're not on that channel");
        return;
    }

    if (parameters.size() == 1)
    {
        if (channel.getTopic().empty())
            sendNumeric(client, "331", channel.getName() + " :No topic is set");
        else
            sendNumeric(client, "332", channel.getName() + " :" + channel.getTopic());
        return;
    }

    if (channel.isTopicRestricted() && !channel.isOperator(client.getFd()))
    {
        sendNumeric(client, "482", channel.getName() + " :You're not channel operator");
        return;
    }

    channel.setTopic(parameters[1]);
    std::string topicMsg = getClientPrefix(client) + " TOPIC " + channel.getName() + " :" + parameters[1];
    broadcastToChannel(channel, topicMsg);
}

void Server::handleMode(
    Client &client,
    const std::vector<std::string> &parameters
)
{
    if (!client.isRegistered())
    {
        sendNumeric(client, "451", ":You have not registered");
        return;
    }

    if (parameters.empty() || parameters[0].empty())
    {
        sendNumeric(client, "461", "MODE :Not enough parameters");
        return;
    }

    const std::string &target = parameters[0];

    if (target[0] != '#' && target[0] != '&')
    {
        if (normalizeName(target) == normalizeName(client.getNickname()))
            sendNumeric(client, "221", "+");
        else
            sendNumeric(client, "502", ":Cant change mode for other users");
        return;
    }

    std::string normChan = normalizeName(target);
    std::map<std::string, Channel>::iterator chanIt = channels.find(normChan);

    if (chanIt == channels.end())
    {
        sendNumeric(client, "403", target + " :No such channel");
        return;
    }

    Channel &channel = chanIt->second;

    if (parameters.size() == 1)
    {
        std::string modeStr = "+";
        std::string modeParams = "";

        if (channel.isInviteOnly())
            modeStr += "i";
        if (channel.isTopicRestricted())
            modeStr += "t";
        if (channel.hasKey())
        {
            modeStr += "k";
            modeParams += " " + channel.getKey();
        }
        if (channel.hasUserLimit())
        {
            modeStr += "l";
            std::ostringstream oss;
            oss << channel.getUserLimit();
            modeParams += " " + oss.str();
        }

        sendNumeric(client, "324", channel.getName() + " " + modeStr + modeParams);
        return;
    }

    if (!channel.hasMember(client.getFd()))
    {
        sendNumeric(client, "442", channel.getName() + " :You're not on that channel");
        return;
    }

    if (!channel.isOperator(client.getFd()))
    {
        sendNumeric(client, "482", channel.getName() + " :You're not channel operator");
        return;
    }

    const std::string &modes = parameters[1];
    std::size_t paramIdx = 2;
    char currentSign = '+';

    std::string appliedModes = "";
    std::string appliedParams = "";
    char lastAppliedSign = '\0';

    for (std::size_t i = 0; i < modes.size(); i++)
    {
        char c = modes[i];

        if (c == '+' || c == '-')
        {
            currentSign = c;
            continue;
        }

        if (c == 'i')
        {
            channel.setInviteOnly(currentSign == '+');
            if (lastAppliedSign != currentSign)
            {
                appliedModes += currentSign;
                lastAppliedSign = currentSign;
            }
            appliedModes += 'i';
        }
        else if (c == 't')
        {
            channel.setTopicRestricted(currentSign == '+');
            if (lastAppliedSign != currentSign)
            {
                appliedModes += currentSign;
                lastAppliedSign = currentSign;
            }
            appliedModes += 't';
        }
        else if (c == 'k')
        {
            if (currentSign == '+')
            {
                if (paramIdx < parameters.size() && !parameters[paramIdx].empty())
                {
                    const std::string &key = parameters[paramIdx++];
                    channel.setKey(key);
                    if (lastAppliedSign != currentSign)
                    {
                        appliedModes += currentSign;
                        lastAppliedSign = currentSign;
                    }
                    appliedModes += 'k';
                    appliedParams += " " + key;
                }
                else
                {
                    sendNumeric(client, "461", "MODE :Not enough parameters");
                }
            }
            else
            {
                channel.setKey("");
                if (lastAppliedSign != currentSign)
                {
                    appliedModes += currentSign;
                    lastAppliedSign = currentSign;
                }
                appliedModes += 'k';
            }
        }
        else if (c == 'o')
        {
            if (paramIdx < parameters.size() && !parameters[paramIdx].empty())
            {
                const std::string &targetNick = parameters[paramIdx++];
                std::string normTarget = normalizeName(targetNick);
                Client *targetClient = NULL;

                for (std::map<int, Client>::iterator cit = clients.begin(); cit != clients.end(); ++cit)
                {
                    if (cit->second.isRegistered() && normalizeName(cit->second.getNickname()) == normTarget)
                    {
                        targetClient = &(cit->second);
                        break;
                    }
                }

                if (!targetClient || !channel.hasMember(targetClient->getFd()))
                {
                    sendNumeric(client, "441", targetNick + " " + channel.getName() + " :They aren't on that channel");
                }
                else
                {
                    if (currentSign == '+')
                        channel.addOperator(targetClient->getFd());
                    else
                        channel.removeOperator(targetClient->getFd());

                    if (lastAppliedSign != currentSign)
                    {
                        appliedModes += currentSign;
                        lastAppliedSign = currentSign;
                    }
                    appliedModes += 'o';
                    appliedParams += " " + targetClient->getNickname();
                }
            }
            else
            {
                sendNumeric(client, "461", "MODE :Not enough parameters");
            }
        }
        else if (c == 'l')
        {
            if (currentSign == '+')
            {
                if (paramIdx < parameters.size() && !parameters[paramIdx].empty())
                {
                    const std::string &limitStr = parameters[paramIdx++];
                    char *end = NULL;
                    errno = 0;
                    long limit = std::strtol(limitStr.c_str(), &end, 10);

                    if (errno == 0 &&
                        end != limitStr.c_str() &&
                        *end == '\0' &&
                        limit > 0)
                    {
                        channel.setUserLimit(static_cast<std::size_t>(limit));
                        if (lastAppliedSign != currentSign)
                        {
                            appliedModes += currentSign;
                            lastAppliedSign = currentSign;
                        }
                        appliedModes += 'l';
                        std::ostringstream oss;
                        oss << limit;
                        appliedParams += " " + oss.str();
                    }
                    else
                    {
                        sendNumeric(
                            client,
                            "696",
                            channel.getName() + " l " + limitStr +
                                " :Invalid mode parameter"
                        );
                    }
                }
                else
                {
                    sendNumeric(client, "461", "MODE :Not enough parameters");
                }
            }
            else
            {
                channel.setUserLimit(0);
                if (lastAppliedSign != currentSign)
                {
                    appliedModes += currentSign;
                    lastAppliedSign = currentSign;
                }
                appliedModes += 'l';
            }
        }
        else
        {
            std::string unknown(1, c);
            sendNumeric(client, "472", unknown + " :is unknown mode char to me for " + channel.getName());
        }
    }

    if (!appliedModes.empty())
    {
        std::string modeNotice = getClientPrefix(client) + " MODE " + channel.getName() + " " + appliedModes + appliedParams;
        broadcastToChannel(channel, modeNotice);
    }
}

std::string Server::getClientPrefix(const Client &client) const
{
    return ":" + client.getNickname() +
        "!" + client.getUsername() + "@localhost";
}

void Server::broadcastToChannel(
    Channel &channel,
    const std::string &message
)
{
    const std::set<int> &members = channel.getMembers();

    for (std::set<int>::const_iterator it = members.begin();
         it != members.end(); ++it)
    {
        std::map<int, Client>::iterator clientIt = clients.find(*it);

        if (clientIt != clients.end())
            queueReply(clientIt->second, message);
    }
}

void Server::sendNames(Client &client, const Channel &channel)
{
    std::string names;
    const std::set<int> &members = channel.getMembers();

    for (std::set<int>::const_iterator it = members.begin();
         it != members.end(); ++it)
    {
        std::map<int, Client>::const_iterator clientIt = clients.find(*it);

        if (clientIt == clients.end())
            continue;

        if (!names.empty())
            names += " ";
        if (channel.isOperator(*it))
            names += "@";
        names += clientIt->second.getNickname();
    }

    sendNumeric(
        client,
        "353",
        "= " + channel.getName() + " :" + names
    );
    sendNumeric(
        client,
        "366",
        channel.getName() + " :End of /NAMES list"
    );
}


void Server::handlePass(
    Client &client,
    const std::vector<std::string> &parameters
)
{
    if (parameters.empty())
    {
        sendNumeric(
            client,
            "461",
            "PASS :Not enough parameters"
        );
        return;
    }

    if (client.isRegistered() ||
        client.isPasswordAccepted())
    {
        sendNumeric(
            client,
            "462",
            ":You may not reregister"
        );
        return;
    }

    if (parameters[0] != password)
    {
        sendNumeric(
            client,
            "464",
            ":Password incorrect"
        );
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
    std::string normalizedNickname =
        normalizeName(nickname);

    std::map<int, Client>::const_iterator it;

    for (it = clients.begin();
         it != clients.end();
         it++)
    {
        if (it->first == currentFd)
            continue;

        if (normalizeName(
                it->second.getNickname()
            ) == normalizedNickname)
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
    if (parameters.empty() ||
        parameters[0].empty())
    {
        sendNumeric(
            client,
            "431",
            ":No nickname given"
        );
        return;
    }

    const std::string &newNickname =
        parameters[0];

    if (!isValidNickname(newNickname))
    {
        sendNumeric(
            client,
            "432",
            newNickname +
            " :Erroneous nickname"
        );
        return;
    }

    if (isNicknameInUse(
            newNickname,
            client.getFd()
        ))
    {
        sendNumeric(
            client,
            "433",
            newNickname +
            " :Nickname is already in use"
        );
        return;
    }

    std::string oldNickname =
        client.getNickname();

    client.setNickname(newNickname);

    if (client.isRegistered() &&
        !oldNickname.empty() &&
        oldNickname != newNickname)
    {
        std::string nickMsg = ":" + oldNickname + "!" + client.getUsername() + "@localhost NICK :" + newNickname;
        queueReply(client, nickMsg);

        std::set<int> notifiedFds;
        for (std::map<std::string, Channel>::iterator it = channels.begin(); it != channels.end(); ++it)
        {
            if (it->second.hasMember(client.getFd()))
            {
                const std::set<int> &members = it->second.getMembers();
                for (std::set<int>::const_iterator mit = members.begin(); mit != members.end(); ++mit)
                {
                    if (*mit != client.getFd() && notifiedFds.find(*mit) == notifiedFds.end())
                    {
                        std::map<int, Client>::iterator cit = clients.find(*mit);
                        if (cit != clients.end())
                        {
                            queueReply(cit->second, nickMsg);
                            notifiedFds.insert(*mit);
                        }
                    }
                }
            }
        }
    }

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
    if (client.isRegistered())
    {
        sendNumeric(
            client,
            "462",
            ":You may not reregister"
        );
        return;
    }

    if (parameters.size() < 4)
    {
        sendNumeric(
            client,
            "461",
            "USER :Not enough parameters"
        );
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

    sendNumeric(
        client,
        "001",
        ":Welcome to the Internet Relay Network " +
        client.getNickname()
    );
    sendNumeric(
        client,
        "002",
        ":Your host is ircserv, running version 1.0"
    );
    sendNumeric(
        client,
        "003",
        ":This server was created today"
    );
    sendNumeric(
        client,
        "004",
        "ircserv 1.0  itkol"
    );
}

void Server::sendNumeric(
    Client &client,
    const std::string &code,
    const std::string &parameters
)
{
    std::string target = client.getNickname();

    if (target.empty())
        target = "*";

    std::string message =
        ":ircserv " + code + " " + target;

    if (!parameters.empty())
        message += " " + parameters;

    queueReply(client, message);
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
        disconnectClient(index, "Internal server error");
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

    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
        return true;

    std::cerr << "send failed for client "
              << clientFd << ": "
              << strerror(errno)
              << std::endl;

    disconnectClient(index, "Connection error");
    return false;
}


std::string Server::normalizeName(
    const std::string &name
) const
{
    std::string normalized = name;

    for (std::size_t i = 0; i < normalized.size(); i++)
    {
        unsigned char character =
            static_cast<unsigned char>(normalized[i]);

        normalized[i] = static_cast<char>(
            std::tolower(character)
        );
    }

    return normalized;
}

bool Server::isValidNickname(
    const std::string &nickname
) const
{
    if (nickname.empty())
        return false;

    unsigned char first =
        static_cast<unsigned char>(nickname[0]);

    if (!std::isalpha(first) &&
        nickname[0] != '_' &&
        nickname[0] != '[' &&
        nickname[0] != ']' &&
        nickname[0] != '\\' &&
        nickname[0] != '`' &&
        nickname[0] != '^' &&
        nickname[0] != '{' &&
        nickname[0] != '|' &&
        nickname[0] != '}')
    {
        return false;
    }

    for (std::size_t i = 1;
         i < nickname.size();
         i++)
    {
        unsigned char character =
            static_cast<unsigned char>(nickname[i]);

        if (!std::isalnum(character) &&
            nickname[i] != '-' &&
            nickname[i] != '_' &&
            nickname[i] != '[' &&
            nickname[i] != ']' &&
            nickname[i] != '\\' &&
            nickname[i] != '`' &&
            nickname[i] != '^' &&
            nickname[i] != '{' &&
            nickname[i] != '|' &&
            nickname[i] != '}')
        {
            return false;
        }
    }

    return true;
}
