#include "Channel.hpp"

Channel::Channel(const std::string &channelName)
    : name(channelName),
      topic(),
      members(),
      operators(),
      invitedClients(),
      inviteOnly(false),
      topicRestricted(true),
      key(),
      userLimit(0)
{
}

const std::string &Channel::getName() const
{
    return name;
}

const std::string &Channel::getTopic() const
{
    return topic;
}

void Channel::setTopic(const std::string &newTopic)
{
    topic = newTopic;
}

void Channel::addMember(int clientFd)
{
    members.insert(clientFd);
}

void Channel::removeMember(int clientFd)
{
    members.erase(clientFd);
    operators.erase(clientFd);
}

bool Channel::hasMember(int clientFd) const
{
    return members.find(clientFd) != members.end();
}

const std::set<int> &Channel::getMembers() const
{
    return members;
}

std::size_t Channel::getMemberCount() const
{
    return members.size();
}

bool Channel::isEmpty() const
{
    return members.empty();
}

void Channel::addOperator(int clientFd)
{
    operators.insert(clientFd);
}

void Channel::removeOperator(int clientFd)
{
    operators.erase(clientFd);
}

bool Channel::isOperator(int clientFd) const
{
    return operators.find(clientFd) != operators.end();
}

bool Channel::hasOperators() const
{
    return !operators.empty();
}

void Channel::invite(int clientFd)
{
    invitedClients.insert(clientFd);
}

void Channel::removeInvitation(int clientFd)
{
    invitedClients.erase(clientFd);
}

bool Channel::isInvited(int clientFd) const
{
    return invitedClients.find(clientFd) != invitedClients.end();
}

bool Channel::isInviteOnly() const
{
    return inviteOnly;
}

void Channel::setInviteOnly(bool value)
{
    inviteOnly = value;
}

bool Channel::isTopicRestricted() const
{
    return topicRestricted;
}

void Channel::setTopicRestricted(bool value)
{
    topicRestricted = value;
}

const std::string &Channel::getKey() const
{
    return key;
}

void Channel::setKey(const std::string &newKey)
{
    key = newKey;
}

bool Channel::hasKey() const
{
    return !key.empty();
}

std::size_t Channel::getUserLimit() const
{
    return userLimit;
}

void Channel::setUserLimit(std::size_t limit)
{
    userLimit = limit;
}

bool Channel::hasUserLimit() const
{
    return userLimit != 0;
}
