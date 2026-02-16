# SuperOtterChat
A nodeMCU based IRC (Internet Relay Chat) server. 

## How to use this sketch
- Open the ino file in the Arduino IDE.
- Open the ServerConfig.h file in a text editor and add your network SSID and Passwor and save. 
- Connect a NodeMCU (or similar) board to your machine and make sure it is detected by the Arduino IDE. 
- Once the board is connected and recognized by Arduino, click upload in the IDE to upload the sketch to the NodeMCU board. 
- Once done, in the Arduino IDE, go to Tools -> Serial Monitor. 
- In the serial monitor, set the Baud rate to 115200
- The IP address of the node MCU should be displayed. 
- THe IP address can be used in an IRC client such as HexChat to connect to the server via port 6667. 

## Commands
- /nick SuperOtter          # Change nickname
- /join #general            # Join #general channel
- /msg #general hello!      # Send message to channel
- /msg OtherUser hi there   # Send private message to user
- /who #general             # See all users in #general
- /part #general            # Leave the channel
- /quit                     # Disconnect
- /list                     # Gets the channel list