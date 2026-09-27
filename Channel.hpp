#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <cstddef>
#include <set>
#include <string>

/**
 * @brief Represents an IRC communication channel.
 * 
 * Tracks channel membership, operator privileges, invitations, topic,
 * and the 5 channel modes: +i (invite-only), +t (topic-restricted),
 * +k (channel key/password), +o (operator status), and +l (user limit).
 */
class Channel
{
private:
    std::string     name;
    std::string     topic;
    std::set<int>   members;
    std::set<int>   operators;
    std::set<int>   invitedClients;
    bool            inviteOnly;
    bool            topicRestricted;
    std::string     key;
    std::size_t     userLimit;

public:
    /**
     * @brief Constructs a Channel with the specified name.
     * @param channelName Case-preserved channel name (starts with '#' or '&').
     * @post Channel initialized with empty members, default +t enabled, no key/limit.
     */
    explicit Channel(const std::string &channelName);

    /**
     * @brief Retrieves the channel's display name.
     * @return Const reference to the channel name string.
     */
    const std::string &getName() const;

    /**
     * @brief Retrieves current channel topic.
     * @return Const reference to topic string (empty if unset).
     */
    const std::string &getTopic() const;

    /**
     * @brief Sets or updates the channel topic.
     * @param newTopic The new topic string.
     * @post topic updated to newTopic.
     */
    void setTopic(const std::string &newTopic);

    /**
     * @brief Adds a client to the channel membership set.
     * @param clientFd Socket file descriptor of the joining client.
     * @post clientFd is inserted into members set.
     */
    void addMember(int clientFd);

    /**
     * @brief Removes a client from members and operator sets.
     * @param clientFd Socket file descriptor of departing client.
     * @post clientFd removed from both members and operators sets.
     */
    void removeMember(int clientFd);

    /**
     * @brief Checks if a client is currently a member of this channel.
     * @param clientFd Socket file descriptor to check.
     * @return true if member, false otherwise.
     */
    bool hasMember(int clientFd) const;

    /**
     * @brief Retrieves the set of all member file descriptors.
     * @return Const reference to the members set.
     */
    const std::set<int> &getMembers() const;

    /**
     * @brief Returns current total member count.
     * @return Number of members in the channel.
     */
    std::size_t getMemberCount() const;

    /**
     * @brief Checks if channel has zero members remaining.
     * @return true if members set is empty, false otherwise.
     */
    bool isEmpty() const;

    /**
     * @brief Grants operator status (+o) to a client.
     * @param clientFd Socket file descriptor of recipient.
     * @post clientFd inserted into operators set.
     */
    void addOperator(int clientFd);

    /**
     * @brief Revokes operator status (-o) from a client.
     * @param clientFd Socket file descriptor of target.
     * @post clientFd erased from operators set.
     */
    void removeOperator(int clientFd);

    /**
     * @brief Checks if a client possesses channel operator status.
     * @param clientFd Socket file descriptor to test.
     * @return true if client is an operator, false otherwise.
     */
    bool isOperator(int clientFd) const;

    /**
     * @brief Checks if the channel currently has at least one operator.
     * @return true if operators set is non-empty, false if chanopless.
     */
    bool hasOperators() const;

    /**
     * @brief Records an invitation for a client on this channel.
     * @param clientFd Socket file descriptor of invited user.
     * @post clientFd inserted into invitedClients set.
     */
    void invite(int clientFd);

    /**
     * @brief Removes/consumes a client's invitation after joining or invalidation.
     * @param clientFd Socket file descriptor of user.
     * @post clientFd removed from invitedClients set.
     */
    void removeInvitation(int clientFd);

    /**
     * @brief Checks if a client holds a valid pending invitation to this channel.
     * @param clientFd Socket file descriptor to test.
     * @return true if invited, false otherwise.
     */
    bool isInvited(int clientFd) const;

    /**
     * @brief Checks if channel is in invite-only mode (+i).
     * @return true if +i is active, false otherwise.
     */
    bool isInviteOnly() const;

    /**
     * @brief Sets or unsets invite-only mode (+i / -i).
     * @param value true to enable +i, false to disable -i.
     * @post inviteOnly updated.
     */
    void setInviteOnly(bool value);

    /**
     * @brief Checks if channel topic is restricted to operators (+t).
     * @return true if +t is active, false if -t.
     */
    bool isTopicRestricted() const;

    /**
     * @brief Sets or unsets topic restriction mode (+t / -t).
     * @param value true for +t (ops only), false for -t (all members).
     * @post topicRestricted updated.
     */
    void setTopicRestricted(bool value);

    /**
     * @brief Retrieves current channel key/password (+k).
     * @return Key string (empty if unset).
     */
    const std::string &getKey() const;

    /**
     * @brief Sets channel key (+k) or clears it when passed empty string (-k).
     * @param newKey Password string required to join.
     * @post key updated.
     */
    void setKey(const std::string &newKey);

    /**
     * @brief Checks if a channel key is currently required (+k).
     * @return true if key is non-empty, false otherwise.
     */
    bool hasKey() const;

    /**
     * @brief Retrieves the maximum user limit (+l).
     * @return Maximum allowed member count (0 means no limit).
     */
    std::size_t getUserLimit() const;

    /**
     * @brief Sets maximum user limit (+l) or removes it (-l when limit is 0).
     * @param limit Positive capacity count, or 0 for unlimited.
     * @post userLimit updated.
     */
    void setUserLimit(std::size_t limit);

    /**
     * @brief Checks if a user capacity limit is currently enforced (+l).
     * @return true if userLimit > 0, false otherwise.
     */
    bool hasUserLimit() const;
};

#endif
