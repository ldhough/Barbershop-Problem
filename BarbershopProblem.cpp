#include <semaphore.h>
#include <pthread.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>
#include <string.h>
#include <functional>
#include <vector>
#include <sys/stat.h>
#include <fcntl.h>

using namespace std;

int CUSTNUM;

void* checkTimeToExit(void *arg) {
    while(true) {
        if (CUSTNUM == 0) {
            exit(0);
        }
    }
}

class Customer {
    private:
        int number;
        bool served = false;
    public:
        Customer(int n);
        static void* activateCustomerWrapper(void *arg);
        void activateCustomer(pthread_mutex_t *canAccessSeats, int *freeChairs, sem_t *waitingCustomers, pthread_mutex_t *barberIdle, int *customerNumber);
        int getCustomerNumber();
        void serve();
};

struct customerThreadArgs {
    Customer *customer;
    pthread_mutex_t *barberIdle;
    pthread_mutex_t *canAccessSeats;
    int *freeChairs;
    sem_t *waitingCustomers;
    int *customerNumber;

    customerThreadArgs(Customer *customerIn, pthread_mutex_t *barberIdleIn, pthread_mutex_t *canAccessSeatsIn, 
        int *freeChairsIn, sem_t *waitingCustomersIn, int *customerNumberIn) : customer(customerIn), barberIdle(barberIdleIn), canAccessSeats(canAccessSeatsIn), 
        freeChairs(freeChairsIn), waitingCustomers(waitingCustomersIn), customerNumber(customerNumberIn)
    {}
};

void *Customer::activateCustomerWrapper(void *arg) {
    struct customerThreadArgs *args = (struct customerThreadArgs*)arg; //typecast pointer
    args->customer->activateCustomer(args->canAccessSeats, args->freeChairs, args->waitingCustomers, args->barberIdle, args->customerNumber);
}

//activateCustomer later...

Customer::Customer(int n) {this->number = n;}

int Customer::getCustomerNumber() {return number;}

void Customer::serve() {
    served = true;
    printf("Served: %d\n", this->getCustomerNumber());
    //cout << this->getCustomerNumber() << " served!" << endl;
}

class Barber {
    private:
        string name;
    public:
        Barber(string s);
        static void* activateBarberWrapper(void *arg);
        void activateBarber(pthread_mutex_t *canAccessSeats, int *freeChairs, sem_t *waitingCustomers, pthread_mutex_t *barberIdle, int *customerNumber);
        string getName();
};

Barber::Barber(string s) {this->name = s;}
string Barber::getName() {return name;}

struct barberThreadArgs {
    Barber *barber;
    pthread_mutex_t *barberIdle;
    pthread_mutex_t *canAccessSeats;
    int *freeChairs;
    sem_t *waitingCustomers;
    int *customerNumber;
};

void *Barber::activateBarberWrapper(void *arg) {
    struct barberThreadArgs *args = (struct barberThreadArgs*)arg; //typecast pointer
    args->barber->activateBarber(args->canAccessSeats, args->freeChairs, args->waitingCustomers, args->barberIdle, args->customerNumber);
}

void Barber::activateBarber(pthread_mutex_t *canAccessSeats, int *freeChairs, sem_t *waitingCustomers, pthread_mutex_t *barberIdle, int *customerNumber) {
    int customersServed = 0;
    printf("Barber created!\n");
    //cout << "Barber " << name << " has been created!" << endl;
    while (/*customersServed < customerNumber*/true) {
        //printf("INFINITY!!!\n");
        sem_wait(waitingCustomers); //wait for customer, sleep if none available
        pthread_mutex_lock(canAccessSeats); //protect number of available seats, lock chair
        *freeChairs++;
        pthread_mutex_unlock(barberIdle); //bring customer for haircut
        pthread_mutex_unlock(canAccessSeats); //release lock on chair
        customersServed++;
        //sleep(3);
        //printf("Total left: %d\n", CUSTNUM);
    }
}

void Customer::activateCustomer(pthread_mutex_t *canAccessSeats, int *freeChairs, sem_t *waitingCustomers, pthread_mutex_t *barberIdle, int *customerNumber) {
    //cout << "Customer " << this->number << " created!" << endl;
    printf("Customer created: %d\n", this->number);
    while (served == false) { //while not served, keep trying to get a seat
        //printf("INFINITY!!!\n");
        pthread_mutex_lock(canAccessSeats); //lock seat so only one thread/person tries to sit in chair
        if (*freeChairs > 0) {
            *freeChairs--; //one less free chair in waiting on sitting down
            sem_post(waitingCustomers); //notify barber someone waiting
            pthread_mutex_unlock(canAccessSeats); //release lock on seats
            pthread_mutex_lock(barberIdle); //wait if barber busy
            //hair being cut
            this->serve();
            //*customerNumber--;
            CUSTNUM--;
            return;
        } else { //no room to sit down, come back later
            pthread_mutex_unlock(canAccessSeats);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        cout << "Usage: ./barbershop <number of customers> <number of chairs>" << endl;
        return 0;
    }

    int customerNumber = atoi(argv[1]);
    CUSTNUM = customerNumber;
    int chairNumber = atoi(argv[2]);
    
    if (chairNumber > customerNumber) {
        cout << "ERROR: There should be more customers than chairs." << endl;
        return 0;
    }

    pthread_mutex_t barberIdle; //sleeping or not
    pthread_mutex_init(&barberIdle, NULL);
    pthread_mutex_t canAccessSeats; //mutex lock for critical section of customers sitting down
    pthread_mutex_init(&canAccessSeats, NULL);
    int freeChairs = chairNumber;
    sem_t *waitingCustomers;
    const char *wC = "waitingCustomers";
    waitingCustomers = sem_open(wC, 0, 1);
    
    string barberName = "Lannie";
    Barber b(barberName);
    Barber *barber = &b;
    pthread_t barberID;
    struct barberThreadArgs barberThreadArgs;
    barberThreadArgs.barber = barber;
    barberThreadArgs.barberIdle = &barberIdle;
    barberThreadArgs.canAccessSeats = &canAccessSeats;
    barberThreadArgs.freeChairs = &freeChairs;
    barberThreadArgs.waitingCustomers = waitingCustomers;
    barberThreadArgs.customerNumber = &customerNumber;

    pthread_create(&barberID, NULL, Barber::activateBarberWrapper, (void *) &barberThreadArgs);

    vector<customerThreadArgs> cA;
    cA.reserve(customerNumber);
    unsigned idx = 0;
    for (int i = 0; i < customerNumber; i++) {
        Customer *c = new Customer(i);
        cA.push_back(customerThreadArgs(c, &barberIdle, &canAccessSeats, &freeChairs, waitingCustomers, &customerNumber));
    }

    //for (int i = 0; i < customerNumber; i++) {
    //    cout << cA[i].customer->getCustomerNumber() << endl;
    //}

    pthread_t tid[customerNumber];
    for (int i = 0; i < customerNumber; i++) {
        pthread_create(&tid[i], NULL, Customer::activateCustomerWrapper, (void *) &cA[i]);
    }

    pthread_t id;
    pthread_create(&id, NULL, checkTimeToExit, NULL);

    pthread_join(barberID, NULL);
    for (int i = 0; i < customerNumber; i++) {
        pthread_join(tid[i], NULL);
    }

}
