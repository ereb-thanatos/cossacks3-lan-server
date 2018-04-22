# Cossacks 3 LAN Server by Ereb

A cross-platform server for the RTS Cossacks 3, intended for use in local area networks.

* [Installation](#installation)
* [How to use](#how-to-use)
    * [Notes](#notes)
* [Compiling](#compiling)
* [License](#license)
* [Acknowledgments](#acknowledgments)

## Installation

* Windows: Download the latest GitHub [release](https://github.com/ereb-thanatos/cossacks3-lan-server/releases) and follow the instructions in the *readme.txt* file.

* Linux: You will need to compile the server from source. See [below](#compiling) for instructions.

## How to use

1. Create a backup of the configuration file *data/resources/servers.dat* from your game directory.
2. Open the configuration file *data/resources/servers.dat* in any text editor.
3. Change the IP address and keep the port number as follows:
    * If you are the one who is running the server:
    ```
    section.begin
        * = 127.0.0.1:31523
    section.end
    ```
    * Otherwise, insert the IP address of whoever runs the server on your local network:
    ```
    section.begin
        * = 192.168.0.42:31523
    section.end
    ```
4. Start the game
5. Type your nickname in the game key field in the login form. The email and password fields are irrelevant and can be left empty.
6. If you want to play on the official server again, restore the backup from step #1 and restart the game.

### Notes

* The PC running the server must be reachable on TCP port 31523. Adjust your firewall and router settings accordingly. Turn your firewall off if nothing else helps.
* The server must be running during the entire game.
* The server provides room host transition functionality. This means that the game will continue even after the room host left. 
* This server adheres to the official server's nickname limitations. Since the game key form field is more permissive, your nickname will be adjusted by the server during login if necessary. The rules are:
  * 4 to 16 characters
  * lowercase and uppercase letters from a to z
  * digits from 0 to 9
  * special characters ( ) + - _ . [ ]
  * no spaces
* This server is meant to be used in a LAN environment **only**. If you try to use it over the internet, from behind a NAT, through VPN, Hamachi or with some other network setting and encounter problems doing it, then you are on your own. It might work, or it might not, depending on your networking skills.

## Compiling

To compile, execute the following command in the source code directory:

```
g++ *cpp -DNDEBUG -I /path/to/asio/include/dir/ -lpthread -o cossacks3-server
```
You should be able to compile without *boost*, since *ASIO_STANDALONE* is defined in the headers.

## License

This project is licensed under the MIT License - see the [LICENSE.MIT](LICENSE.MIT) file for details.

## Acknowledgments

* [GSC Game World](http://www.gsc-game.ru/index.htm?lang=en) for giving us the long awaited reboot of the classic Cossacks RTS.

