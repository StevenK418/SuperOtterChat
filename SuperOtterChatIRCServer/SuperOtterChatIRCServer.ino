#include <ESP8266WiFi.h>
#include "ServerConfig.h"

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// IRC Server settings
#define IRC_PORT 6667
#define MAX_CLIENTS 5
#define SERVER_NAME "SuperOtterChat"
#define SERVER_VERSION "0.1"

WiFiServer server(IRC_PORT);
WiFiClient clients[MAX_CLIENTS];

struct IRCUser {
  String nick;
  String user;
  String realname;
  bool registered;
  String channels;
};

IRCUser users[MAX_CLIENTS];

void setup() {
  Serial.begin(115200);
  delay(10);
  
  // Initialize user structs
  for (int i = 0; i < MAX_CLIENTS; i++) {
    users[i].registered = false;
  }
  
  // Connect to WiFi
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  // Start IRC server
  server.begin();
  Serial.print("IRC Server started on port ");
  Serial.println(IRC_PORT);
  Serial.println("Ready for connections!");
}

void loop() {
  // Check for new clients
  if (server.hasClient()) {
    bool connected = false;
    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (!clients[i] || !clients[i].connected()) {
        if (clients[i]) clients[i].stop();
        clients[i] = server.available();
        Serial.print("New client connected: ");
        Serial.println(i);
        
        // Send welcome on connect
        sendToClient(i, ":"+String(SERVER_NAME)+" NOTICE AUTH :*** Looking up your hostname...");
        sendToClient(i, ":"+String(SERVER_NAME)+" NOTICE AUTH :*** Found your hostname");
        
        connected = true;
        break;
      }
    }
    
    if (!connected) {
      // No free slot, reject
      WiFiClient rejectClient = server.available();
      rejectClient.println("ERROR :Server is full");
      rejectClient.stop();
    }
  }
  
  // Handle existing clients
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i] && clients[i].connected()) {
      if (clients[i].available()) {
        String line = clients[i].readStringUntil('\n');
        line.trim();
        if (line.length() > 0) {
          handleIRCCommand(i, line);
        }
      }
    } else {
      // Client disconnected
      if (clients[i]) {
        Serial.print("Client disconnected: ");
        Serial.println(i);
        if (users[i].nick.length() > 0) {
          broadcastMessage(":" + users[i].nick + " QUIT :Client disconnected");
        }
        users[i].nick = "";
        users[i].user = "";
        users[i].realname = "";
        users[i].registered = false;
        users[i].channels = "";
        clients[i].stop();
      }
    }
  }
  
  delay(1);
}

void sendToClient(int clientId, String message) {
  if (clients[clientId] && clients[clientId].connected()) {
    clients[clientId].println(message);
    Serial.print("-> [");
    Serial.print(clientId);
    Serial.print("] ");
    Serial.println(message);
  }
}

void broadcastMessage(String message) {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i] && clients[i].connected() && users[i].registered) {
      sendToClient(i, message);
    }
  }
}

void broadcastToChannel(String channel, String message, int exceptClient = -1) {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (i != exceptClient && clients[i] && clients[i].connected() && users[i].registered) {
      if (users[i].channels.indexOf(channel) >= 0) {
        sendToClient(i, message);
      }
    }
  }
}

void handleIRCCommand(int clientId, String line) {
  Serial.print("<- [");
  Serial.print(clientId);
  Serial.print("] ");
  Serial.println(line);
  
  // Parse command
  line.trim();
  int spacePos = line.indexOf(' ');
  String cmd = spacePos > 0 ? line.substring(0, spacePos) : line;
  String params = spacePos > 0 ? line.substring(spacePos + 1) : "";
  
  cmd.toUpperCase();
  
  // Handle NICK command
  if (cmd == "NICK") {
    String oldNick = users[clientId].nick;
    users[clientId].nick = params;
    users[clientId].nick.trim();
    
    if (users[clientId].registered) {
      broadcastMessage(":" + oldNick + " NICK :" + users[clientId].nick);
    } else {
      checkRegistration(clientId);
    }
  }
  
  // Handle USER command
  else if (cmd == "USER") {
    int pos1 = params.indexOf(' ');
    if (pos1 > 0) {
      users[clientId].user = params.substring(0, pos1);
      int pos2 = params.indexOf(':', pos1);
      if (pos2 > 0) {
        users[clientId].realname = params.substring(pos2 + 1);
      }
    }
    checkRegistration(clientId);
  }
  
  // Handle PING command
  else if (cmd == "PING") {
    sendToClient(clientId, "PONG " + String(SERVER_NAME) + " :" + params);
  }
  
  // Handle JOIN command
  else if (cmd == "JOIN") {
    if (!users[clientId].registered) return;
    
    String channel = params;
    channel.trim();
    if (channel.indexOf(' ') > 0) {
      channel = channel.substring(0, channel.indexOf(' '));
    }
    
    if (channel.charAt(0) != '#') {
      channel = "#" + channel;
    }
    
    // Add channel to user's channel list
    if (users[clientId].channels.indexOf(channel) < 0) {
      if (users[clientId].channels.length() > 0) {
        users[clientId].channels += ",";
      }
      users[clientId].channels += channel;
      
      // Notify all users in channel
      broadcastToChannel(channel, ":" + users[clientId].nick + " JOIN :" + channel);
      
      // Send topic
      sendToClient(clientId, ":" + String(SERVER_NAME) + " 332 " + users[clientId].nick + " " + channel + " :Welcome to " + channel);
      
      // Send names list
      String namesList = "";
      for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i].connected() && users[i].registered) {
          if (users[i].channels.indexOf(channel) >= 0) {
            if (namesList.length() > 0) namesList += " ";
            namesList += users[i].nick;
          }
        }
      }
      sendToClient(clientId, ":" + String(SERVER_NAME) + " 353 " + users[clientId].nick + " = " + channel + " :" + namesList);
      sendToClient(clientId, ":" + String(SERVER_NAME) + " 366 " + users[clientId].nick + " " + channel + " :End of /NAMES list.");
    }
  }
  
  // Handle PRIVMSG command
  else if (cmd == "PRIVMSG") {
    if (!users[clientId].registered) return;
    
    int colonPos = params.indexOf(':');
    if (colonPos > 0) {
      String target = params.substring(0, colonPos);
      target.trim();
      String message = params.substring(colonPos + 1);
      
      if (target.charAt(0) == '#') {
        // Channel message
        broadcastToChannel(target, ":" + users[clientId].nick + " PRIVMSG " + target + " :" + message, clientId);
      } else {
        // Private message to user
        for (int i = 0; i < MAX_CLIENTS; i++) {
          if (users[i].nick.equalsIgnoreCase(target) && users[i].registered) {
            sendToClient(i, ":" + users[clientId].nick + " PRIVMSG " + target + " :" + message);
            break;
          }
        }
      }
    }
  }
  
  // Handle PART command
  else if (cmd == "PART") {
    if (!users[clientId].registered) return;
    
    String channel = params;
    if (channel.indexOf(':') > 0) {
      channel = channel.substring(0, channel.indexOf(':'));
    }
    channel.trim();
    
    if (users[clientId].channels.indexOf(channel) >= 0) {
      broadcastToChannel(channel, ":" + users[clientId].nick + " PART :" + channel);
      
      // Remove channel from user's list
      users[clientId].channels.replace(channel + ",", "");
      users[clientId].channels.replace("," + channel, "");
      users[clientId].channels.replace(channel, "");
    }
  }
  
  // Handle QUIT command
  else if (cmd == "QUIT") {
    if (users[clientId].registered) {
      broadcastMessage(":" + users[clientId].nick + " QUIT :Client quit");
    }
    clients[clientId].stop();
  }
  
  // Handle WHO command
  else if (cmd == "WHO") {
    if (!users[clientId].registered) return;
    String channel = params;
    channel.trim();
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (clients[i] && clients[i].connected() && users[i].registered) {
        if (users[i].channels.indexOf(channel) >= 0) {
          sendToClient(clientId, ":" + String(SERVER_NAME) + " 352 " + users[clientId].nick + " " + channel + " " + users[i].user + " " + SERVER_NAME + " " + SERVER_NAME + " " + users[i].nick + " H :0 " + users[i].realname);
        }
      }
    }
    sendToClient(clientId, ":" + String(SERVER_NAME) + " 315 " + users[clientId].nick + " " + channel + " :End of /WHO list.");
  }
  
  // Handle MODE command (basic stub)
  else if (cmd == "MODE") {
    // Just acknowledge, don't actually implement modes
    if (users[clientId].registered) {
      String target = params;
      if (target.indexOf(' ') > 0) {
        target = target.substring(0, target.indexOf(' '));
      }
      sendToClient(clientId, ":" + String(SERVER_NAME) + " 324 " + users[clientId].nick + " " + target + " +");
    }
  }
}

void checkRegistration(int clientId) {
  if (!users[clientId].registered && users[clientId].nick.length() > 0 && users[clientId].user.length() > 0) {
    users[clientId].registered = true;
    
    // Send welcome messages (IRC numerics)
    sendToClient(clientId, ":" + String(SERVER_NAME) + " 001 " + users[clientId].nick + " :Welcome to NodeMCU IRC " + users[clientId].nick + "!" + users[clientId].user + "@" + SERVER_NAME);
    sendToClient(clientId, ":" + String(SERVER_NAME) + " 002 " + users[clientId].nick + " :Your host is " + String(SERVER_NAME) + ", running version " + SERVER_VERSION);
    sendToClient(clientId, ":" + String(SERVER_NAME) + " 003 " + users[clientId].nick + " :This server was created just now");
    sendToClient(clientId, ":" + String(SERVER_NAME) + " 004 " + users[clientId].nick + " " + String(SERVER_NAME) + " " + SERVER_VERSION + " o o");
    sendToClient(clientId, ":" + String(SERVER_NAME) + " 375 " + users[clientId].nick + " :- Message of the day -");
    sendToClient(clientId, ":" + String(SERVER_NAME) + " 372 " + users[clientId].nick + " :- Welcome to NodeMCU IRC Server!");
    sendToClient(clientId, ":" + String(SERVER_NAME) + " 372 " + users[clientId].nick + " :- This is a minimal IRC server running on ESP8266");
    sendToClient(clientId, ":" + String(SERVER_NAME) + " 376 " + users[clientId].nick + " :End of /MOTD command.");
    
    Serial.print("User registered: ");
    Serial.println(users[clientId].nick);
  }
}
