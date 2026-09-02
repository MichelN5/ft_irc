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

class Client
{
private:
    int         fd;
    std::string inputBuffer;
    std::string outputBuffer;
    bool passwordAccepted;
    std::string nickname;
    std::string username;
    std::string realname;
    bool        registered;


public:
    explicit Client(int clientFd);

    int getFd() const;
    void appendData(const char *data, std::size_t length);
    const std::string &getInputBuffer() const;
    bool hasCompleteMessage() const;
    std::string extractMessage();

    bool isPasswordAccepted() const;
    void setPasswordAccepted(bool accepted);

    const std::string &getNickname() const;
    void setNickname(const std::string &newNickname);

    const std::string &getUsername() const;
    const std::string &getRealname() const;

    void setUserInfo(
        const std::string &newUsername,
        const std::string &newRealname
    );

    bool isRegistered() const;
    void setRegistered(bool value);

    void queueMessage(const std::string &message);
    bool hasPendingOutput() const;
    const std::string &getOutputBuffer() const;
    void removeSentData(std::size_t length);

};

#endif
