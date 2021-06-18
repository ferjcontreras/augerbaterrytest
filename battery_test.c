#include <gtk/gtk.h>
#include <gtk/gtkx.h>
#include <signal.h>

#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <time.h>

#define MAX_BATTERIES 8

// Main Window
GtkWidget *window;
GtkWidget *serial[MAX_BATTERIES];
GtkWidget *volt[MAX_BATTERIES];
GtkWidget *curr[MAX_BATTERIES];
GtkWidget *msg[MAX_BATTERIES];
GtkWidget *minvolt;
GtkWidget *status;
GtkBuilder *builder;


// Parametros archivo configuración
double ver1[MAX_BATTERIES];
double icurr;
char term_multi[20];
char term_arduino[20];
char location[100];
int delay;
char pathconfigfile[100];
////////////////////////////

time_t seconds[MAX_BATTERIES];
int samples[MAX_BATTERIES];
unsigned char available;
FILE * files[MAX_BATTERIES];
int qty;
int excluded[MAX_BATTERIES];
float lastcurrent[MAX_BATTERIES];
float lastvoltage[MAX_BATTERIES];
pthread_t inc_x_thread;


void logMessage(char *  message) {
  FILE * f;
  time_t rawtime;
  char tiempo[25];
  char logfile[100];
  rawtime = time(NULL);
  struct tm *ptm = localtime(&rawtime);

  sprintf(logfile, "%s/log/test_%i_%i_%i.log", location, ptm->tm_year+1900, ptm->tm_mon+1, ptm->tm_mday);
  strcpy(tiempo, ctime(&rawtime));
  tiempo[24] = '\0';

  f = fopen(logfile, "a");

  fprintf(f, "%s: %s\n",tiempo, message);
  fflush(f);
  fclose(f);
}


// Display Error Message
void error_message (const gchar *message)
{
        GtkWidget               *dialog;

        /* log to terminal window */
        g_warning ("%s", message);

        /* create an error message dialog and display modally to the user */
        dialog = gtk_message_dialog_new (NULL,
                                         GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_MESSAGE_ERROR,
                                         GTK_BUTTONS_OK,
					 "%s", message);

        gtk_window_set_title (GTK_WINDOW (dialog), "Error!");
        gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);
}


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



void SendCommandArduino(char c, char which) {
	char * command;
  int fd, nr, j;
  char buff[10];
	command = (char *)malloc(3 * sizeof(char));
	sprintf(command, "%c%02x", c, 1 << which);
  fd = open_serial(term_arduino,3, B9600, 0);
  printf("written: %d\n",write(fd,command,3));
  //sleep(0.5);
  nr=read(fd,buff,256);
  if(nr>0){
    for(j=0;j<nr;j++){
      printf("%d %02x %c\n",j,buff[j],
             (32<=buff[j] && buff[j]<127)?buff[j]:'.');
    }
  }
  close(fd);
}

double ReadMultimeterValue() {
  float voltage = 0.0;
  float temp = 0.0;
  int fd,nr,i,j=0;
  double numb;
  char buff[256], str[10];

  fd = open_serial(term_multi, 3, B9600, 1);
  //sleep(1);
  buff[0]='d';
  buff[1]='\0';
  printf("writte: %d\n",write(fd,buff,1));
  sleep(2);
  nr=read(fd,buff,256);
  if(nr>0){
    for(i=0;i<nr;i++){
      //printf("%d %02x %c\n",i,buff[i],
        //     (32<=buff[i] && buff[i]<127)?buff[i]:'.');
      if (i>=46 && i<=51) {
        str[j] = buff[i];
        j++;
      }
      str[j] = '\0';
    }
  }
  numb = atof(str);
  //printf("Voltaje %f\n", numb);
  return numb;
}

double GetCurrent(char which) {
  double value;
  char svalue[10];

  SendCommandArduino('I', which);
  sleep(5);
	value = ReadMultimeterValue();
  value = value / icurr; // pass to current



  sprintf(svalue, "%f A", value);
  gtk_label_set_text(GTK_LABEL(curr[which]), svalue);

  return value;
}
void ApagarBateria(int which) {
  char * command;
  char c = 'E';
  int fd, nr, j;
  char buff[10];
  available = available & (~(1<<which));
	command = (char *)malloc(3 * sizeof(char));
	sprintf(command, "%c%02x", c, available);
  fd = open_serial(term_arduino,3, B9600, 0);
  printf("written: %d\n",write(fd,command,3));
  //sleep(0.5);
  nr=read(fd,buff,256);
  if(nr>0){
    for(j=0;j<nr;j++){
      printf("%d %02x %c\n",j,buff[j],
             (32<=buff[j] && buff[j]<127)?buff[j]:'.');
    }
  }
  close(fd);
}


double GetVoltage(int which) {
  double value;
  char svalue[10];

  SendCommandArduino('V', which);
  sleep(5);
	value = ReadMultimeterValue();
  value = value * ver1[which]; // make correction

  sprintf(svalue, "%f V", value);
  gtk_label_set_text(GTK_LABEL(volt[which]), svalue);

  return value;

}


// This function switch all the batteries on
void EncenderBaterias() {
  char * command;
  char c = 'E';
  int fd, nr, j;
  char buff[10];
  //available = 0xff;
	command = (char *)malloc(3 * sizeof(char));
	sprintf(command, "%c%02x", c, available);
  fd = open_serial(term_arduino,3, B9600, 0);
  printf("written: %d\n",write(fd,command,3));

  nr=read(fd,buff,256);
  if(nr>0){
    for(j=0;j<nr;j++){
      printf("%d %02x %c\n",j,buff[j],
             (32<=buff[j] && buff[j]<127)?buff[j]:'.');
    }
  }
  close(fd);
}

void initConfig() {
  int i;
  char c;
  int totalBatteries;
  char name[100];
  time_t rawtime;
  available = 0x00;
  totalBatteries = 0;
  for (i=0; i<MAX_BATTERIES;i++){
    if (excluded[i] == 0) {
      rawtime = time(NULL);
      struct tm *ptm = localtime(&rawtime);
      sprintf(name, "%s/files/%i_%i_%i_%s.txt", location, ptm->tm_year+1900, ptm->tm_mon+1, ptm->tm_mday, gtk_entry_get_text(GTK_ENTRY(serial[i])));
      files[i] = fopen(name, "w");
      sprintf(name, "Battery %i: %s", i+1, gtk_entry_get_text(GTK_ENTRY(serial[i])));
      logMessage(name);
      seconds[i] = time(NULL);
      samples[i] = 0;
      available = available | (1 << i); // enciendo baterias
      gtk_label_set_text(GTK_LABEL(volt[i]), "0.00");
      gtk_label_set_text(GTK_LABEL(curr[i]), "0.00");
      totalBatteries++;
    }
  }
  //available = 0xff;
  EncenderBaterias();
  qty = totalBatteries;
}

void UpdateRunningStatus() {
  FILE * running;
  char runningname[100];
  int i;

  sprintf(runningname, "%s/.running", location);
  running = fopen(runningname, "w");
  for (i = 0; i < MAX_BATTERIES; i++) {
    if (strcmp(gtk_entry_get_text(GTK_ENTRY(serial[i])),"") != 0) {
      if (excluded[i] == 0) fprintf(running, "%i;%s;%f;%f;R\n", samples[i], gtk_entry_get_text(GTK_ENTRY(serial[i])), lastvoltage[i], lastcurrent[i]);
      else fprintf(running, "%i;%s;%f;%f;S\n", samples[i], gtk_entry_get_text(GTK_ENTRY(serial[i])), lastvoltage[i], lastcurrent[i]);
      fflush(running);
    }
  }
  fclose(running);
}

void StartTest() {
	int i, diff, salir=0;
  time_t now;
  char message[20];
  char result[10];
  double voltage;
  double current;
  int sec;

  logMessage("--- Starting Test ---");
  initConfig();
  gtk_label_set_text(GTK_LABEL(status), "Starting Test...");



  while(1) {



    for (i = 0; i < MAX_BATTERIES; i++) {

      if (qty == 0) salir = 1;
      if (excluded[i] == 0) {



        now = time(NULL);
        int diff = now - seconds[i];
        if (diff > 60 ) {
          diff = diff / 60; // pasamos a minutos
          sprintf(result, "%i min", diff);
        }
        else sprintf(result, "%i sec", diff);
        sprintf(message, "Total time: %s - Testing battery %i...", result, i+1);
        gtk_label_set_text(GTK_LABEL(status), message);
        gtk_label_set_markup (GTK_LABEL (msg[i]), "<b><span color='green'>Testing...</span></b>");


        voltage = 0.0;
    		voltage = GetVoltage(i);

        sleep(0.5);

        current = 0.0;
        current = GetCurrent(i);
        fprintf(files[i], "%i\t%f\t%f\n",time(NULL), voltage, current);
        fflush(files[i]);
        samples[i] = samples[i] + 1;

        lastvoltage[i] = voltage;
        lastcurrent[i] = current;

        if (voltage < atoi(gtk_entry_get_text(GTK_ENTRY(minvolt)))) {
          excluded[i] = 1;
          qty--;
          ApagarBateria(i);

          sprintf(message, "<b><span color='red'>Finished at %s (%i)</span></b>", result, samples[i]);
          gtk_label_set_markup (GTK_LABEL (msg[i]), message);

          sprintf(message, "Battery %i stopped...", i+1);
          gtk_label_set_text(GTK_LABEL(status), message);
          logMessage(message);

        } else gtk_label_set_text(GTK_LABEL(msg[i]), "");
      }

  	}

    UpdateRunningStatus();
    if (salir == 1) break;
    sec=delay;
    while (sec != 0) {
      sprintf(message, "Waiting %i seconds...", sec);
      gtk_label_set_text(GTK_LABEL(status), message);
      sec--;
      sleep(1);
    }
    //sleep(delay);
  }
  gtk_label_set_text(GTK_LABEL(status), "Test Finished!");
  logMessage("--- Test Finished ---");
}


/* Just used for start a thread*/
void *inc_x(void *x_void_ptr) {

  StartTest();
  return NULL;
}


void CheckAndStart() {
	// First we check that everything is complete for the test...
	// In case of not, we just show an error
  int i;
  for (i=0; i<MAX_BATTERIES; i++) {
    gtk_label_set_text(GTK_LABEL(msg[i]),"");
    gtk_label_set_text(GTK_LABEL(volt[i]), "0.00");
    gtk_label_set_text(GTK_LABEL(curr[i]), "0.00");
    samples[i]=0;
    if (strcmp(gtk_entry_get_text(GTK_ENTRY(serial[i])),"") == 0){// || strcmp(gtk_entry_get_text(GTK_ENTRY(minvolt)),"") == 0) {
      excluded[i] = 1;
    }
    else {
      excluded[i] = 0;
    }
  }
  if(pthread_create(&inc_x_thread, NULL, inc_x, NULL)) {
    error_message("Error creating thread");
    return;
  }
		//StartTest();
}



void LoadConfigFromFile() {
  char line[256];
  int linenum=0;
  FILE * archivo;

  archivo = fopen(pathconfigfile, "r");
  if (archivo == NULL) printf("Cannot read the file!\n");

  while(fgets(line, 256, archivo) != NULL) {
          char valor[50], clave[50];

          linenum++;
          if(line[0] == '#') continue;

          if(sscanf(line, "%s %s", clave, valor) != 2)
          {
                  fprintf(stderr, "Syntax error, line %d\n", linenum);
                  continue;
          }

          if(strcmp(clave, "TERM_ARDUINO") == 0) {
            strcpy(term_arduino, valor);
          }
          else if(strcmp(clave, "TERM_MULTI") == 0) {
            strcpy(term_multi, valor);
          }
          else if (strcmp(clave, "ICURR") == 0) {
            icurr = atof(valor);
          }
          else if (strcmp(clave, "DELAY") == 0) {
            delay = atoi(valor);
          }
          else if (strcmp(clave, "LOCATION") == 0) {
            strcpy(location, valor);
          }
          else if (strcmp(clave, "VER1") == 0) {
            ver1[0] = atof(valor);
          }
          else if (strcmp(clave, "VER2") == 0) {
            ver1[1] = atof(valor);
          }
          else if (strcmp(clave, "VER3") == 0) {
            ver1[2] = atof(valor);
          }
          else if (strcmp(clave, "VER4") == 0) {
            ver1[3] = atof(valor);
          }
          else if (strcmp(clave, "VER5") == 0) {
            ver1[4] = atof(valor);
          }
          else if (strcmp(clave, "VER6") == 0) {
            ver1[5] = atof(valor);
          }
          else if (strcmp(clave, "VER7") == 0) {
            ver1[6] = atof(valor);
          }
          else if (strcmp(clave, "VER8") == 0) {
            ver1[7] = atof(valor);
          }

  }
}

/*Display the usage*/
void PrintUsage() {
        printf("Usage:\n");
        printf("-----\n");
        printf("        battery <options>\n");
        printf("Options:\n");
        printf("        -c : paramenters file\n");
}

/*Display the help for this application*/
void ShowHelp() {
        printf("-----------------------------------------------------------------------\n");
        printf("-            Testeo de Baterías  -   Version 2.0                      -\n");
        printf("-----------------------------------------------------------------------\n");
        printf("Author: Fernando Contreras <fcontreras@auger.org.ar>                   \n");
        printf("24/Oct/2019                                                            \n");
        printf("\n");
        PrintUsage();
}

int main (int argc, char *argv[]) {
  int i;
  char c;

	gtk_init(&argc, &argv);

  while ((c = getopt (argc, argv, "c:h")) != -1) {
          switch (c) {
                  case 'c': // Configuration Files
                          strcpy(pathconfigfile, optarg);
                          break;
                  case 'h':
                          ShowHelp();
                          return 0;
                  case '?':
                          break;
                  default:
                          printf("unknow option: -%c %s\n", c, optarg);
                          return 1;
          }
  }


	builder = gtk_builder_new_from_file("layout.glade");


	window = GTK_WIDGET(gtk_builder_get_object(builder, "window"));
	g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

  LoadConfigFromFile();

  for (i=0; i<MAX_BATTERIES;i++) {
    char widgetname[10];
    sprintf(widgetname, "serial%i",i+1);
    serial[i] = GTK_WIDGET(gtk_builder_get_object(builder, widgetname));

    sprintf(widgetname, "volt%i",i+1);
    volt[i] = GTK_WIDGET(gtk_builder_get_object(builder, widgetname));
    gtk_label_set_text(GTK_LABEL(volt[i]), "0.00");

    sprintf(widgetname, "curr%i",i+1);
    curr[i] = GTK_WIDGET(gtk_builder_get_object(builder, widgetname));
    gtk_label_set_text(GTK_LABEL(curr[i]), "0.00");

    sprintf(widgetname, "msg%i",i+1);
    msg[i] = GTK_WIDGET(gtk_builder_get_object(builder, widgetname));
    gtk_label_set_text(GTK_LABEL(msg[i]), "");
  }

	minvolt = GTK_WIDGET(gtk_builder_get_object(builder, "voltmin"));
  status = GTK_WIDGET(gtk_builder_get_object(builder, "status"));
  gtk_label_set_text(GTK_LABEL(status), "");

	gtk_builder_connect_signals(builder, NULL);

	gtk_widget_show(window);

	gtk_main();

	return 0;
}
void on_btnComenzar_clicked(GtkButton *b) {
	CheckAndStart();
}

void on_btnCancelar_clicked(GtkButton *b) {
  int i;
  for (i=0; i <MAX_BATTERIES; i++) {
    gtk_label_set_text(GTK_LABEL(msg[i]),"");
    gtk_label_set_text(GTK_LABEL(volt[i]), "0.00");
    gtk_label_set_text(GTK_LABEL(curr[i]), "0.00");
    excluded[i] = 1;
  }
  gtk_label_set_text(GTK_LABEL(status), "Test cancelled");
  pthread_cancel(inc_x_thread);
  UpdateRunningStatus();

  available = 0x00;
  EncenderBaterias(); // en realidad las estamos apagando porque le estamos enviando available = 0x00

  logMessage("--- Test Cancelled by User ---");
}
