/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mnaouss <mnaouss@student.42beirut.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/01 22:15:36 by mnaouss           #+#    #+#             */
/*   Updated: 2026/09/02 16:48:31 by mnaouss          ###   ########.fr       */
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
    bool passwordAccepted;
    std::string nickname;

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
    
};

#endif