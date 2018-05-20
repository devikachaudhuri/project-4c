//Devika Chaudhuri
//devika.chaudhuri@gmail.com
//304597339

#include <stdio.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <mraa/gpio.h>
#include <mraa/aio.h>
#include <math.h>
#include <poll.h>
#include <sys/socket.h>
#include <netdb.h>

int logon = 0;
int logfd;
int temp = 0;//default F
char *logfile;
sig_atomic_t volatile run_flag = 1;
uint16_t value;
const int B = 4275; // B value of thermistor
const int Ro = 100000;
int amount = 1;
int reports = 1;
int id_num = 0;
char *host_name = NULL;
int port_num = 0;
int sockfd;

int Open(char * pathname, int flags, mode_t mode){
  int fd = open(pathname, flags, mode);
  if (fd == -1){
    fprintf(stderr, "Open error:%s\n", strerror(errno));
  }
  return fd;
}

void Close(int fd){
  int c = close(fd);
  if (c == -1){
    fprintf(stderr,"Close error:%s\n", strerror(errno));
  }
}


int Exit(int exitnum){
  if (logon == 1){
    Close(logfd);
  }		 
  exit(exitnum);
}

ssize_t Read(int fd, void * buf, size_t count){
  ssize_t r = read(fd, buf, count);
  if (r == -1){
    fprintf(stderr, "Read error:%s\n", strerror(errno));
    Exit(1);
  }
  return r;
}

int len(int n){
  return (floor(log10(abs(n))) + 1);
}

void check_for_id_host_and_port(){
  int e = 0;
  if (id_num == 0 || len(id_num) != 9){
    fprintf(stderr, "Need to provide a 9 digit ID.\n");
    e =1;
  }
  if (host_name == NULL){
    fprintf(stderr, "Need to provide a host name or address.\n");
    e = 1;
  }
  if (port_num  < 0){
    fprintf(stderr, "Port must be positive.\n");
    e = 1;
  }
  if (e)
    Exit(1);
}

void getopts(int argc, char **argv){
  while (optind <argc){
    static struct option long_options[]=
      {
        {"scale", required_argument, 0, 's'},
        {"log", required_argument, 0, 'l'},
	{"period", required_argument, 0, 'p'},
	{"id", required_argument, 0, 'i'},
	{"host", required_argument, 0, 'h'},
        {0,0,0,0}
      };
    int option_index = 0;
    int c = getopt_long(argc, argv, "", long_options, &option_index);
    if (c == 's'){
      if (*optarg == 'C'){
	temp = 1;
      }
      else if (*optarg == 'F')
	temp = 0;
      else{
	fprintf(stderr, "Argument for scale can omly be C or F\n");
	exit(1);
      }
    }
    if (c == 'l'){
      logon = 1;
      logfile = optarg;
    }
    if (c == 'p'){
      amount = atoi(optarg);
    }
    if (c == 'i'){
      id_num = atoi(optarg);
    }
    if (c == 'h'){
      host_name = optarg;
    }
    else if (c == '?'){
      fprintf(stderr, "Arguments can only be --scale and --log\n");//print the error and say what the arguments can actually be
      exit(1);
    }
    else if (c == -1){//TODO: only works as last argument. is this correct?
      port_num= atoi(argv[optind]);
      optind++;}
  }				  
}

void button_func();

void startup(){
  //start generating reports again
  reports = 1;
}

void stop(){
  //stop generating reports but keep on logging
  reports =0;
}


void parse_inputs(){
  char buf[512];
  ssize_t r = Read(STDIN_FILENO/*sockfd*/, buf, 512);
  int offset = 0;
  int count = 0;
  int i;
  for (i = 0; i < r; i++){
    //    printf("char %c ", buf[i]);
    if (buf[i] == '\n'){//end of command
      //get command
      buf[i] = '\0';
      //printf("count %d offset %d\n", count, offset);
      if (strncmp(buf + offset, "SCALE=F", count) == 0){
	dprintf(logfd, "SCALE=F\n");
	temp = 0;
      }
      else if (strncmp(buf + offset, "SCALE=C", count) == 0){
	dprintf(logfd, "SCALE=C\n");
	temp = 1;
      }
      else if (strncmp(buf + offset, "START", count) == 0){
	dprintf(logfd, "START\n");
	startup();
      }
      else if (strncmp(buf + offset, "STOP", count) == 0){
	dprintf(logfd, "STOP\n");
	stop();
      }
      else if (strncmp(buf + offset, "OFF", count) == 0){
	dprintf(logfd, "OFF\n");
	button_func();
      }
      else if (strncmp(buf + offset, "PERIOD", 6) == 0){
	//change the period
	dprintf(logfd, "PERIOD=%d\n", atoi(&buf[offset + 7]));
	amount = atoi(&buf[offset + 7]);
      }
      else if (strncmp(buf + offset, "LOG", 3) == 0){
	//log the output
	char filename[512];
	int k;
	for(k = 0; k < count; k++){
	  filename[k] = buf[offset + k];
	}
	filename[count] = '\0';
	dprintf(logfd, "%s\n", filename);
      }
      else if (strncmp(buf + offset, "-EOF", 4) == 0){
	//pass the test script
      }
      else{
	fprintf(stderr, "Invalid argument\n");
	Exit(1);
      }
      offset = i +1;
      count = 0;
    }
    count ++;
  }
}

void print_time();

void button_func(){
  print_time();
  dprintf(STDIN_FILENO/*sockfd*/, "SHUTDOWN\n"); //TODO change to dprintf(1, "SHUTDOWN");
  if (logon == 1){
    dprintf(logfd, "SHUTDOWN\n");
  }
  Exit(0);
}

void print_time(){
  char buffer[9];
  time_t rawtime;
  struct tm *timeinfo;
  time(&rawtime);
  timeinfo = localtime(&rawtime);
  strftime(buffer, 9, "%H:%M:%S", timeinfo);
  if (reports == 1){
    dprintf(STDOUT_FILENO, "%s", buffer);//TODO: change to dprintf("%s", buffer)
    dprintf(STDOUT_FILENO, " ");
  }
  if (logon == 1){
    dprintf(logfd, "%s", buffer);
    dprintf(logfd, " ");
  }		
}

void get_temp(uint16_t value){
  float R = 1023.0/value - 1.0;
  R = Ro * R;

  float temperature = 1.0/(log(R/Ro)/B+1/298.15)-273.15; // convert to celsius
  if (temp == 0){//f
    temperature = (temperature*1.8) + 32;
  }
  if (reports == 1){
    dprintf(STDOUT_FILENO, "%.1f\n", temperature);
  }
  if (logon == 1){
    dprintf(logfd, "%.1f\n", temperature);
  }
}

int main (int argc, char **argv){
  getopts(argc, argv);
  check_for_id_host_and_port();
  printf("port num: %d\n", port_num);
  if (logon == 1){
    logfd = Open(logfile, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  }

  mraa_gpio_context button;
  mraa_aio_context tempsensor;
  
  button = mraa_gpio_init(60);
  tempsensor = mraa_aio_init(1);
  
  mraa_gpio_dir(button, MRAA_GPIO_IN);
  mraa_gpio_isr(button, MRAA_GPIO_EDGE_RISING, &button_func, NULL);

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if(sockfd < 0){
    fprintf(stderr, "Error while making the socket.\n");
    Exit(2);
  }
  printf("Made socket\n");
  struct hostent *server = gethostbyname(host_name);
  if(server == NULL){
    fprintf(stderr, "no such host\n");
    Exit(1);
  }
  printf("got host\n");
  struct sockaddr_in addr;
  addr.sin_port = htons(port_num);
  addr.sin_family = AF_INET;
  memcpy((char *) &addr.sin_addr.s_addr, (char *) server->h_addr, server->h_length);
  if(connect(sockfd, (struct sockaddr*) &addr, sizeof(addr)) < 0){
    fprintf(stderr, "Error connecting to the server.\n");
    Exit(2);
  }
  printf("Connected\n");
  dprintf(sockfd, "ID=%d\n", id_num);
  if(logon){
    dprintf(logfd, "ID=%d\n", id_num);
  }
  
  struct pollfd pfd[1];
  pfd[0].fd = /*sockfd*/STDIN_FILENO;
  pfd[0].events = POLLIN;
  pfd[0].revents = 0;
  
  while(run_flag == 1){
    value = mraa_aio_read(tempsensor);

    print_time();
    get_temp(value);
    sleep(amount);
    

    int p = poll(pfd, 1, 0);
    //printf("logon %d\n", logon);
    if (p == -1){
      fprintf(stderr, "Poll error:%s\n", strerror(errno));
      Exit(1);
      }
    if (pfd[0].revents & POLLIN){
      parse_inputs();
    }
    
  }
  mraa_gpio_close(button);
  mraa_aio_close(tempsensor);
  // close(sockfd);
  return 0;
}


