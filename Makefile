
EXE:=ledtest

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