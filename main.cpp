/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mnaouss <mnaouss@student.42beirut.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/24 20:54:20 by mnaouss           #+#    #+#             */
/*   Updated: 2026/09/01 23:26:45 by mnaouss          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"

#include <iostream>
#include <cstdlib>
#include <cerrno>

static bool parsePort(const char *text, int &port)
{
    char *end = NULL;

    errno = 0;
    long value = std::strtol(text, &end, 10);

    if (errno != 0 ||
        text[0] == '\0' ||
        *end != '\0' ||
        value < 1 ||
        value > 65535)
    {
        return false;
    }

    port = static_cast<int>(value);
    return true;
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: "
                  << argv[0]
                  << " <port> <password>"
                  << std::endl;
        return 1;
    }

    int port;

    if (!parsePort(argv[1], port))
    {
        std::cerr << "Invalid port" << std::endl;
        return 1;
    }

    if (argv[2][0] == '\0')
    {
        std::cerr << "Password cannot be empty"
                  << std::endl;
        return 1;
    }

    Server server(port, argv[2]);

    return server.run();
}
