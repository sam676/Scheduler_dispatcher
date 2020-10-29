//Stephanie Michalowicz
//Operating Systems
//Lab 2 - Scheduler/Dispatcher

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <string.h>
#include <vector>
#include <cstring>
#include <sstream>
#include <locale>				
#include <algorithm>
#include <iomanip>
#include <unistd.h>
#include <ctype.h>
#include <queue>
#include <list>
#include <iterator>

using namespace std; 

//file name
ifstream the_file;

//random file
ifstream random_file;

int num_rand = 0;
unsigned int * randvals;

//IO stats variables
int IO_busy_counter = 0;
int IO_busy_start_time =0;
int IO_busy_total_time = 0;

int number = 0;

//STATES TO TRANSITION TO
enum state_trans_t{
	 	TO_READY,
        TO_RUNNING,
        TO_BLOCKED,
        TO_PREEMPT
};

//POSSIBLE CURRENT STATES
enum state_t{
    ST_READY,
    ST_RUNNING,
    ST_BLOCKED,
    ST_PREEMPT
};

//Process struct
struct Process {
	//variables for specification, never modified
	int arrivalTime;
	int cpuTotalTime;
	int cpuBurst;
	int ioBurst;
	//below variables can be modified
	int remainingCPU;
	int remainingIO;
    state_t currentState;
    state_trans_t futureState;
    int last_time_ts;            //beginning time of last state
    int time_in_prev_state;
    int ready_waiting_time;      //time spent in ready state (CPU WAITING TIME)
    int io_time;
    int cpu_running_time;		//last CPU running time
//    int total_cpu_time;			//sum of CPU running time
    int finishTime;
};

//Event struct
struct Event {
	int timeStamp;			//the time the event fires
	struct Process *proc;	//pointer to a process
    state_trans_t nextState;  		//next state (the state to transition to)
	int timeInPrevState;
};

list<Event*> eventQueue;   //list of process events that must be strictly ordered

//ready queue
list<Process*> readyQueue;

////////////////////////////////////////////////////////////////
////////////Generic scheduler parent class//////////////////////

class Scheduler{
	public:
		//Constructor
		Scheduler(){};

		//Functions
		virtual Process * get_from_queue () = 0;
		virtual void add_to_queue (Process*) = 0;
//		virtual int get_quantum () = 0;
};

//Global scheduler variable
Scheduler * sched;

////////////////////////////////////////////////////////////////
///////////////////////FCFS Scheduler///////////////////////////
class FCFS : public Scheduler{
	public:
		//constructor
		FCFS(){};

		//functions
		Process * get_from_queue();
		void add_to_queue(Process * p);
		//int get_quantum();
};

Process * FCFS :: get_from_queue(){
	if (readyQueue.empty()) return NULL;
	struct Process * FCFS_proc = readyQueue.front();
	readyQueue.pop_front();
	return FCFS_proc;
};

void FCFS :: add_to_queue (Process * p){

//	cout << "inside FCFS: " << __FUNCTION__ << " : " << __LINE__ << endl;

	//otherwise insert process 
	readyQueue.push_back(p);
};

//int FCFS :: get_quantum (){
//	return 10000000;
//};


//class LCLS : public Scheduler{
//public:
//		//constructor
//		LCLS(){};
//
//		//functions
//		Process * get_from_queue();
//		void add_to_queue(Process * p);
//};
//
//Process * LCLS ::get_from_queue(){
//	if(readyQueue.empty()) return NULL;
//};


/////////////////////////////////////////////////////////////////

//add process to event queue
void add_event(int ts, struct Process *p, state_trans_t next_state){

	struct Event * e = new struct Event;
	e -> timeStamp = ts;
	e -> proc = p;
	e -> nextState = next_state;

	//if the vector is empty, push the new process onto the queue
	if (eventQueue.empty()){
		eventQueue.push_back(e);
		return;
	}

    list<Event*>::iterator it;
	for (it = eventQueue.begin(); it != eventQueue.end(); ++it) {

		if (((*it)->timeStamp) <= ((unsigned long) ts)){
//			printf("inside the if statement: %d %d\n", (*it)->timeStamp,ts);
			continue;
		}
		
//		printf("insert event: %d %d\n", (*it)->timeStamp,ts);
		eventQueue.insert(it,e);
		return;
	}
	eventQueue.push_back(e);
}

//get event from event queue
Event* get_event()
{
	if (eventQueue.empty()) return NULL;

	struct Event * ev = eventQueue.front();
	eventQueue.pop_front();
	return ev;
}

//peek into next event, but don't pop it yet
int get_next_event_ts()
{
	if (eventQueue.empty())
		return -1;

	struct Event * ev = eventQueue.front();
	return ev->timeStamp;
}


//get one process
struct Process * get_process(){

	int v1, v2, v3 , v4;

	//return null if at the end of the file
	if (the_file.eof()){
		return NULL;
	}
	
	//stream ints from the file into variables
	if (! (the_file >> v1 >> v2 >> v3 >> v4) ) {
	    // this means more input
	    return NULL;
	}

	//create a new process, with specifications
	struct Process * p = new struct Process;
	p -> arrivalTime = v1;
	p -> cpuTotalTime = v2;
	p -> cpuBurst = v3;
	p -> ioBurst = v4;

	p -> remainingCPU = p->cpuTotalTime;
	p -> remainingIO = p->ioBurst;
	p-> currentState = ST_BLOCKED;
	p-> last_time_ts = p->arrivalTime;

	return p;
};

void printResults(Process * p){
	int arrival = p->arrivalTime;
	int totalCPU = p->cpuTotalTime;
	int cpuB = p->cpuBurst;
	int ioB = p->ioBurst;
	int PRIO = 0;
	int FT = p->finishTime;  //FINISHING TIME
	int TT = (FT - arrival);  //TURN AROUND TIME
	int IT = p->io_time; // I/O TIME (TIME IN BLOCKED STATE)
	int CW = p->ready_waiting_time;
	printf("%04d: %4d %4d %4d %4d %1d | %5d %5d %5d %5d\n", number, arrival, totalCPU, cpuB, ioB, PRIO, FT, TT, IT, CW);
	number = number + 1;
};

//look for the random file and open it
void readRandomFile(char * fname) {

		random_file.open(fname);

		//throw an error if the random file won't open
		if(!random_file.is_open()){
			printf("Can't open the random file!\n");
			exit(99);
		}

		///////read the first number to determine the number of random numbers 
		random_file >> num_rand;
		randvals = new unsigned int[num_rand];

		//////////read the random file line by line into a global array
		int i;
		for (i = 0; i< num_rand; i++){
			unsigned int number;
			random_file >> number;
			randvals[i] = number;
		}

	random_file.close();
}

//return a random number
int myRandom(int burst){

	static int ofs = 0;
	int retValue = (randvals[ofs] % burst);
	ofs = (ofs + 1) % num_rand;
	return retValue;
}

void Simulation() {

    //create a new process, with specifications
	struct Process *curproc = NULL; // currently running process
    struct Process *p;
	bool CALL_SCHEDULER = false;    //the scheduler is running if true
    struct Event *ev;
    int CURRENT_TIME;

    //Get all processes and add them to the event queue
    while ((p = get_process())) {
        add_event(p->arrivalTime, p, p->futureState);
//        printf("%d %d %d %d\n", p->arrivalTime, p->cpuTotalTime, p->cpuBurst, p->ioBurst);
    }

    while ((ev = get_event()) != NULL) {
        CURRENT_TIME = ev->timeStamp; // time has now discretely jumped to this
        p = ev->proc;  // we are operating on this process
		p->time_in_prev_state = CURRENT_TIME - p->last_time_ts; //duration of time the process spent in the previous state

        state_trans_t toState = ev->nextState; // this assumes you have not merged the state_t and state_trans_t
        state_t prevState = p->currentState;    // this assumes you have not merged the state_t and state_trans_t

		//calculate IO and CPU burst times
		int currentCPU_burst = myRandom(p->cpuBurst);
		int currentIO_burst = myRandom(p->ioBurst);

		//set the process' time stamp
        p->last_time_ts = CURRENT_TIME;  // we are executing the state transition below

        //remove current event object from memory
        delete ev;
        ev = NULL;
        // never ever refer to "ev" below;

        switch (toState) {   //the state the event should transition to
            case TO_READY:
                //must come from BLOCKED or PREEMPTION

                CALL_SCHEDULER = true;  //conditional on ===>  you must calculate
                // how long the process was blocked if it came from blocked ===>
                p->currentState = ST_READY;

				//time spent waiting (either blocked or ready)
				p->ready_waiting_time += p->time_in_prev_state;

//				cout << "TO_READY (p arrival time) " << p->arrivalTime << endl;

                //this process was blocked (in I/o) but is now ready
                //subtract one from IO counter
                //if there are zero processes in IO (IO = FREE)....
                //calculate how long the latest process was busy in IO
                //add last IO busy time to total IO busy time
                if(--IO_busy_counter == 0){
                    IO_busy_total_time += (CURRENT_TIME - IO_busy_start_time);

//                    cout << "IO busy time: " << IO_busy_total_time << endl;
                }
				add_event(CURRENT_TIME, p, TO_RUNNING);
				sched->add_to_queue(p);
                break;
            case TO_RUNNING:
                curproc = p;

//				cout << "TO_RUNNING" << endl;
//				cout << "current CPU BURST TO RUNNING: " << currentCPU_burst << "remaingin CPU BURST" << p->remainingCPU  << endl;
                //set CPU burst based on how much remaining time the process needs in the CPU
                if (currentCPU_burst > curproc->remainingCPU){
                    currentCPU_burst = curproc->remainingCPU;
//                    cout << "current CPU BURST: " << currentCPU_burst << endl;
                }else{

                	curproc->remainingCPU -= currentCPU_burst;
//					cout << "p->remainingCPU " << curproc->remainingCPU << endl;

                }

                //set current state
                curproc->currentState = ST_RUNNING;
				//time spent waiting (either blocked or ready)
				curproc->ready_waiting_time += curproc->time_in_prev_state;


				//if the process has finished running
				if(curproc->remainingCPU <= 0) {
					curproc->finishTime = CURRENT_TIME;
//					cout << "got here!!" << endl;
					printResults(curproc);
				}else{
					add_event(CURRENT_TIME + currentCPU_burst, curproc, TO_BLOCKED);
				}

				break;
			case TO_BLOCKED:
				CALL_SCHEDULER = true;
//				cout << "TO_BLOCKED" << endl;
			    currentIO_burst = myRandom(p->ioBurst);
				p->currentState = ST_BLOCKED;
				add_event(CURRENT_TIME + currentIO_burst, p, TO_READY);
				if(++IO_busy_counter==1){
				    IO_busy_start_time = CURRENT_TIME;
//					cout << "IO BUSY START TIME" << IO_busy_start_time << endl;
				}
				p->io_time= IO_busy_start_time + p->ioBurst;
				break;
        }

        if(CALL_SCHEDULER){

            if(get_next_event_ts() == CURRENT_TIME){

//				cout << "ts == current time " << endl;
                continue; //process next event from the queue
            }

            CALL_SCHEDULER = false;

            if(curproc == NULL){
//				cout << "curproc == NULL" << endl;
                curproc = sched->get_from_queue();

//                cout << "inside curproc NULL, arrival time = " << curproc->arrivalTime << endl;

                if(curproc == NULL){
//                    add_event(CURRENT_TIME, p, TO_RUNNING);
                    continue;
                }

                //create event to make process runnable for same time
            }
        }

    }

}


/////////////////////////////////////////////////////main function
    int main(int argc, char *argv[]) {

        char option;

        while ((option = getopt(argc, argv, "v:s:")) != -1) {
            switch (option) {
                //alogrithm options
                case 's': {
                    switch (*optarg) {
                        case 'F': {
                            //FIFO
                            sched = new FCFS();
                            printf("FCFS\n");
                            break;
                        }
                        default: {
                            //default
                            sched = new FCFS();
							printf("FCFS\n");
                            break;
                        }
                    }
                }
            }
        }


        //look for a file and open it
        the_file.open(argv[optind]);

        //throw an error if the file won't open
        if (!the_file.is_open()) {
            printf("Can't open the file!\n");
            exit(99);
        }

        //read the random file
        readRandomFile(argv[optind + 1]);

        //start the simulation
        Simulation();

        /***********************************/

        // printout the sum results
////     printf(“SUM: %d %.2lf %.2lf %.2lf %.2lf %.3lf\n",);


        return 0;

        //close the file
        the_file.close();
    }

    
