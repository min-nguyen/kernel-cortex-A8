#include "philosopher.h"


void main_philosopher() {
  pid_t forkpid;

  for(int i = 0; i < 15; i++){
    forkpid = fork();
    if(forkpid > 0)
      write( STDOUT_FILENO, " ", 1);
    if( 0 == forkpid ) {
      break;
    }
  }

  if(forkpid != 0){
       uint32_t* page_0x702;
       page_0x702 = ( uint32_t* )( 0x70210000 );
       *page_0x702 = 5;
   }
    // send(5,10);
    // send(5,12);
  //}
  /*
  else{
    receive(2);
    receive(2);
  }
  */
  while( 1 ) {
    write( STDOUT_FILENO, "Ph", 2 );

    uint32_t lo = 1 <<  8;
    uint32_t hi = 1 << 24;

    for( uint32_t x = lo; x < hi; x++ ) {

    }
  }
  // uint32_t* page_0x701;
  // if(forkpid != 0){
  //    write( STDOUT_FILENO, "!!", 2 );
  //    page_0x701 = ( uint32_t* )( 0x70110000 );
  //    *page_0x701 = 5;
  //  }

  exit( EXIT_SUCCESS );
}
