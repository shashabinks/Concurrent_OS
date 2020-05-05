

// USES SYNCHRONIZATION IN ORDER TO ACHIEVE IPC//
#include "philosopher.h" 

#define NUM_PHILOSOPHERS 16

bool free_forks(int left, int right){  //check whether forks are available to that process
    if(left == 0 && right == 0){
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

                
                //sem_wait(&fork_lock);

                int left = p_id;    //pick up left fork
                int right = (p_id+1)%16;  //pick up right fork

                int forkl = sem_open(left+3,1);  //initialise semaphore value, link with processid
                int forkr = sem_open(right+3,1);

                
                if(free_forks(forkl,forkr)){ //check if either forks are available, if not, the value of each fork is 0      
                    sem_wait(left+3); //decrement semaphore value, or the fork in this case
                    sem_wait(right+3);
                   
                    write( STDOUT_FILENO, "P", 1);
                    write( STDOUT_FILENO, phil, 2);
                    write( STDOUT_FILENO, " picked up the forks  \n", 23); 
              
                }else{
                    
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

              
                sem_post(left+3,1); // release forks or increment semaphore value
                sem_post(right+3,1);
                    

                sem_close(left+3); //close & unlink semaphore values
                sem_close(right+3);
               
                write( STDOUT_FILENO, "P", 1);
                write( STDOUT_FILENO, phil, 2);
                write( STDOUT_FILENO, " put down both forks\n", 22 );
       
            }
        }      
    }
    exit(EXIT_SUCCESS);

}