#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <cstddef>
#include <set>
#include <string>

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
    explicit Channel(const std::string &channelName);

    const std::string &getName() const;
    const std::string &getTopic() const;
    void setTopic(const std::string &newTopic);

    void addMember(int clientFd);
    void removeMember(int clientFd);
    bool hasMember(int clientFd) const;
    const std::set<int> &getMembers() const;
    std::size_t getMemberCount() const;
    bool isEmpty() const;

    void addOperator(int clientFd);
    void removeOperator(int clientFd);
    bool isOperator(int clientFd) const;
    bool hasOperators() const;

    void invite(int clientFd);
    void removeInvitation(int clientFd);
    bool isInvited(int clientFd) const;

    bool isInviteOnly() const;
    void setInviteOnly(bool value);
    bool isTopicRestricted() const;
    void setTopicRestricted(bool value);

    const std::string &getKey() const;
    void setKey(const std::string &newKey);
    bool hasKey() const;

    std::size_t getUserLimit() const;
    void setUserLimit(std::size_t limit);
    bool hasUserLimit() const;
};

#endif
