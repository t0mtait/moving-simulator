#include <pthread.h> // for pthread_create(), pthread_join(), etc.
#include <stdio.h> // for scanf(), printf(), etc.
#include <stdlib.h> // for malloc()
#include <unistd.h> // for sleep()


#define NUMBER_OF_BOXES_PER_DWELLER 5
#define ROOM_IN_TRUCK 10
#define MIN_BOX_WEIGHT 5
#define MAX_BOX_WEIGHT 50
#define MAX_TIME_FOR_HOUSE_DWELLER 7
#define MIN_TIME_FOR_HOUSE_DWELLER 1
#define MAX_TIME_FOR_MOVER 3
#define MIN_TIME_FOR_MOVER 1
#define MIN_TIME_FOR_TRUCKER 1
#define MAX_TIME_FOR_TRUCKER 3
#define MIN_TRIP_TIME 5
#define MAX_TRIP_TIME 10
#define RANDOM_WITHIN_RANGE(a,b,seed) (a+rand_r(&seed)%(b-a))
// For pipes
#define READ_END 0
#define WRITE_END 1

// Pipe between house dwellers and movers
int houseFloor[2];
// Pipe between movers and truckers
int nextToTrucks[2];

int itemsPacked = 0;
int ndwellers, nmovers, ndrivers;
int totalMoved = 0;


typedef struct _params
{
    int id;
    int nHandled;
    int nDwellers;
    int nMovers;
    int nDrivers;
    int nInTruck;
    int weightPacked;
} ThreadParams;

void* houseDweller(void* arg) {
    ThreadParams* t = (ThreadParams*) arg;
    unsigned int seed = t->id;
    printf("Hello from house dweller %d\n", t->id);
    while ((int) t->nHandled < NUMBER_OF_BOXES_PER_DWELLER) {
        int weight = RANDOM_WITHIN_RANGE(MIN_BOX_WEIGHT, MAX_BOX_WEIGHT, seed);
        int time = RANDOM_WITHIN_RANGE(MIN_TIME_FOR_HOUSE_DWELLER, MAX_TIME_FOR_HOUSE_DWELLER, seed);
        sleep(time);
        printf("House dweller %d created a box that weighs %d in %d units of time\n", t->id, weight, time);
        write(houseFloor[WRITE_END], &weight, sizeof(weight));
        t->nHandled++;
    }
    printf("House dweller %d is done packing\n", t->id);
    free(t);
    return NULL;
}

void* mover(void* arg) {
    ThreadParams* t = (ThreadParams*) arg;
    unsigned int seed = t->id;
    int weight;
    printf("Hello from mover %d\n", t->id);
    while (read(houseFloor[READ_END], &weight, sizeof(weight)) > 0) {
        int time = RANDOM_WITHIN_RANGE(MIN_TIME_FOR_MOVER, MAX_TIME_FOR_MOVER, seed);
        sleep(time);
        printf("Mover %d brought down a box that weighs %d in %d units of time \n", t->id, weight, time);
        write(nextToTrucks[WRITE_END], &weight, sizeof(weight));
        t->nHandled++;
    }
    printf("Mover %d is done moving boxes downstairs\n", t->id);
    free(t);
    return NULL;
}

void* driver(void* arg) {
    ThreadParams* t = (ThreadParams*) arg;
    unsigned int seed = t->id; 
    printf("Hello from truck driver %d\n", t->id);
    int totalBoxes = t->nDwellers * NUMBER_OF_BOXES_PER_DWELLER;
    int maxHandledPerDriver = (totalBoxes / t->nDrivers) + 1;
    int weight;
        while (read(nextToTrucks[READ_END], &weight, sizeof(weight)) > 0) 
        {
            int time = RANDOM_WITHIN_RANGE(MIN_TIME_FOR_TRUCKER, MAX_TIME_FOR_TRUCKER, seed);
            if (t->nInTruck < ROOM_IN_TRUCK) {
                t->nInTruck++;
                t->weightPacked += weight;
                printf("Trucker %d loaded up a box that weighs %d to the truck, took %d units of time, room left: %d\n",
                t->id, weight, time, ROOM_IN_TRUCK - t->nInTruck);
                t->nHandled++;  
            }

            if (t->nInTruck == ROOM_IN_TRUCK) {
                printf("Full truck with load of %d units of mass departed, round-trip will take %d\n", 
                t->weightPacked, time);
                sleep(time);
                t->weightPacked = 0; 
                t->nInTruck = 0;
            }
            totalMoved++;
            if (totalMoved >= totalBoxes) {
                if (t->weightPacked > 0) 
                {
                    int tripTime = RANDOM_WITHIN_RANGE(MIN_TRIP_TIME, MAX_TRIP_TIME, seed);
                    printf("Not full truck with load of %d units of mass departed, one way trip will take %d\n", t->weightPacked, tripTime);
                    sleep(tripTime);
                }
                printf("Trucker %d is finished\n", t->id);
                free(t);
                return NULL;
            }
        }
        return NULL;
}





int main() {
    int itemsPacked = 0;
    printf("Please input number of people living in the house, number of movers and number of truck drivers\n");
    scanf("%d %d %d", &ndwellers, &nmovers, &ndrivers);
    if(pipe(houseFloor) == -1) {
        printf("housefloor piping failed.");
        return 1;
    }
    if(pipe(nextToTrucks) == -1) {
        printf("truck piping failed.");
        return 1;
    }
    pthread_t threads[ndwellers + nmovers + ndrivers];

    for (int i = 0 ; i < ndwellers; i++) {
        ThreadParams* thr = (ThreadParams*)malloc(sizeof(ThreadParams));
        thr->id = i;    
        thr->nHandled = 0;
        pthread_create(&threads[i], NULL, houseDweller, (void*) thr);
    }
    for (int i = ndwellers ; i < nmovers+ndwellers; i++) {
        ThreadParams* thr = (ThreadParams*)malloc(sizeof(ThreadParams));
        thr->id = i - ndwellers;   
        thr->nDwellers = ndwellers; 
        thr->nMovers = nmovers;
        thr->nHandled = 0;

        pthread_create(&threads[i+ndwellers], NULL, mover, (void*) thr);
    }
    for (int i = ndwellers+nmovers ; i < ndrivers+ndwellers+nmovers; i++) {
        ThreadParams* thr = (ThreadParams*)malloc(sizeof(ThreadParams));
        thr->id = i - ndwellers -nmovers;    
        thr->nMovers = nmovers;
        thr->nDwellers = ndwellers;
        thr->nDrivers = ndrivers;
        thr->nHandled = 0;
        thr->nInTruck = 0;
        thr->weightPacked = 0;
        pthread_create(&threads[i], NULL, driver, (void*) thr);
    }
    

    for (int i = 0 ; i < (ndwellers+nmovers+ndrivers); i++) {
        pthread_join(threads[i], NULL);
    }
    printf("Moving is finished!\n");
    close(houseFloor[READ_END]);
    close(houseFloor[WRITE_END]);
    close(nextToTrucks[READ_END]);
    close(nextToTrucks[WRITE_END]);
    return 0;
}
