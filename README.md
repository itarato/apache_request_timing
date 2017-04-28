Apache Server - Request Timer
=============================

This is a little experiment to see if request times can be examined realtime - and maybe apply some predition based on statistics.

# Requirements

- apache 2.x server source files
- clang or gcc

# Install

Compile and enable apache module:

```
sudo apxs -i -a -c mod_timing.c
sudo a2enmod timing
sudo service apache2 restarts
```

Compile and run the client:

```
clang++-3.8 -std=c++14 -lncurses -pthread client.cpp -o client
./client
```


# Usage

Enable apache module and run the client. [ESC] is to quit from client.

