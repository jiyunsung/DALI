PROG = guideagent
OBJS = guideagent.o
CFLAGS=-Wall -pedantic -std=c99 -ggdb
GFLAGS=`pkg-config --cflags --libs gtk+-3.0`
LLIBS = ../lib/cs50ds.a ../common/common.a
CC = gcc

$(PROG):
	gcc $(CFLAGS)  $(GFLAGS) guideagent.c ../lib/cs50ds.a ../common/common.a -o guideagent

all: 
	@echo "TODO: make guide_agent..."

memorytest1:
	valgrind --leak-check=full --show-leak-kinds=all ./guideagent Ruby Jiyun flume.cs.dartmouth.edu 55304

memorytest2:
	valgrind --leak-check=full --show-leak-kinds=all ./guideagent Ruby Jiyun flume.cs.dartmouth.edu 34659

test1:
	./guideagent Ruby Jiyun flume.cs.dartmouth.edu 55304

test2:
	./guideagent Ruby Jiyun flume.cs.dartmouth.edu 34659

test3:
	./guideagent -log=raw Ruby Jiyun flume.cs.dartmouth.edu 55304

clean:
	@echo "TODO: make clean guide_agent..."
	rm -f *.o
	rm -f core.*
	rm -f $(PROG)
	@echo "done\n"
