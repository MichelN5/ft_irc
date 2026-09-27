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
#include <csignal>

/**
 * @brief Parses and validates a port string argument into an integer.
 * @param text The ASCII string representing the port number.
 * @param port Output reference to store the validated integer port.
 * @return true if string represents an integer in range [1, 65535], false otherwise.
 * @pre text must be a valid null-terminated C string.
 * @post If valid, port is set to the parsed value.
 * @note Rejects non-numeric characters, empty string, overflow, and out-of-range ports.
 */
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

/**
 * @brief Program entry point for the ft_irc server.
 * @param argc Command-line argument count.
 * @param argv Command-line argument strings (<port> <password>).
 * @return 0 on clean server shutdown, 1 on argument validation or runtime failure.
 * @pre Target execution platform POSIX / Linux.
 * @post Configures signal handlers (SIGPIPE ignored, SIGINT/SIGTERM gracefully handled),
 *       instantiates Server, and enters event loop.
 */
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

    std::signal(SIGPIPE, SIG_IGN);
    std::signal(SIGINT, Server::handleSignal);
    std::signal(SIGTERM, Server::handleSignal);

    Server server(port, argv[2]);

    return server.run();
}
