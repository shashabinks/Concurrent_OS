
// POSIX UNAMED SEMAPHORE API //
// USES SYNCHRONIZATION IN ORDER TO ACHIEVE IPC//
#include "philosopher.h" 

#define NUM_PHILOSOPHERS 16

int fork_lock = 1;  //mutex lock

bool free_forks(int left, int right){
    if(left == 1 && right == 1){
        return true;
    }
    else{
        return false;
    }

}

void sleep( int x ){  //sleep function
    for ( int i = 1 ; i <= 468951; i++ ){
        for ( int j = 1 ; j <= x ; j++ ){
            asm ( "nop" );
        }
    }
}

void main_philosopher(){
    char phil[2];   
    int p_id;
    //bool hasEaten = false;
    
    for(int i=0; i < NUM_PHILOSOPHERS; i++){

        p_id = i;  //assign each philosopher an id

        if(fork() == 0){  //if child
               
            itoa(phil,p_id+1); //produce string

            while(true){ //runs infinite
   
                sleep(950); // may have to adjust this so that it prints/works correctly depending on pc but it is tuned to work for 2.11 machines
                //change to 900 if it doesn't run/see [x->y] but no actual operation taking place
                // for lab machines : 1450
                
                write( STDOUT_FILENO, "P", 1);
                write( STDOUT_FILENO, phil, 2);
                write( STDOUT_FILENO, " is thinking\n", 13 );

                
                sem_wait(&fork_lock);

                int left = p_id;    //pick up left fork
                int right = (p_id+1)%16;  //pick up right fork

                int forkl = sem_read(left+3,1);  //read
                int forkr = sem_read(right+3,1);

                
                if(free_forks(forkl,forkr)){ //check if either forks are available, if not, the value of each fork is 0
                
                   sem_write(left+3,0); //write
                   sem_write(right+3,0);
                   
                    write( STDOUT_FILENO, "P", 1);
                    write( STDOUT_FILENO, phil, 2);
                    write( STDOUT_FILENO, " picked up the forks  \n", 23); 

                    sem_post(&fork_lock); //release lock,allow other processes to continue 
                    
                }else{
                    sem_post(&fork_lock); //release lock
                    continue; //don't go further
                }

                write( STDOUT_FILENO, "P", 1);
                write( STDOUT_FILENO, phil, 2);
                write( STDOUT_FILENO, " is eating\n", 11 );
                sleep(850); //sleep for a while after philosopher has started eating
                //change to 800 if no operation or less
                //for lab machines: 1350
                
                write( STDOUT_FILENO, "P", 1); //resume
                write( STDOUT_FILENO, phil, 2);
                write( STDOUT_FILENO, " has finished eating\n", 22 );

                sem_wait(&fork_lock); //place lock to prevent other philosophers placing their forks down at the same time

                forkl = sem_read(left+3,0);  //read the fork
                forkr = sem_read(right+3,0);

                if(forkl == 0 && forkr == 0){ //place forks down
                    sem_write(left+3,1);
                    sem_write(right+3,1);
                    
                }

                sem_close(left+3); //close & unlink semaphore values
                sem_close(right+3);

                //release lock
                sem_post(&fork_lock); // allow other processes to resume
                write( STDOUT_FILENO, "P", 1);
                write( STDOUT_FILENO, phil, 2);
                write( STDOUT_FILENO, " put down both forks\n", 22 );

               
               
            }
        }      
    }
    exit(EXIT_SUCCESS);

}