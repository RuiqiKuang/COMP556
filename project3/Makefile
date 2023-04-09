CC = g++
COPTS = -g -Wall -std=c++11
LKOPTS = 

OBJS =\
    DVProImp.o\
	Event.o\
	Link.o\
	LSImpl.o\
	Node.o\
	RoutingProtocolImpl.o\
	Simulator.o

HEADRES =\
    DVProImp.h\
	global.h\
	Event.h\
	Link.h\
	Node.h\
	RoutingProtocol.h\
	Simulator.h

%.o: %.cc
	$(CC) $(COPTS) -c $< -o $@

all:	Simulator

Simulator: $(OBJS)
	$(CC) $(LKOPTS) -o Simulator $(OBJS)

$(OBJS): global.h
Event.o: Event.h Link.h Node.h Simulator.h
Link.o: Event.h Link.h Node.h Simulator.h
Node.o: Event.h Link.h Node.h Simulator.h
Simulator.o: Event.h Link.h Node.h RoutingProtocol.h Simulator.h 
RoutingProtocolImpl.o: RoutingProtocolImpl.h

clean:
	rm -f *.o Simulator

