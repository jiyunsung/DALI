# Guideagent.c - program to display data on codedrops and field agents and send hints to field agents

## Jiyun Sung, May 2016

Usage: ./guideagent [-log=raw] teamName playerName GShost GSport

Compile: type make.

Exit Status:
* 0 - success
* 1 - incorrect number of arguments
* 2 - unable to create socket
* 3 - unidentified host
* 4 - unable to open log file

Testing:
Before game server was complete, I used the chatserver program provided by class and set its port to our proxy, 3469. I manually typed possible game server messages

test1: game server test
test2: chatserver test
test3: game server test with -log=raw

memorytest1: game server test with valgrind
memorytest2: chatserver test with valgrind

Assumptions:
There are less than 10 field agents.
There are less than 50 code drops.
Memory leaks because of GTK are ignored.
There is a responding server.
X11 window is available on the user's part.