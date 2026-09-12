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
#include <cstdlib>

/**
 * @brief Core IRC Server engine.
 * 
 * Orchestrates non-blocking I/O multiplexing via a single poll() call site,
 * tracks active client connections and channels, and dispatches protocol
 * command handlers conforming to RFC 1459/2812 and 42 School specifications.
 */
class Server
{
private:
    int                            serverFd;
    std::map<int, Client>          clients;
    std::map<std::string, Channel> channels;
    int                            port;
    std::string                    password;
    std::vector<struct pollfd>     pollFds;

    /**
     * @brief Puts a socket file descriptor into non-blocking mode.
     * @param fd The socket descriptor to configure.
     * @return true on success, false if fcntl fails.
     * @pre fd must be a valid open file descriptor.
     * @post O_NONBLOCK flag is set on fd via bare fcntl(fd, F_SETFL, O_NONBLOCK).
     */
    bool setNonBlocking(int fd);

    /**
     * @brief Accepts an incoming connection on the listening server socket.
     * @pre poll() has reported POLLIN on pollFds[0] (serverFd).
     * @post New non-blocking client socket created, registered in pollFds and clients map.
     * @note Edge case: If accept() or setNonBlocking() fails, socket is closed with no state leak.
     */
    void acceptClient();

    /**
     * @brief Reads available incoming data from a client socket.
     * @param index Index into pollFds array corresponding to the client.
     * @return true if connection remains active, false if EOF/error requires disconnection.
     * @pre poll() has reported POLLIN on pollFds[index].
     * @post Inbound bytes appended to Client inputBuffer and complete commands processed.
     */
    bool readClient(std::size_t index);

    /**
     * @brief Disconnects a client, closes their socket, and removes them from all channels.
     * @param index Index of client in pollFds vector.
     * @param reason Disconnect reason broadcast to peers.
     * @pre index < pollFds.size() and index > 0.
     * @post Client removed from channels, pollFds, and clients map; socket closed.
     */
    void disconnectClient(
        std::size_t index,
        const std::string &reason
    );

    /**
     * @brief Removes a client from all joined channels and broadcasts QUIT/PART notices.
     * @param client Reference to departing Client.
     * @param reason Part/quit explanation message.
     * @post Client removed from all channels; empty channels pruned; vacant op status reassigned.
     */
    void removeClientFromChannels(
        Client &client,
        const std::string &reason
    );

    /**
     * @brief Creates, binds, and configures the main listening socket.
     * @return true on successful setup, false on socket/bind/listen failure.
     * @post serverFd is bound to 0.0.0.0:port in non-blocking listen mode and added to pollFds[0].
     */
    bool setupSocket();

    /**
     * @brief Parses and dispatches a single IRC command message.
     * @param clientFd Socket file descriptor of the sender.
     * @param message Raw line extracted from client's input buffer.
     * @pre message must be non-empty and stripped of framing delimiters.
     * @post Appropriate command handler invoked or numeric error returned.
     */
    void processMessage(int clientFd, const std::string &message);

    /**
     * @brief Broadcasts a message to all members of a channel.
     * @param channel Reference to destination Channel.
     * @param message Fully framed IRC message to send.
     * @post Message queued into outputBuffer of every channel member.
     */
    void broadcastToChannel(
        Channel &channel,
        const std::string &message
    );

    /**
     * @brief Sends RPL_NAMREPLY (353) and RPL_ENDOFNAMES (366) to a client.
     * @param client Recipient client.
     * @param channel Target channel whose member list is being sent.
     * @post 353 and 366 numerics queued to client.
     */
    void sendNames(Client &client, const Channel &channel);

    /**
     * @brief Constructs the standard IRC prefix for a client (:nick!user@localhost).
     * @param client The source client.
     * @return Formatted prefix string.
     */
    std::string getClientPrefix(const Client &client) const;

public:
    /** @brief Global running flag controlled by signal handlers. */
    static bool running;

    /**
     * @brief Signal handler for SIGINT and SIGTERM.
     * @param signum Signal number received.
     * @post Sets running to false to initiate clean server shutdown.
     */
    static void handleSignal(int signum);

    /**
     * @brief Constructs the Server with port and connection password.
     * @param serverPort Valid port number (1-65535).
     * @param serverPassword Required connection authentication password.
     * @post Server initialized; listening socket not yet bound until run() is called.
     */
    Server(int serverPort, const std::string &serverPassword);

    /**
     * @brief Destructor: releases all resources and closes active sockets.
     * @post All client sockets and listening socket closed; memory freed.
     */
    ~Server();

    /**
     * @brief Main server loop; multiplexes events via poll() until termination.
     * @return Exit status (0 on clean shutdown, 1 on initialization failure).
     * @post Runs single poll() multiplexing loop; cleans up all state on exit.
     */
    int run();
    
    /**
     * @brief Handles the PASS command for connection authentication.
     * @param client Client issuing the command.
     * @param parameters Command argument list.
     * @post Validates password; sets passwordAccepted if correct; rejects with 464 if wrong.
     */
    void handlePass(
        Client &client,
        const std::vector<std::string> &parameters
    );

    /**
     * @brief Handles the NICK command for setting or changing nicknames.
     * @param client Client issuing the command.
     * @param parameters Command argument list containing new nickname.
     * @post Validates format (432) and uniqueness (433); updates nickname and broadcasts changes.
     */
    void handleNick(
        Client &client,
        const std::vector<std::string> &parameters
    );

    /**
     * @brief Checks if a nickname is already taken by another registered client.
     * @param nickname The nickname to search for (case-insensitive).
     * @param currentFd File descriptor of caller to exclude from collision check.
     * @return true if taken by another client, false if available.
     */
    bool isNicknameInUse(
        const std::string &nickname,
        int currentFd
    ) const;

    /**
     * @brief Validates nickname syntax according to RFC 1459/2812.
     * @param nickname Candidate nickname string.
     * @return true if syntax is valid, false otherwise.
     */
    bool isValidNickname(
        const std::string &nickname
    ) const;

    /**
     * @brief Normalizes an IRC entity name (lowercasing with IRC casing rules).
     * @param name Input channel or user name.
     * @return Lowercase normalized string.
     */
    std::string normalizeName(
        const std::string &name
    ) const;

    /**
     * @brief Handles the USER command to supply username and realname.
     * @param client Client issuing the command.
     * @param parameters Command argument list.
     * @post Sets username and realname; attempts registration completion.
     */
    void handleUser(
        Client &client,
        const std::vector<std::string> &parameters
    );

    /**
     * @brief Handles the JOIN command for entering channels.
     * @param client Client issuing the command.
     * @param parameters Comma-separated channel names and optional keys.
     * @post Checks +i, +k, +l; creates channel or adds member; auto-ops first member.
     */
    void handleJoin(
        Client &client,
        const std::vector<std::string> &parameters
    );

    /**
     * @brief Handles the PART command for leaving channels.
     * @param client Client issuing the command.
     * @param parameters Channel names and optional parting message.
     * @post Removes client from channel and broadcasts PART notice.
     */
    void handlePart(
        Client &client,
        const std::vector<std::string> &parameters
    );

    /**
     * @brief Handles the QUIT command for graceful client departure.
     * @param client Client issuing the command.
     * @param parameters Optional quit message.
     * @post Flags client for quit; broadcasts QUIT to all shared channels.
     */
    void handleQuit(
        Client &client,
        const std::vector<std::string> &parameters
    );

    /**
     * @brief Handles the PING command to maintain connection liveness.
     * @param client Client issuing the command.
     * @param parameters Ping token parameter.
     * @post Queues PONG response with matching token.
     */
    void handlePing(
        Client &client,
        const std::vector<std::string> &parameters
    );

    /**
     * @brief Handles the CAP command for client capability negotiation.
     * @param client Client issuing the command.
     * @param parameters CAP subcommands (e.g. LS, END).
     * @post Responds to CAP LS (empty list) or ignores unsupported caps gracefully.
     */
    void handleCap(
        Client &client,
        const std::vector<std::string> &parameters
    );

    /**
     * @brief Handles the PRIVMSG command for direct and channel messaging.
     * @param client Client issuing the command.
     * @param parameters Targets (comma-separated nicks/channels) and message text.
     * @post Relays message to target users or channel members (excluding sender).
     */
    void handlePrivmsg(
        Client &client,
        const std::vector<std::string> &parameters
    );

    /**
     * @brief Handles the KICK command to forcefully remove a member from a channel.
     * @param client Client issuing command (must be channel operator).
     * @param parameters Channel name, target nickname, and optional comment.
     * @post Target removed from channel and kick broadcast sent to all members.
     */
    void handleKick(
        Client &client,
        const std::vector<std::string> &parameters
    );

    /**
     * @brief Handles the INVITE command to invite a user to a channel.
     * @param client Client issuing command (must be op if channel is +i).
     * @param parameters Target nickname and channel name.
     * @post Invitation recorded on channel; 341 sent to caller; invite notice sent to target.
     */
    void handleInvite(
        Client &client,
        const std::vector<std::string> &parameters
    );

    /**
     * @brief Handles the TOPIC command to query or change channel topics.
     * @param client Client issuing command (must be op if channel is +t).
     * @param parameters Channel name and optional new topic string.
     * @post Queries topic (331/332) or updates topic and broadcasts change.
     */
    void handleTopic(
        Client &client,
        const std::vector<std::string> &parameters
    );

    /**
     * @brief Handles the MODE command for querying or modifying channel modes.
     * @param client Client issuing command.
     * @param parameters Channel name, mode string (+-itkol), and optional parameters.
     * @post Returns 324 on query; applies mode modifications if caller is operator.
     */
    void handleMode(
        Client &client,
        const std::vector<std::string> &parameters
    );

    /**
     * @brief Evaluates registration prerequisites and sends welcome burst on completion.
     * @param client Client to evaluate.
     * @post If passwordAccepted, nickname, and username are satisfied, sends 001-004.
     */
    void tryRegister(Client &client);

    /**
     * @brief Enqueues a formatted IRC line into a client's outbound buffer.
     * @param client Target client.
     * @param message Message text (automatically appends \r\n if missing).
     * @post Message appended to client's outputBuffer; POLLOUT enabled for client.
     */
    void queueReply(
        Client &client,
        const std::string &message
    );

    /**
     * @brief Formats and enqueues a standard numeric reply (:server <code> <nick> <params>).
     * @param client Target recipient client.
     * @param code Three-digit numeric reply code string.
     * @param parameters Numeric reply arguments.
     * @post Formatted numeric reply queued to client.
     */
    void sendNumeric(
        Client &client,
        const std::string &code,
        const std::string &parameters
    );

    /**
     * @brief Sends buffered outbound data to a client socket.
     * @param index Index into pollFds array corresponding to the client.
     * @return true if client remains healthy, false if send() failed or client disconnected.
     * @pre poll() has reported POLLOUT on pollFds[index].
     * @post Transmitted bytes removed from client's outputBuffer; disables POLLOUT if buffer empty.
     */
    bool writeClient(std::size_t index);
};

#endif
