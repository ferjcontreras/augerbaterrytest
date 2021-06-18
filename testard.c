#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>

#define TERM_ARDUINO "/dev/ttyUSB0"

int open_serial(char *TerminalPort,int mode, int baudios, int multimeter) {
      /*open and set the serial port.

        TerminalPort: would be the device associated with the serial port
        .             something like "/dev/ttyS0"

        mode=1: for READONLY mode
        .    2: for WRITE only mode
        .    others values: for ready and write mode

        return: file descriptor or -1 in case of error.
      */
      int fd,fd_mode,baud;
      struct termios oldtio,newtio;
      int dsrv;

      baud = baudios; //baud=B9600; /* it is the normal value for LEEDS radios. */
      if (mode==1){
          fd_mode=O_RDONLY;
      } else if(mode==2){
          fd_mode=O_WRONLY;
      } else {
          fd_mode=O_RDWR;
      }
      fd = open(TerminalPort, fd_mode | O_NOCTTY | O_NONBLOCK);
      if (fd < 0) {
          printf("Not possible to open the termial %s\n",TerminalPort);
          //error_message("No es posible abrir el puerto serie");
          return -1;
      }
      /* set the serial port parameter */

      //fcntl(fd, F_SETFL, 0);
      bzero(&newtio, sizeof(newtio));
      if (multimeter == 1) {
        newtio.c_cflag = baud  | CS7 | CLOCAL | CREAD;
      }
      else {
        newtio.c_cflag = baud  | CS8 | CLOCAL | CREAD;
      }

      newtio.c_iflag = IGNPAR;
      newtio.c_oflag = 0;
      newtio.c_lflag = 0;

      newtio.c_cc[VTIME]    = 0;
      newtio.c_cc[VMIN]     = 0;
      tcflush(fd, TCIFLUSH);
      tcsetattr(fd,TCSANOW,&newtio);


      //put the pin 7 in -9V - needed because the multimeter generate
      // signal according the voltage of this pin (not really clear how it works).
      if (multimeter == 1) {
        dsrv=TIOCM_RTS;
        ioctl(fd, TIOCMBIC, &dsrv);
      }

      return fd;
}

int main () {
  char * command;
  int fd;
  int i,j;
  char c, test;
  unsigned char v=0xff;
  int which=1;
  char buff[10];
  int nr;

  command = (char *)malloc(3 * sizeof(char));
  for (i=0; i < 8; i++) {
    c = 'E';

    //v = v | (1 << i); // enciendo baterias
    //printf("v: %2x\n", v);

    v = v & (~(1<<i)); // apagar baterias
    test = ~(1<<i);
    printf("%c %02x %02x : %i\n", c, v, test, i);

    sprintf(command, "%c%02x", c, v);
    fd = open_serial(TERM_ARDUINO,3, B9600, 0);
    if (fd != -1) {
      printf("written: %d\n",write(fd,command,3));

      sleep(1);
      nr=read(fd,buff,256);
      if(nr>0){
        for(j=0;j<nr;j++){
          printf("%d %02x %c\n",j,buff[j],
                 (32<=buff[j] && buff[j]<127)?buff[j]:'.');
        }
      }
      close(fd);
    }
    else {
      printf("Imposible abrir el puerto\n");
    }
    sleep(1);
  }
}
