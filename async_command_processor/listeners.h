// listeners.h in Otus homework#7 project

#pragma once

#include <string>
#include <memory>

class MessageBroadcaster;
class NotificationBroadcaster;

enum class Message : unsigned int;
/// Base class for message broadcast subscribers
class MessageListener
{
public:

  virtual ~MessageListener(){}

  /// Describes object's reaction to a message
  virtual void reactMessage(MessageBroadcaster* sender, Message message) = 0;
};

class NotificationListener
{
public:

  virtual ~NotificationListener(){}

  /// Describes object's reaction to a notification
  virtual void reactNotification(NotificationBroadcaster* sender) = 0;
};
