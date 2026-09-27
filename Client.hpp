/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mnaouss <mnaouss@student.42beirut.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/01 22:15:36 by mnaouss           #+#    #+#             */
/*   Updated: 2026/09/02 17:57:05 by mnaouss          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <cstddef>
#include <vector>

/**
 * @brief Represents a connected IRC client session.
 * 
 * Manages socket file descriptor, inbound/outbound stream buffers,
 * authentication/registration flags, nickname, username, realname,
 * and disconnect state.
 */
class Client
{
private:
    int         fd;
    std::string inputBuffer;
    std::string outputBuffer;
    bool        passwordAccepted;
    std::string nickname;
    std::string username;
    std::string realname;
    bool        registered;
    bool        quitRequested;
    std::string quitReason;

public:
    /**
     * @brief Constructs a new Client associated with a socket file descriptor.
     * @param clientFd Socket file descriptor returned by accept().
     * @pre clientFd must be a valid, open, non-blocking socket descriptor.
     * @post Client is initialized with empty buffers and unregistered state.
     */
    explicit Client(int clientFd);

    /**
     * @brief Retrieves the client's socket file descriptor.
     * @return The socket file descriptor integer.
     */
    int getFd() const;

    /**
     * @brief Appends raw incoming network bytes to the client's input buffer.
     * @param data Pointer to received byte buffer.
     * @param length Number of bytes to append.
     * @pre data must be non-NULL if length > 0.
     * @post inputBuffer size increases by length bytes.
     * @note Edge case: Appending 0 bytes is a safe no-op.
     */
    void appendData(const char *data, std::size_t length);

    /**
     * @brief Gets read-only reference to the raw input buffer.
     * @return Reference to the inbound message buffer string.
     */
    const std::string &getInputBuffer() const;

    /**
     * @brief Checks if the input buffer contains at least one framed command.
     * @return true if a newline delimiter ('\n') is found, false otherwise.
     */
    bool hasCompleteMessage() const;

    /**
     * @brief Extracts the oldest framed command from the input buffer.
     * @return The message string stripped of trailing '\r' and '\n', or empty string if incomplete.
     * @post Extracted command and its delimiter are removed from inputBuffer.
     * @note Handles both CRLF ("\r\n") and bare LF ("\n") delimiters.
     */
    std::string extractMessage();

    /**
     * @brief Checks whether the client has supplied the correct server password.
     * @return true if PASS succeeded, false otherwise.
     */
    bool isPasswordAccepted() const;

    /**
     * @brief Updates password acceptance status.
     * @param accepted true if password matched server password.
     * @post passwordAccepted state is updated.
     */
    void setPasswordAccepted(bool accepted);

    /**
     * @brief Retrieves the client's current nickname.
     * @return Nickname string, or empty string if not yet set.
     */
    const std::string &getNickname() const;

    /**
     * @brief Updates the client's nickname.
     * @param newNickname The validated new nickname string.
     * @post nickname is updated.
     */
    void setNickname(const std::string &newNickname);

    /**
     * @brief Retrieves the client's username.
     * @return Username string.
     */
    const std::string &getUsername() const;

    /**
     * @brief Retrieves the client's real name.
     * @return Real name string.
     */
    const std::string &getRealname() const;

    /**
     * @brief Sets user details from the USER command.
     * @param newUsername Inbound username parameter.
     * @param newRealname Inbound real name parameter.
     * @post username and realname fields are populated.
     */
    void setUserInfo(
        const std::string &newUsername,
        const std::string &newRealname
    );

    /**
     * @brief Checks whether the client has completed full IRC registration.
     * @return true if registered (PASS, NICK, and USER all satisfied), false otherwise.
     */
    bool isRegistered() const;

    /**
     * @brief Sets client registration state.
     * @param value true when registration handshake completes.
     * @post registered status is updated.
     */
    void setRegistered(bool value);

    /**
     * @brief Flags client for termination after pending outbound data is flushed.
     * @param reason Disconnect reason (e.g. "Client Quit" or error description).
     * @post quitRequested set to true; quitReason stored.
     */
    void requestQuit(const std::string &reason);

    /**
     * @brief Checks if the client has requested disconnection.
     * @return true if QUIT was processed or socket should close, false otherwise.
     */
    bool isQuitRequested() const;

    /**
     * @brief Retrieves the disconnect reason string.
     * @return Disconnect explanation string.
     */
    const std::string &getQuitReason() const;

    /**
     * @brief Enqueues outbound message data to be sent on next writable poll event.
     * @param message Complete formatted message including trailing "\r\n".
     * @post message is appended to outputBuffer.
     */
    void queueMessage(const std::string &message);

    /**
     * @brief Checks whether there is outbound data waiting in the output buffer.
     * @return true if outputBuffer is non-empty, false otherwise.
     */
    bool hasPendingOutput() const;

    /**
     * @brief Retrieves read-only reference to the pending output buffer.
     * @return Reference to outputBuffer.
     */
    const std::string &getOutputBuffer() const;

    /**
     * @brief Erases bytes from the front of the output buffer after a successful send().
     * @param length Number of bytes successfully transmitted.
     * @pre length <= outputBuffer.size().
     * @post First length bytes removed from outputBuffer.
     */
    void removeSentData(std::size_t length);
};

#endif
