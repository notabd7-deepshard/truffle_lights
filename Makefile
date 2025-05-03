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

# Build test program cycling through all states
STATE_TEST:=led_state_test

$(STATE_TEST): ledcontrol.cc test_led_states.cc
	g++ -I. ledcontrol.cc test_led_states.cc -o $(STATE_TEST) -pthread

.PHONY: state_test
state_test: $(STATE_TEST)