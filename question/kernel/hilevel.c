/* Copyright (C) 2017 Daniel Page <csdsp@bristol.ac.uk>
 *
 * Use of this source code is restricted per the CC BY-NC-ND license, a copy of
 * which can be found via http://creativecommons.org (and should be included as
 * LICENSE.txt within the associated archive or repository).
 */

#include "hilevel.h"


pcb_t procTab[MAX_PROCS];
pcb_t* executing = NULL;

extern uint32_t tos_semaphore;
sem_t (*mutexes)[30] = (void *)&tos_semaphore; //needs to be renamed to semaphores

void dispatch( ctx_t* ctx, pcb_t* prev, pcb_t* next ) {
  char prev_pid = '?', next_pid = '?';
  
  if( NULL != prev ) {
    memcpy( &prev->ctx, ctx, sizeof( ctx_t ) ); // preserve execution context of P_{prev}
     
    prev_pid = '0' + prev->pid;
  }
  if( NULL != next ) {
    memcpy( ctx, &next->ctx, sizeof( ctx_t ) ); // restore  execution context of P_{next}
    next_pid = '0' + next->pid;
  }

    PL011_putc( UART0, '[',      true );
    PL011_putc( UART0, prev_pid, true );
    PL011_putc( UART0, '-',      true );
    PL011_putc( UART0, '>',      true );
    PL011_putc( UART0, next_pid, true );
    PL011_putc( UART0, ']',      true );
    

    executing = next ;                          // update   executing process to P_{next}

  return;
}

int priority(int i){
  return procTab[i].priority + procTab[i].age;  // return the priority of a process
}

void schedule(ctx_t* ctx) { // priority based scheduler

  int maxPriority = 0; //set max priority to 0
  int nextProc = (executing->pid)-1; 
  int currentProc = (executing->pid);
  
 
  for (int i = 0; i < MAX_PROCS; i++) {
    int p = priority(i); //calculate priority of each process
    if (procTab[i].priority + procTab[i].age > maxPriority && (procTab[i].status != STATUS_TERMINATED) && ((executing->pid) != procTab[i].pid)) { // only accept ready or waiting currentProcesses
      maxPriority = p;     // update max total priority
      nextProc = i;       // update next if i is the max
    }
    if(procTab[i].status == STATUS_READY){ 
      procTab[i].age++; // increment age of every process that is ready

    }
  }

  procTab[nextProc].age = 0; // reset the age of the process to be executed to 0

  if(procTab[nextProc].status != STATUS_TERMINATED){
    dispatch( ctx,  &procTab[currentProc - 1], &procTab[ nextProc ]);  // context switch 

    if(procTab[currentProc - 1].status != STATUS_TERMINATED){
        procTab[currentProc - 1].status = STATUS_READY;             // update execution status of process 
    }         
    
    procTab[ nextProc ].status = STATUS_EXECUTING; 

   }

}
 
 
extern void     main_console();
extern uint32_t tos_console;
extern uint32_t tos_user;


void init_mutex(){
  for(int i =0; i<20;i++){
    mutexes[i]->status = 1; //sead to open
    mutexes[i]->process = 0; //no owner atm
    mutexes[i]->data = 1;
  }
}

void hilevel_handler_rst( ctx_t* ctx ) {

  PL011_putc( UART0, 'R', true );

  init_mutex();

   for(int i=0;i<MAX_PROCS;i++){  //create all processes
   
    memset( &procTab[ i ], 0, sizeof( pcb_t ) ); 
    procTab[ i ].pid      = i + 1;
    procTab[ i ].status   = STATUS_CREATED;
    procTab[ i ].ctx.cpsr = 0x50;
    procTab[ i ].ctx.pc   = ( uint32_t )( 0 );
    procTab[ i ].ctx.sp   = (uint32_t) (&tos_user) - (i*0x00001000);
    procTab[ i ].tos      = procTab[i].ctx.sp;
    procTab[ i ].priority = 0;
    procTab[ i ].age      = 0;

  }

 for(int i = 0;i<MAX_PROCS;i++){ //set all procs to terminated
  procTab[i].status = STATUS_TERMINATED;

}
 memset( &procTab[ 0 ], 0, sizeof( pcb_t ) ); // initialise console
   
 procTab[ 0 ].pid      = 1;      
 procTab[ 0 ].status   = STATUS_CREATED;
 procTab[ 0 ].ctx.cpsr = 0x50;
 procTab[ 0 ].ctx.pc   = ( uint32_t )( &main_console );
 procTab[ 0 ].ctx.sp   = ( uint32_t )( &tos_user);
 procTab[ 0 ].tos      = ( uint32_t )( &tos_user);
 procTab[ 0 ].priority = 5;
 procTab[ 0 ].age      = 0;


 TIMER0->Timer1Load  = 0x00100000; // select period = 2^20 ticks ~= 1 sec
 TIMER0->Timer1Ctrl  = 0x00000002; // select 32-bit   timer
 TIMER0->Timer1Ctrl |= 0x00000040; // select periodic timer
 TIMER0->Timer1Ctrl |= 0x00000020; // enable          timer interrupt
 TIMER0->Timer1Ctrl |= 0x00000080; // enable          timer

 GICC0->PMR          = 0x000000F0; // unmask all            interrupts
 GICD0->ISENABLER1  |= 0x00000010; // enable timer          interrupt
 GICC0->CTLR         = 0x00000001; // enable GIC interface
 GICD0->CTLR         = 0x00000001; // enable GIC distributor

 int_enable_irq();

 dispatch( ctx, NULL, &procTab[ 0 ] );

 return;
}

void hilevel_handler_irq(ctx_t* ctx) {
  // Step 2: read  the interrupt identifier so we know the source.

  uint32_t id = GICC0->IAR;

  // Step 4: handle the interrupt, then clear (or reset) the source.

  if( id == GIC_SOURCE_TIMER0 ) {
    
    schedule(ctx); TIMER0->Timer1IntClr = 0x01;
    
  }

  // Step 5: write the interrupt identifier to signal we're done.

  GICC0->EOIR = id;

  return;
}


void hilevel_handler_svc(ctx_t* ctx, uint32_t id) {

  switch(id) {

    case 0x00 : { // 0x00 => yield()
      schedule( ctx );
      break;
    }

    case 0x01 : { // 0x01 => write( fd, x, n )
      int   fd = ( int   )( ctx->gpr[ 0 ] ); //needs to be extended for pipes
      char*  x = ( char* )( ctx->gpr[ 1 ] );
      int    n = ( int   )( ctx->gpr[ 2 ] );

      for( int i = 0; i < n; i++ ) {
        PL011_putc( UART0, *x++, true );
      }

      ctx->gpr[ 0 ] = n;
      break;
    }  

    case 0x03 : { // 0x03 => fork()

      PL011_putc( UART0, 'F', true );
      PL011_putc( UART0, 'O', true );
      PL011_putc( UART0, 'R', true );
      PL011_putc( UART0, 'K', true );
      PL011_putc( UART0, ' ', true );

      int childid = 0; //set default child id to 0

      for(int i = 0; i < MAX_PROCS;i++){ //replace terminated process
        if(procTab[i].status == STATUS_TERMINATED){
          childid = i;   //if any of the processes were terminated, assign childid to that slot
          break;
        }
      }

      if(childid == 0){ //if there is no space on pcb or the id defaults to 0, don't create any more processes
        break;
      }
      
      memcpy( &procTab[childid].ctx, ctx, sizeof( ctx_t ));  //create child proc
    
      procTab[childid].pid = childid + 1;    //set new pid of child process
  
      uint32_t offset = procTab[executing->pid-1].tos - ctx->sp; //calculate offset
      procTab[childid].ctx.sp = procTab[childid].tos - offset;
      memcpy((uint32_t* ) procTab[childid].ctx.sp, (uint32_t*) ctx->sp,offset); //copy parent stack into child

      procTab[childid].priority = 6; //set child proc priority
      procTab[childid].age = 0;

      procTab[childid].ctx.gpr[0] = 0;           //return 0 to child
      ctx->gpr[0] = procTab[childid].pid;       //return pid to parent so it can be used by exec
      
      procTab[childid].status = STATUS_READY; //set it to ready to start execution

      break;
    }

    case 0x04 : { // 0x04 => exit()

      PL011_putc( UART0, 'E', true );
      PL011_putc( UART0, 'X', true );   
      PL011_putc( UART0, 'I', true );
      PL011_putc( UART0, 'T', true );
      PL011_putc( UART0, ' ', true );

      executing->status = STATUS_TERMINATED;          //end current process, reschedule
      schedule(ctx);
      break;
    }

    case 0x05 : { // 0x05 => exec()
      PL011_putc( UART0, 'E', true );
      PL011_putc( UART0, 'X', true );
      PL011_putc( UART0, 'E', true );
      PL011_putc( UART0, 'C', true );
      PL011_putc( UART0, ' ', true );
     
      if(executing->status != STATUS_TERMINATED){     //if current process is not terminated i.e. STATUS_CREATED then start executing it
        executing->status = STATUS_EXECUTING;
        ctx->pc = (uint32_t) ctx->gpr[0];
      }
      break;
    }

    case 0x06 : { // 0x06 => kill()

      PL011_putc( UART0, 'K', true );
      PL011_putc( UART0, 'I', true );
      PL011_putc( UART0, 'L', true );
      PL011_putc( UART0, 'L', true );
      PL011_putc( UART0, ' ', true );

      procTab[ctx->gpr[0]-1].status = STATUS_TERMINATED; //kill or terminate process
      break;
    }

    case 0x07 : { // 0x07 => nice()

      PL011_putc( UART0, 'N', true );
      PL011_putc( UART0, 'I', true );
      PL011_putc( UART0, 'C', true );
      PL011_putc( UART0, 'E', true );
      PL011_putc( UART0, ' ', true );

      procTab[ctx->gpr[0]-1].priority = ctx->gpr[1]; //updates priority of a process
      break;

    }

    case 0x08 : { // 0x08 => sem_read()    //rename
      int pid = ctx->gpr[0]-3;
      int data = ctx->gpr[1];
      int result = 0;
      
        if(mutexes[pid]->data == 1 || mutexes[pid]->process == 0){  //each process must check whether that data is available to them
          result = 1; 
          mutexes[pid]->process = executing->pid;   //if available,assign that data to itself
        }
        else{
          
          result = 0;  //else no data
        }
      ctx->gpr[0] = result; //return result
    
      break;
    }

    case 0x09 : { // 0x09 => sem_write()   //Essentially, here we are allowing a process to read that semaphore value
      int pid = ctx->gpr[0]-3;  //minus 3 because it would start from sem[3] rather than sem[0] 
      int data = ctx->gpr[1];
      int result = 0;

      if(mutexes[pid]->process == executing->pid){   //if process is assigned to the data slot, allow it to write
        mutexes[pid]->data = data;
        result = data;
        
      }
      else{
        result = 0;
      }


       ctx->gpr[0] = result;
      break;
    }

    case 0x10 : { //0x10 => sem_close()   //Here, we are essentially closing the semaphore && unlinking it from the process it was assigned to
   // PL011_putc( UART0, 'E', true );
      int pid = ctx->gpr[0]-3;
      int data = ctx->gpr[1];
      int result = 0;   //need to return error

      mutexes[pid]->process = 0; //unassign a process to that slot
      mutexes[pid]->data = 1;    //reset the resources
      result = 1;

       ctx->gpr[0] = result;
       break;
    }


    default : {
      break ;
    }

  }
  return;
}
