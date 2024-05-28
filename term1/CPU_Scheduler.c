#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_PROCESS_N 100
#define MAX_PID 10000
#define MAX_BURST_TIME 10
#define MAX_ARRIVAL_TIME 15
#define MAX_PRIORITY 10
#define ALG_NUM 8
#define INF 100000000

enum Algorithm { FCFS_ALG, SJF_ALG, PRIORITY_ALG, P_SJF_ALG, P_PRIORITY_ALG, RR_ALG }; // For algorithm selection
enum Criteria { PID_CRIT, BURST_TIME_CRIT, ARRIVAL_TIME_CRIT, PRIORITY_CRIT }; // Criteria for getting Process
enum State { HIGH, LOW }; // For applying criteria in ASC, DESC order
char algo_str[6][20] = { "FCFS", "SJF", "Priority", "Preemptive SJF", "Preemptive Priority", "RR" }; // For printing

typedef struct {
    int idx; // idx for searching array
    int pid; // must be unique, -1 for idle
    int process_type; // 0 for CPU-bounded, 1 for I/O-bounded, -1 for Idle
    int burst_time;
    int arrival_time;
    int priority;
} Process;

typedef struct {
    Process process;
    int start_time;
    int end_time;
} Scheduled_Process;

typedef struct {
    enum Algorithm algorithm;
    int time_quantum;
    int is_success;
    float avg_waiting_time;
    float avg_turnaround_time;
} Schedule_Stat; // Statistics of Scheduled_Processes, Interface between Schedule & Evaluate

int process_N; // Number of created processes

Process processes[MAX_PROCESS_N]; // Process pool acting as stack
int p_top = 0; // Top idx for processes

Process run_queue[MAX_PROCESS_N]; // Run queue for scheduling
int run_queue_num = 0;

Process crit_result[MAX_PROCESS_N]; // Output of Get_Processes_By_Criteria
int crit_result_num = 0;

/* ===== Debug Functions ===== */

int my_ceil(float num) {
    int inum = (int)num;
    if (num == (float)inum) return inum;
    return inum + 1;
}

void Print_Process_Info(Process process){
    printf("--------------------\n");
    printf("IDX : \t\t%d\n", process.idx);
    printf("PID : \t\t%d\n", process.pid);
    printf("Type : \t\t%d\n", process.process_type);
    printf("Burst Time : \t%d\n", process.burst_time);
    printf("Arrival Time : \t%d\n", process.arrival_time);
    printf("Priority : \t%d\n", process.priority);
    printf("--------------------\n");
}

void Print_Schedule_Result(Scheduled_Process result[], int result_num){
    printf("-----Scheduled Result-----\n");
    for(int i=0; i<result_num; i++){
        printf("pid: %d start_time: %d end_time: %d\n",
        result[i].process.pid,
        result[i].start_time,
        result[i].end_time);
    }
    printf("--------------------------\n");
}

void Print_Processes(Process processes[], int process_num){
    printf("idx\tpid\ttype\tburst time\tarrival time\tpriority\n");
    for(int i=0; i<process_num; i++){
        printf("%d\t%d\t%d\t%d\t\t%d\t\t%d\n", processes[i].idx, processes[i].pid,
            processes[i].process_type, processes[i].burst_time,
            processes[i].arrival_time, processes[i].priority);
    }
    printf("\n");
}

// Print Gantt chart graphically from scheduled result in wanted column size
void Print_Gantt_Chart(Scheduled_Process result[], int result_num, int col_num){
    printf("> Gantt Chart <\n");
    for(int base=0; base<(int)my_ceil((float)result_num/col_num); base++){
        int start = base * col_num;
        int end = (base + 1) * col_num;

        printf("+");
        for(int i=start; i<end && i<result_num; i++){
            printf("---------------+");
        }
        printf("\n|");
        for(int i=start; i<end && i<result_num; i++){
            Process process = result[i].process;
            if (process.process_type == -1){
                printf("\tIdle\t|");
            } else {
                printf("\t%d\t|", process.pid);
            }
        }
        printf("\n+");
        for(int i=start; i<end && i<result_num; i++){
            printf("---------------+");
        }
        printf("\n%d", result[start].start_time);
        for(int i=start; i<end && i<result_num; i++){
            printf("\t\t%d", result[i].end_time);
        }
        printf("\n");
    }
}

void Print_Schedule_Stats(Schedule_Stat results[], int result_num){
    printf("------------------------------Statistics------------------------------\n");
    printf("\tAlgorithm\t\tAvg_Turnaround_Time\tAvg_Waiting_Time\n");
    for (int i=0; i<result_num; i++){
        Schedule_Stat result = results[i];
        printf("%d.\t", i+1);
        if (result.algorithm == RR_ALG){
            printf("%s with %d t.q.", algo_str[result.algorithm], result.time_quantum);
            printf("\t\t");
            printf("%.2f\t\t\t%.2f\n", result.avg_turnaround_time, result.avg_waiting_time);
        } else {
            printf("%s", algo_str[result.algorithm]);
            for(int i=0; i<3-(strlen(algo_str[result.algorithm])/8); i++) printf("\t");
            printf("%.2f\t\t\t%.2f\n", result.avg_turnaround_time, result.avg_waiting_time);
        }
    }
    printf("----------------------------------------------------------------------\n");
}

/* ===== Helper Functions ===== */

void Init_Process(Process* process){
    process->idx = 0;
    process->pid = 0;
    process->process_type = 0;
    process->burst_time = 0;
    process->arrival_time = 0;
    process->priority = 0;
}

void Init_Processes(Process processes[], int process_num){
    for (int i=0; i<process_num; i++){
        Init_Process(&processes[i]);
    }
}

// Deep copy process array
void Copy_Process_Array(Process src[], Process dest[], int process_num){
    for(int i=0; i<process_num; i++){
        dest[i] = src[i];
    }
}

// Get idx of process in array by pid, return -1 if not exsits
int Get_Process_Idx_By_PID(Process processes[], int pid, int process_num){
    int idx = -1;
    for(int i=0; i<process_num; i++){
        if (processes[i].pid == pid){
            idx = i;
            break;
        }
    }
    return idx;
}

// Pop element from process array by idx
Process Pop_Process(Process processes[], int idx, int *process_num){
    Process ret;

    // Input validation
    if (idx >= *process_num || idx < 0){
        printf("Pop Out Of Range! Got %d for %d\n", idx, *process_num);
        return ret;
    }
    
    // Backup
    ret = processes[idx];

    // Shift Left, might take some time, can be changed to linked list if needed.
    for(int i=idx+1; i<*process_num; i++){
        processes[i].idx--;
        processes[i-1] = processes[i]; 
    }

    // Init Last Element
    Init_Process(&processes[*process_num-1]);
    --*process_num;

    return ret;
}

Process Create_Process(int idx, int pid, int p_type, int burst_time, int arrival_time, int priority){
    Process process;

    process.idx = idx;
    process.pid = pid;
    process.process_type = p_type;
    process.burst_time = burst_time;
    process.arrival_time = arrival_time;
    process.priority = priority;

    return process;
}

// Create random process. PID can be typed manually.
Process Create_Random_Process(int process_type, int is_pid_manual_input){
    int pid, exists;
    Process process;

    if (is_pid_manual_input){
        do{
            printf("Input PID (1 ~ %d) : ", MAX_PID);
            scanf("%d", &pid);

            exists = 0;
            for (int i=0; i<p_top; i++){
                if (processes[i].pid == pid){
                    exists = 1;
                    break;
                }
            }
            if (exists) {
                printf("PID %d already exists! Try another PID.\n", pid);
            }
        } while(pid > MAX_PID || pid <= 0 || exists);
    } else {
        pid = p_top + 1;
    }

    process = Create_Process(p_top, pid, 0,
                            (rand() % MAX_BURST_TIME) + 1,
                            (rand() % MAX_ARRIVAL_TIME),
                            (rand() % MAX_PRIORITY) + 1);

    return process;
}

Process Create_Idle_Process(int start_time, int burst_time){
    return Create_Process(-1, 0, -1, burst_time, start_time, 0);
}

Scheduled_Process Create_Scheduled_Process(Process cur_process, int prev_end_time, int burst_time){
    Scheduled_Process s_process;

    s_process.process = cur_process;
    s_process.start_time = prev_end_time;
    s_process.end_time = prev_end_time + burst_time;

    return s_process;
}

Scheduled_Process Create_Scheduled_Idle_Process(int start_time, int burst_time){
    Process idle_process = Create_Idle_Process(start_time, burst_time);
    return Create_Scheduled_Process(idle_process, start_time, burst_time);
}

// Get array of processes satisfying criteria, output : result[], return : len(result[])
int Get_Processes_By_Criteria(Process result[], Process processes[], int process_num, enum Criteria crit, enum State state, int time){
    int result_num = 0;
    int min = INF, max = -1;
    
    // Traverse all processes to get lowest or highest value process.
    // Might take some time traversing. It can be changed to other data structure like tree if needed.
    for (int i=0; i<process_num; i++){
        int val;
        Process process = processes[i];

        // Exclude out of time processes. If time is -1 ignore time.
        if (time != -1 && process.arrival_time > time){
            continue;
        }

        // Exclude idle process.
        if (process.process_type == -1){
            continue;
        }

        // Get value of criteria
        switch(crit){
            case PID_CRIT:
                val = process.pid;
                break;
            case BURST_TIME_CRIT:
                val = process.burst_time;
                break;
            case ARRIVAL_TIME_CRIT:
                val = process.arrival_time;
                break;
            case PRIORITY_CRIT:
                val = process.priority;
                break;
            default:
                printf("Wrong Criteria %d\n", crit);
                return 0;
        }

        // Push satisfying process
        if (state == LOW){
            if (val < min){
                result_num = 1;
                min = val;
                result[0] = process;
            } else if (val == min) {
                result[result_num++] = process;
            }
        } else {
            if (val > max){
                result_num = 1;
                max = val;
                result[0] = process;
            } else if (val == max) {
                result[result_num++] = process;
            }
        }
    }

    return result_num;
}


/* ===== Basic (Main) Functions ===== */

int Init(){
    p_top = 0;
    run_queue_num = 0;
    crit_result_num = 0;
    srand(time(NULL)); // random seed
}

int FCFS(Scheduled_Process result[]){
    int result_num = 0;
    int prev_end_time = 0;

    // Until all processes are popped
    while(run_queue_num > 0){
        crit_result_num = 0;
        
        // Get least arrival time process
        int crit_result_num = Get_Processes_By_Criteria(crit_result, run_queue, run_queue_num, ARRIVAL_TIME_CRIT, LOW, -1);
        if (crit_result_num > 1){
            // If more then one, get highest priority process
            crit_result_num = Get_Processes_By_Criteria(crit_result, crit_result, crit_result_num, PRIORITY_CRIT, HIGH, -1);
        }

        // Set first received process as cur_process
        Process cur_process = crit_result[0];
        
        if (cur_process.arrival_time > prev_end_time){
            // If there is a time gap, Push idle process
            Scheduled_Process s_idle_process = Create_Scheduled_Idle_Process(prev_end_time, cur_process.arrival_time - prev_end_time);
            
            result[result_num++] = s_idle_process;

            prev_end_time = s_idle_process.end_time;
        }

        // Push cur process
        result[result_num++] = Create_Scheduled_Process(cur_process, prev_end_time, cur_process.burst_time);

        // Pop process from tmp_processes array
        Pop_Process(run_queue, cur_process.idx, &run_queue_num);

        prev_end_time = result[result_num-1].end_time;
    }

    return result_num;
}

int SJF(Scheduled_Process result[]){
    int result_num = 0;
    int prev_end_time = 0;

    // Until all processes are popped
    while(run_queue_num > 0){
        crit_result_num = 0;

        // Get closest arrival time
        Get_Processes_By_Criteria(crit_result, run_queue, run_queue_num, ARRIVAL_TIME_CRIT, LOW, -1);
        int min_arrival_time = crit_result[0].arrival_time;

        // Set startable time.
        int startable_time = (prev_end_time > min_arrival_time) ? prev_end_time : min_arrival_time;

        // Get shortest job
        // Priority : low burst_time > closest arrival time > high priority
        int crit_result_num = Get_Processes_By_Criteria(crit_result, run_queue, run_queue_num, BURST_TIME_CRIT, LOW, startable_time);
        if (crit_result_num > 1){
            crit_result_num = Get_Processes_By_Criteria(crit_result, crit_result, crit_result_num, ARRIVAL_TIME_CRIT, LOW, -1);
        }
        if (crit_result_num > 1){
            crit_result_num = Get_Processes_By_Criteria(crit_result, crit_result, crit_result_num, PRIORITY_CRIT, HIGH, -1);
        }

        // Set first received process to cur_process
        Process cur_process = crit_result[0];

        if (cur_process.arrival_time > prev_end_time){
            // If there is a time gap, Push idle process
            Scheduled_Process s_idle_process = Create_Scheduled_Idle_Process(prev_end_time, cur_process.arrival_time - prev_end_time);
            
            result[result_num++] = s_idle_process;

            prev_end_time = s_idle_process.end_time;
        }

        // Push cur process
        result[result_num++] = Create_Scheduled_Process(cur_process, prev_end_time, cur_process.burst_time);

        // Pop process from tmp_processes array
        Pop_Process(run_queue, cur_process.idx, &run_queue_num);

        prev_end_time = result[result_num-1].end_time;
    }

    return result_num;
}

int Priority(Scheduled_Process result[]){
    int result_num = 0;
    int prev_end_time = 0;

    // Until all processes are popped
    while(run_queue_num > 0){
        crit_result_num = 0;

        // Get closest arrival time
        Get_Processes_By_Criteria(crit_result, run_queue, run_queue_num, ARRIVAL_TIME_CRIT, LOW, -1);
        int min_arrival_time = crit_result[0].arrival_time;

        // Set startable time.
        int startable_time = (prev_end_time > min_arrival_time) ? prev_end_time : min_arrival_time;

        // Get highest priority job
        // Priority : high priority > closest arrival time
        int crit_result_num = Get_Processes_By_Criteria(crit_result, run_queue, run_queue_num, PRIORITY_CRIT, HIGH, startable_time);
        if (crit_result_num > 1){
            crit_result_num = Get_Processes_By_Criteria(crit_result, crit_result, crit_result_num, ARRIVAL_TIME_CRIT, LOW, -1);
        }

        // Set first received process to cur_process
        Process cur_process = crit_result[0];

        if (cur_process.arrival_time > prev_end_time){
            // If there a is time gap, Push idle process
            Scheduled_Process s_idle_process = Create_Scheduled_Idle_Process(prev_end_time, cur_process.arrival_time - prev_end_time);
            
            result[result_num++] = s_idle_process;

            prev_end_time = s_idle_process.end_time;
        }

        // Push cur process
        result[result_num++] = Create_Scheduled_Process(cur_process, prev_end_time, cur_process.burst_time);

        // Pop process from tmp_processes array
        Pop_Process(run_queue, cur_process.idx, &run_queue_num);

        prev_end_time = result[result_num-1].end_time;
    }

    return result_num;
}

int RR(Scheduled_Process result[], int time_quantum){
    int result_num = 0;
    int prev_end_time = 0;

    // Until all processes are popped
    while(run_queue_num > 0){
        crit_result_num = 0;

        // Get least arrival time process
        int crit_result_num = Get_Processes_By_Criteria(crit_result, run_queue, run_queue_num, ARRIVAL_TIME_CRIT, LOW, -1);
        if (crit_result_num > 1){
            // If more then one, get highest priority process
            crit_result_num = Get_Processes_By_Criteria(crit_result, crit_result, crit_result_num, PRIORITY_CRIT, HIGH, -1);
        }

        // Set first received process to cur_process
        Process cur_process = crit_result[0];
        
        if (cur_process.arrival_time > prev_end_time){
            // If there is a time gap, Push idle process
            Scheduled_Process s_idle_process = Create_Scheduled_Idle_Process(prev_end_time, cur_process.arrival_time - prev_end_time);

            result[result_num++] = s_idle_process;

            prev_end_time = s_idle_process.end_time;
        }

        // Set current burst time
        int burst_time = cur_process.burst_time;
        if (cur_process.burst_time > time_quantum){
            burst_time = time_quantum;
        }

        // Push scheduled process
        if (result_num > 0 && result[result_num-1].process.pid == cur_process.pid){
            result[result_num-1].end_time += burst_time;
        } else {
            result[result_num++] = Create_Scheduled_Process(cur_process, prev_end_time, burst_time);
        }

        // Modify process info (similar to pop & push)
        int p_idx = Get_Process_Idx_By_PID(run_queue, cur_process.pid, run_queue_num);
        run_queue[p_idx].arrival_time = result[result_num-1].end_time;
        run_queue[p_idx].burst_time -= time_quantum;

        if (run_queue[p_idx].burst_time < 0){
            // Pop process from tmp_processes array
            Pop_Process(run_queue, cur_process.idx, &run_queue_num);
        }

        prev_end_time = result[result_num-1].end_time;
    }

    return result_num;
}

int Preemptive_SJF(Scheduled_Process result[]){
    int result_num = 0;
    int prev_end_time = 0;
    int cur_time = 0;

    // Until all processes are popped
    while(run_queue_num > 0){
        Process cur_process;
        int burst_time = 1;
        crit_result_num = 0;

        // Get shortest job at current time
        // Priority : low burst_time > high priority > low arrival time
        crit_result_num = Get_Processes_By_Criteria(crit_result, run_queue, run_queue_num, BURST_TIME_CRIT, LOW, cur_time);
        if (crit_result_num > 1){
            crit_result_num = Get_Processes_By_Criteria(crit_result, crit_result, crit_result_num, PRIORITY_CRIT, HIGH, -1);
        }
        if (crit_result_num > 1){
            crit_result_num = Get_Processes_By_Criteria(crit_result, crit_result, crit_result_num, ARRIVAL_TIME_CRIT, LOW, -1);
        }

        if (crit_result_num < 1){
            // Get fastest arrival time
            Get_Processes_By_Criteria(crit_result, run_queue, run_queue_num, ARRIVAL_TIME_CRIT, LOW, -1);
            burst_time = crit_result[0].arrival_time - cur_time;
            // Set idle process if there is no crit_result
            cur_process = Create_Idle_Process(prev_end_time, burst_time);
        } else {
            // Set first received process
            cur_process = crit_result[0];
        }
        
        if (result_num > 0 && result[result_num-1].process.pid == cur_process.pid){
            // Modify last scheduled process
            result[result_num-1].end_time += burst_time;
        } else {
            // Push cur process
            result[result_num++] = Create_Scheduled_Process(cur_process, prev_end_time, burst_time);
        }

        // Modify pushed process burst time
        int p_idx = Get_Process_Idx_By_PID(run_queue, cur_process.pid, run_queue_num);
        if (p_idx != -1){
            run_queue[p_idx].burst_time -= burst_time;

            // Pop process from tmp_processes array
            if (run_queue[p_idx].burst_time <= 0){
                Pop_Process(run_queue, cur_process.idx, &run_queue_num);
            }
        }
        
        // Update current time
        cur_time += burst_time;

        prev_end_time = result[result_num-1].end_time;
    }

    return result_num;
}

int Preemptive_Priority(Scheduled_Process result[]){
    int result_num = 0;
    int prev_end_time = 0;
    int cur_time = 0;

    // Until all processes are popped
    while(run_queue_num > 0){
        Process cur_process;
        int burst_time = 1;
        crit_result_num = 0;

        // Get highest priority job at current time
        // Priority : high priority > low arrival time
        crit_result_num = Get_Processes_By_Criteria(crit_result, run_queue, run_queue_num, PRIORITY_CRIT, HIGH, cur_time);
        if (crit_result_num > 1){
            crit_result_num = Get_Processes_By_Criteria(crit_result, crit_result, crit_result_num, ARRIVAL_TIME_CRIT, LOW, -1);
        }

        if (crit_result_num < 1){
            // Get fastest arrival time
            Get_Processes_By_Criteria(crit_result, run_queue, run_queue_num, ARRIVAL_TIME_CRIT, LOW, -1);
            burst_time = crit_result[0].arrival_time - cur_time;
            // Set idle process if there is no crit_result
            cur_process = Create_Idle_Process(prev_end_time, burst_time);
        } else {
            // Set first received process
            cur_process = crit_result[0];
        }
        
        if (result_num > 0 && result[result_num-1].process.pid == cur_process.pid){
            // Modify last scheduled process
            result[result_num-1].end_time += burst_time;
        } else {
            // Push cur process
            result[result_num++] = Create_Scheduled_Process(cur_process, prev_end_time, burst_time);
        }

        // Modify pushed process burst time
        int p_idx = Get_Process_Idx_By_PID(run_queue, cur_process.pid, run_queue_num);
        if (p_idx != -1){
            run_queue[p_idx].burst_time -= burst_time;
            // Pop process from tmp_processes array
            if (run_queue[p_idx].burst_time <= 0){
                Pop_Process(run_queue, cur_process.idx, &run_queue_num);
            }
        }
        
        // Update current time
        cur_time += burst_time;

        prev_end_time = result[result_num-1].end_time;
    }

    return result_num;
}

Schedule_Stat Schedule(enum Algorithm algo, int time_quantum){
    Process tmp_processes[MAX_PROCESS_N];
    Schedule_Stat statistic;
    Scheduled_Process result[MAX_PROCESS_N * 5];
    int end_time[MAX_PROCESS_N];
    int turnaround_time_sum = 0, waiting_time_sum = 0;
    int result_num = 0;

    // Make temporary process array to modify
    Copy_Process_Array(processes, run_queue, process_N);
    run_queue_num = process_N;

    // Get scheduling result according to algorithm
    printf("\n < %s Scheduling > \n", algo_str[algo]);
    switch(algo){
        case FCFS_ALG:
            result_num = FCFS(result);
            break;
        case SJF_ALG:
            result_num = SJF(result);
            break;
        case PRIORITY_ALG:
            result_num = Priority(result);
            break;
        case RR_ALG:
            result_num = RR(result, time_quantum);
            break;
        case P_SJF_ALG:
            result_num = Preemptive_SJF(result);
            break;
        case P_PRIORITY_ALG:
            result_num = Preemptive_Priority(result);
            break;
        default:
            return statistic;
    }

    // Print scheduled result info
    Print_Schedule_Result(result, result_num);
    Print_Gantt_Chart(result, result_num, 5);

    // Calculate statistical info from scheduled result
    // turnaround_time = end_time - arrival_time
    // waiting_time = turnaround_time - burst_time
    for(int i=0; i<result_num; i++){
        Scheduled_Process sp = result[i];
        if (sp.process.process_type != -1){
            int process_idx = Get_Process_Idx_By_PID(processes, sp.process.pid, process_N);
            end_time[process_idx] = sp.end_time;
        }
    }

    for(int i=0; i<process_N; i++){
        if (processes[i].process_type != -1){
            turnaround_time_sum -= processes[i].arrival_time;
            waiting_time_sum -= processes[i].burst_time;
            turnaround_time_sum += end_time[i];
        }
    }
    waiting_time_sum += turnaround_time_sum;

    // Create statistic
    statistic.algorithm = algo;
    statistic.time_quantum = time_quantum;
    statistic.is_success = 1;
    statistic.avg_turnaround_time = ((float)turnaround_time_sum / process_N);
    statistic.avg_waiting_time = ((float)waiting_time_sum / process_N);

    return statistic;
}

void Evaluation(Schedule_Stat result_list[], int result_num){
    Schedule_Stat min_ATT_result, min_AWT_result;
    int min_ATT = INF, min_AWT = INF;

    Print_Schedule_Stats(result_list, result_num);

    // Get best algorithms
    for (int i=0; i<result_num; i++){
        Schedule_Stat result = result_list[i];

        if (result.avg_turnaround_time < min_ATT){
            min_ATT = result.avg_turnaround_time;
            min_ATT_result = result;
        }

        if(result.avg_waiting_time < min_AWT){
            min_AWT = result.avg_waiting_time;
            min_AWT_result = result;
        }
    }

    // Print best algorithms
    printf("Algorithm with minimum average turnaround time : %s", algo_str[min_ATT_result.algorithm]);
    if (min_ATT_result.algorithm == RR_ALG){
        printf(" with %d t.q.", min_ATT_result.time_quantum);
    }
    printf("\nAlgorithm with minimum average waiting time : %s", algo_str[min_AWT_result.algorithm]);
    if (min_AWT_result.algorithm == RR_ALG){
        printf(" with %d t.q.", min_AWT_result.time_quantum);
    }
    printf("\n");
}

int main() {
    Init();

    // User input
    do{
        printf("Input Process Number (max %d) : ", MAX_PROCESS_N);
        scanf("%d", &process_N);
    } while(process_N > MAX_PROCESS_N);

    int is_pid_manual_input = -1; // 0 for random, 1 for manual
    char input[5];
    while(is_pid_manual_input == -1 ||  strlen(input) < 1 || strlen(input) > 5){
        printf("Manually Input PID? [y/n]\n");
        scanf("%s", input);

        if (input[0] == 'y' || input[0] == 'Y'){
            is_pid_manual_input = 1;
        } else if (input[0] == 'n' || input[0] == 'N'){
            is_pid_manual_input = 0;
        }
    };


    // Create processes
    printf("=====Create Process=====\n");
    for(int i=0; i<process_N; i++){
        Process random_process = Create_Random_Process(0, is_pid_manual_input);
        Print_Process_Info(random_process);
        processes[p_top++] = random_process;
    }

    // Display created processes
    Print_Processes(processes, process_N);

    // Scheduling
    printf("=====Scheduling=====\n");
    Schedule_Stat stats[10];
    int lim = RR_ALG;
    for (int i=0; i<lim; i++){
        stats[i] = Schedule(i, 0);
    }
    // RR Scheduling with different time quantum
    stats[lim] = Schedule(RR_ALG, 5);
    stats[lim+1] = Schedule(RR_ALG, 10);
    stats[lim+2] = Schedule(RR_ALG, 15);

    // Evaluation
    printf("=====Evaluation=====\n");
    Evaluation(stats, lim + 3);
}