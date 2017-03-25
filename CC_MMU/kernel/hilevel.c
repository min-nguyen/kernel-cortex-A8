#include "hilevel.h"
#include "P3.h"

extern void     main_console();
extern uint32_t tos_Console;

uint32_t* page_0x701 = ( uint32_t* )( 0x70110000 );
uint32_t* page_0x702 = ( uint32_t* )( 0x70210000 );
uint32_t* page_0x703 = ( uint32_t* )( 0x70310000 );

bool pageframe[4096];
pcb_t pcb[PCB_SIZE], *current = NULL;
buffer_t buffers[BUFFER_SIZE];
int threads = 1, currentPCB = 0;

//Note: PID = PCB + 1
int nextFreePCB(int startPCB){
  int freePCB = startPCB + 1;
  while(pcb[freePCB].active){
    freePCB++;
  }
  return freePCB;
}



int nextActivePCB(int startPCB){
  int activePCB = startPCB + 1;
  while(!(pcb[activePCB].active) || (pcb[activePCB].waiting)){
    activePCB++;
    if(activePCB >= PCB_SIZE){
      return 0;
    }
  }
  return activePCB;
}

void hilevel_write(char* x, int n){
  for( int i = 0; i < n; i++ ) {
    PL011_putc( UART0, *x++, true );
  }
}

void scheduler( ctx_t* ctx ) {

  // switch(currentPCB){
  //   case 1:
  //     hilevel_write("pcb(1) ", 7);
  //     break;
  //   case 2:
  //     hilevel_write("pid(2) ", 7);
  //     break;
  //   case 3:
  //     hilevel_write("pid(3) ", 7);
  //     break;
    // case 5:
    //   hilevel_write("pid(5) ", 7);
    //   break;
    // case 6:
    //   hilevel_write("pid(6) ", 7);
    //   break;
    // case 7:
    //   hilevel_write("pid(7) ", 7);
    //   break;
    // case 8:
    //   hilevel_write("pid(8) ", 7);
    //   break;
    // case 9:
    //   hilevel_write("pid(9) ", 7);
    //   break;
    // case 10:
    //   hilevel_write("pid(10) ", 8);
    //   break;
    // case 11:
    //   hilevel_write("pid(11) ", 8);
    //   break;
    // case 12:
    //   hilevel_write("pid(12) ", 8);
    //   break;
    // case 13:
    //   hilevel_write("pid(13) ", 8);
    //   break;
    // case 14:
    //   hilevel_write("pid(14) ", 8);
    //   break;
    // case 15:
    //   hilevel_write("pid(15) ", 8);
    //   break;
    // case 16:
    //   hilevel_write("pid(16) ", 8);
    //   break;
  //}

  pcb[currentPCB].timer--;
  if(pcb[currentPCB].timer <= 0){
    pcb[currentPCB].timer = pcb[currentPCB].priority;
    if(threads > 1){
         int lastPCB = currentPCB;
         int nextPCB = nextActivePCB(currentPCB);

         memcpy( &pcb[lastPCB].ctx, ctx, sizeof( ctx_t ) );

         mmu_set_ptr0(pcb[nextPCB].T);
         mmu_flush();

         //Set no access for current process
         memcpy( ctx, &pcb[nextPCB].ctx, sizeof( ctx_t ) );
         current =  &pcb[nextPCB];

         currentPCB = nextPCB;
     }
  }
  return;
}

void exitCurrentPID(ctx_t* ctx ){


  pageframe[pcb[currentPCB].T[0x704] >> 20] = false;
  pageframe[pcb[currentPCB].T[0x705] >> 20] = false;
  pcb[currentPCB].active = false;
  pcb[currentPCB].waiting = false;
  pcb[currentPCB].timer = 0;
  threads--;
  scheduler(ctx);

}

void exitPID(ctx_t* ctx, int pid){

  if(pid == currentPCB + 1){
    exitCurrentPID(ctx);
    return;
  }
  int exitPCB = pid - 1;
  pageframe[pcb[exitPCB].T[0x704] >> 20] = false;
  pageframe[pcb[exitPCB].T[0x705] >> 20] = false;
  pcb[exitPCB].active = false;
  pcb[exitPCB].waiting = false;
  threads--;
  int nextPCB = nextActivePCB(currentPCB);

}

int nextFreeBuffer(int targetPID, int currentPID){
  int buffer_start_index = 0;
  //If currentPID has previously sent a value to targetPID in buffers[x] which hasn't been received yet,
  //Maintain IPC order by ensuring next sent value is in buffer index greater than x
  for(int i = 0; i < BUFFER_SIZE; i++){
    if(buffers[i].toPID == targetPID && buffers[i].fromPID == currentPID)
      buffer_start_index = i + 1;
  }
  for(int i = buffer_start_index; i < BUFFER_SIZE; i++){
    if(buffers[i].fromPID == 0)
      return i;
  }

  return 0;
}

void hilevel_send(int targetPID, int val){
  int currentPID  = currentPCB + 1;
  int targetPCB   = targetPID - 1;
  int freeBuffer  = nextFreeBuffer(targetPID, currentPID);
  buffers[freeBuffer].value   = val;
  buffers[freeBuffer].toPID   = targetPID;
  buffers[freeBuffer].fromPID = currentPID;
  pcb[targetPCB].waiting = false;
}

void hilevel_receive(int pid, ctx_t* ctx){
  int requestPCB = pid - 1;
  int currentPID = currentPCB + 1;
  //Check if requested value has been loaded
  for(int i = 0; i < BUFFER_SIZE; i++){
    if(buffers[i].fromPID == pid && buffers[i].toPID == currentPID){
      ctx->gpr[0] = buffers[i].value;
      buffers[i].value    = (int) NULL;
      buffers[i].toPID    = 0;
      buffers[i].fromPID  = 0;
      pcb[currentPCB].waiting = false;
      return;
    }
  }
  //Requested value not loaded, deschedule current process
  pcb[currentPCB].waiting = true;
  ctx->gpr[0] = (int) NULL;

  scheduler(ctx);

}

//We cannot reference physical memory at all. All addresses specified are virtual and are taken care of. Page table only contains *extra* data to take care of access protection.
// T[ i ] |= 0xC00;  //E[AP] -> Set no access protection for all
//
//4096 pages
//1 physical page is 1 MiB/1048576 Bytes in size so T[n] 0x70310000 - T[n-1] 0x70210000 = 1MiB,
//Given T[pagenumber]
//pagenumber bits 1-0,
//        10 to specify entry type as page table
//pagenumber bits 31-20 specify mapping to physical address of page frame
//pagenumber bits 8-5 control domain:
//        0001 is client domain thus access checking,
//        0011 is manager domain thus no access checking
//pagenumber bits 15,11,10 control permissions:
//        000 All Permissions : No Access
//        011 Privileged Permissions : Read/write   User Permissions : Read/write
//        001 Privileged Permissions : Read/write   User Permissions : No Access
//        010 Privileged Permissions : Read/write   User Permissions : Read Only

int nextFreePageFrame(){
  for(int i = 0x701; i < 4096; i++){
    if(pageframe[i] == false)
      return i;
  }
  return 0;
}

void fork_setup_pagetable(int pcb_num){
    //User permissions : No Access
    for( int i = 0; i < 0x700; i++){
      pcb[pcb_num].T[i] = ( ( pte_t) (i    ) << 20 ) | 0x00422;
    }
    //User permissions : Full Access (data/bss/text)
    pcb[pcb_num].T[0x700] = ( ( pte_t) (0x700) << 20 ) | 0x00C22;
    //User permissions : No Access (kernel stack)
    pcb[pcb_num].T[0x701] = ( ( pte_t) (0x701) << 20 ) | 0x00422;
    pcb[pcb_num].T[0x702] = ( ( pte_t) (0x702) << 20 ) | 0x00422;
    //User permissions : Full Access (console)
    pcb[pcb_num].T[0x703] = ( ( pte_t) (0x703) << 20 ) | 0x00C22;
    //User permissions : Full Access (Map process stack & heap to next 2 free page frames)
    int freeframe = nextFreePageFrame();
    pcb[pcb_num].T[0x704] = ( ( pte_t) (freeframe  ) << 20 ) | 0x00C22;
    pcb[pcb_num].T[0x705] = ( ( pte_t) (freeframe + 1 ) << 20 ) | 0x00C22;
    pageframe[freeframe] = true;
    pageframe[freeframe + 1] = true;
    //User permissions : Read Only (memory above program heap)
    for(int i = 0x706; i < 4096; i++){
      pcb[pcb_num].T[i] = ( ( pte_t) (i    ) << 20 ) | 0x00822;
    }
}

void hilevel_fork(ctx_t* ctx){
  int freePCB = nextFreePCB(0);
  memset(&pcb[freePCB], 0, sizeof(pcb_t));
  memcpy(&pcb[freePCB].ctx, ctx, sizeof(ctx_t));
  fork_setup_pagetable(freePCB);
  pcb[freePCB].pid = freePCB + 1;
  pcb[freePCB].ctx.sp = (uint32_t) (0x70500000 - 8);
  pcb[freePCB].ctx.gpr[0] = 0;
  pcb[freePCB].priority = pcb[freePCB].pid;
  pcb[freePCB].timer = pcb[freePCB].priority;
  pcb[freePCB].active = true;
  pcb[freePCB].waiting = false;
  ctx->gpr[0] = pcb[freePCB].pid;
  threads++;
  hilevel_write("fork ",5);
}


void hilevel_handler_rst( ctx_t* ctx              ) {

  TIMER0->Timer1Load  = 0x00010000; // select period = 2^20 ticks ~= 1 sec
  TIMER0->Timer1Ctrl  = 0x00000002; // select 32-bit   timer
  TIMER0->Timer1Ctrl |= 0x00000040; // select periodic timer
  TIMER0->Timer1Ctrl |= 0x00000020; // enable          timer interrupt
  TIMER0->Timer1Ctrl |= 0x00000080; // enable          timer

  GICC0->PMR          = 0x000000F0; // unmask all            interrupts
  GICD0->ISENABLER1  |= 0x00000010; // enable timer          interrupt
  GICC0->CTLR         = 0x00000001; // enable GIC interface
  GICD0->CTLR         = 0x00000001; // enable GIC distributor

  //Console Stack directly maps to 0x703 -> (0x70400000 - 8)
  pcb[ 0 ].pid      = 1;
  pcb[ 0 ].ctx.cpsr = 0x50;
  pcb[ 0 ].ctx.pc   = ( uint32_t )( &main_console );
  pcb[ 0 ].ctx.sp   = ( uint32_t ) (0x70400000 - 8);
  pcb[ 0 ].priority = 1;
  pcb[ 0 ].timer    = 1;
  pcb[ 0 ].waiting  = false;
  pcb[ 0 ].active   = true;

  current = &pcb[ 0 ];
  memcpy( ctx, &current->ctx, sizeof( ctx_t ) );

  //Initialise buffers
  for(int n = 0; n < 50; n++){
    buffers[n].fromPID = 0;
    buffers[n].toPID   = 0;
    buffers[n].value   = (int) NULL;
  }

  //Initialise page frame allocation tracker up to top of console's memory 0x703
  for( int i = 0; i < 0x704; i++){
    pageframe[i] = true;
  }
  for( int i = 0x704; i < 4096; i++){
    pageframe[i] = false;
  }

  //Initialise Console page table
  for( int i = 0; i < 4096; i++ ) {
    pcb[0].T[i] = ( ( pte_t) (i    ) << 20 ) | 0x00C22;
  }
  // User Permission : No Access (kernel protection)
  pcb[0].T[0x701] = ( ( pte_t) (0x701) << 20 ) | 0x00422;
  pcb[0].T[0x702] = ( ( pte_t) (0x702) << 20 ) | 0x00422;


  // MMU configure
  mmu_set_ptr0( pcb[0].T );
  mmu_set_dom( 0, 0x3 ); // set domain 0 to 11_{(2)} => manager (i.e., not checked)
  mmu_set_dom( 1, 0x1 ); // set domain 1 to 01_{(2)} => client  (i.e.,     checked)
  mmu_enable();
  int_enable_irq();

  hilevel_write("MMU Enabled", 11);

  return;
}


void hilevel_handler_irq(ctx_t* ctx       ) {
  uint32_t id = GICC0->IAR;
  // Step 4: handle the interrupt, then clear (or reset) the source.
  if( id == GIC_SOURCE_TIMER0 ) {
    //PL011_putc( UART0, 'T', true );
    scheduler(ctx);
    TIMER0->Timer1IntClr = 0x01;
  }
  // Step 5: write the interrupt identifier to signal we're done.
  GICC0->EOIR = id;
  return;
}

void hilevel_handler_pab(ctx_t* ctx) {
  hilevel_write("PAB",3);
  exitCurrentPID(ctx);
  return;
}

void hilevel_handler_dab(ctx_t* ctx) {
  hilevel_write("DAB",3);
  exitCurrentPID(ctx);
  return;
}

void hilevel_handler_svc( ctx_t* ctx, uint32_t id ) {

  switch( id ) {
    case 0x00 : { // 0x00 => yield()
      scheduler( ctx );
      break;
    }
    case 0x01 : { // 0x01 => write( fd, x, n )
      int   fd = ( int   )( ctx->gpr[ 0 ] );
      char*  x = ( char* )( ctx->gpr[ 1 ] );
      int    n = ( int   )( ctx->gpr[ 2 ] );

      for( int i = 0; i < n; i++ ) {
        PL011_putc( UART0, *x++, true );
      }
      ctx->gpr[ 0 ] = n;
      break;
    }
    case 0x03 : { // 0x03 => fork()
      hilevel_fork(ctx);
      break;
    }
    case 0x04 : { //0x04 => exit()
      //int exitPID = ctx->gpr[0]; don't need
      exitCurrentPID(ctx);
      break;
    }
    case 0x05 : { // 0x05 => exec()
      ctx->pc = ( uint32_t ) (ctx->gpr[ 0 ]);
      break;
    }
    case 0x06 : { // 0x06 => kill()
      int exit_PID = ctx->gpr[0];
      exitPID(ctx, exit_PID);
      break;
    }
    case 0x07 : { // 0x07 => send()
      int targetPID = ctx->gpr[0];
      int val = ctx->gpr[1];
      hilevel_send(targetPID, val);
      break;
    }
    case 0x08 : { // 0x08 => receive()
      int reqPID = ctx->gpr[0];
      hilevel_receive(reqPID, ctx);
      break;
    }

    default   : { // 0x?? => unknown/unsupported
      break;
    }
  }

  return;
}
