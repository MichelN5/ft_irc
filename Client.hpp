/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mnaouss <mnaouss@student.42beirut.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/01 22:15:36 by mnaouss           #+#    #+#             */
/*   Updated: 2026/09/01 22:51:41 by mnaouss          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <cstddef>

class Client
{
private:
    int         fd;
    std::string inputBuffer;

public:
    explicit Client(int clientFd);

    int getFd() const;
    void appendData(const char *data, std::size_t length);
    const std::string &getInputBuffer() const;
    bool hasCompleteMessage() const;
    std::string extractMessage();
};

#endif