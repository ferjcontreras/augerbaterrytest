#include <termios.h>
#include <fcntl.h>
#include <unistd.h>

#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <math.h>

int open_serial(char *TerminalPort,int mode)
{

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

  baud=B9600; /* it is the normal value for LEEDS radios. */
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
    return -1;
  }
  /* set the serial port parameter */
  bzero(&newtio, sizeof(newtio));
  newtio.c_cflag = baud  | CS7 | CLOCAL | CREAD;
  newtio.c_iflag = IGNPAR;
  newtio.c_oflag = 0;
  newtio.c_lflag = 0;

  newtio.c_cc[VTIME]    = 0;
  newtio.c_cc[VMIN]     = 0;
  tcflush(fd, TCIFLUSH);
  tcsetattr(fd,TCSANOW,&newtio);


  //put the pin 7 in -9V - needed because the multimeter generate
  // signal according the voltage of this pin (not really clear how it works).
  dsrv=TIOCM_RTS;
  ioctl(fd, TIOCMBIC, &dsrv);



  return fd;
}

int open_fifo(char *FIFO_port,int mode)
{

  /*open and set the serial port.

    mode=1: for READONLY mode
    .    2: for WRITE only mode
    .    others values: for ready and write mode

    return: file descriptor or -1 in case of error.
  */

  int fd,fd_mode;

  if (mode==1){
    fd_mode=O_RDONLY;
  } else if(mode==2){
    fd_mode=O_WRONLY;
  }
  fd = open(FIFO_port, fd_mode );//| O_NONBLOCK);
  if (fd < 0) {
    printf("Not possible to open the fifo %d\n",FIFO_port);
    return -1;
  }
  return fd;
}



int main()
{
  float voltage = 0.0;
  float temp = 0.0;
  int fd,nr,i,j=0;
  double numb;
  char buff[256], str[10];
  fd=open_serial("/dev/ttyUSB0",3);
  sleep(1);
  buff[0]='d';
  buff[1]='\0';
  printf("writte: %d\n",write(fd,buff,1));
  sleep(1);
  nr=read(fd,buff,256);
  if(nr>0){
    for(i=0;i<nr;i++){
      printf("%d %02x %c\n",i,buff[i],
             (32<=buff[i] && buff[i]<127)?buff[i]:'.');

      if (i>=4 && i<=9) {
        str[j] = buff[i];
        j++;
      }
      str[j] = '\0';
      //printf("%f - exponente: %i\n", temp, 5-i);
    }
  }

  numb = atof(str);
  float numb2 = 23.5;
  //sscanf(str, "%f", &numb);
  printf("nr: %d - volt: %s - voltf: %f - numb2: %f\n",nr, str, numb,numb2);
}

/*

rsato@ricardo:/mnt/home/rsato/SOC/BatTest/teste$ a.out
writte: 1
0 7e ~
1 41 A
2 43 C
3 20
4 31 1
5 2e .
6 32 2
7 31 1
8 36 6
9 39 9
10 20
11 20
12 20
13 56 V
14 0d .
15 20
16 20
17 20
18 20
19 20
20 20
21 20
22 20
23 20
24 20
25 20
26 20
27 20
28 0d .
29 46 F
30 52 R
31 20
32 30 0
33 30 0
34 2e .
35 30 0
36 35 5
37 30 0
38 20
39 6b k
40 48 H
41 7a z
42 0d .
43 64 d
44 42 B
45 20
46 30 0
nr: 47

*/
