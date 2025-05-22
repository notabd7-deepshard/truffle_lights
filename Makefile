EXE:=ledtest

CONNECT_EXE:=test_connecting

.PHONY: all
all: $(EXE)

.PHONY: clean
clean:
	rm -fv $(EXE) *.o 

$(EXE):
	g++ -I. ledcontrol.cc main.cc -o $(EXE)

.PHONY: exec
exec:
	sudo ./$(EXE)

.PHONY: run
run: clean all exec

.PHONY: stop
stop:
	sudo systemctl stop truffle.service

.PHONY: start
start:
	sudo systemctl start truffle.service

.PHONY: connecting
connecting: $(CONNECT_EXE)

$(CONNECT_EXE):
	g++ -I. ledcontrol.cc test_connecting_state.cc -o $(CONNECT_EXE)

.PHONY: connect
connect: clean connecting exec_connect

.PHONY: exec_connect
exec_connect:
	sudo ./$(CONNECT_EXE)