#include <stdio.h>
#include <iostream>
#include <iomanip>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <chrono>
#include <cstring>
#include <csignal>
#include <cmath>
#include <sstream>


int pingloop = 1;
char host[50];
char* dns_lookup(char*);
void set_ttl(int,int);

//ICMP  structure
struct icmp{
  uint8_t icmp_type;
  uint8_t icmp_code;
  uint16_t icmp_checksum;
  uint32_t icmp_data;
};
//icmp response for ttl
struct icmp_response{
  uint8_t icmp_type;
  uint8_t icmp_code;
  uint16_t icmp_checksum;
  uint16_t icmp_identifier;
  uint16_t sequence_number;
};

//handler for interruption
void loop_Handler(int nothing){
    pingloop=0;
}

int main(int argc, char *argv[]){

  time_t inicial, final;
  int pack_send = 0,i=0,rep_loss = 0,ttl_set=0,pack_received = 0;
  unsigned int resAddressSize;
  double total_time=0.0;
  char* arg1 = argv[1];
  char host_ip[50];
  struct sockaddr_in addr;
  struct sockaddr resAddress;
  unsigned char buf[sizeof(struct in_addr)],res[30] = "";


  if (argc < 2){
  std::cerr << "\nNo arguments."<<std::endl;
  return EXIT_FAILURE;
}
  if(argc == 3){
    std::istringstream ss(argv[2]);
    if(!(ss>>ttl_set)){
    std::cerr << "invalid number: " <<argv[2]<< '\n';
    }
    else if(ttl_set > 255 || ttl_set < 0)
    std::cerr << "TTL number incorrect." << '\n';

  }
  for (i = 0; i < 50; i++) {
    if (arg1[i] == '\0')break;
    else
      host_ip[i] = arg1[i];
    }
  host_ip[i] = '\0';

  char *host_check = dns_lookup(host_ip); //resolve hostname ip return ipv4 or ipv6

  //create a socket
  int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
  if(sock <= 0)
  {
      std::cout << "\nSocket error"<<std::endl;
      return EXIT_FAILURE;
  }

  if(argc==3)
  set_ttl(sock,ttl_set);
    icmp icmp_packet;
    //create a icmp packet and set parameters
    icmp_packet.icmp_type = 8;
    icmp_packet.icmp_code = 0;
    icmp_packet.icmp_checksum = 0xfff7;
    icmp_packet.icmp_data = 0;

    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    addr.sin_addr.s_addr = inet_pton(AF_INET,host_check,buf);


  std::cout<<"PING: "<<host_ip<<"("<<host_check<<")"<<"\n";
  time(&inicial);
  signal(SIGINT, loop_Handler);//catching interrupt
  while(pingloop){

    auto start = std::chrono::steady_clock::now(); //starting chronometer
    //send icmp packets
    int send_packet = sendto(sock, &icmp_packet, sizeof(icmp_packet), 0, (struct sockaddr*)&addr, sizeof(addr));

    if(send_packet < 0)
    {
      std::cout << "\nPing error."<<std::endl;
      return EXIT_FAILURE;
      }
    else
      pack_send++;


     //create struct to recieved icmp

    int response_pack = recvfrom(sock, res, sizeof(res), 0, &resAddress, &resAddressSize);//receive icmp packet
    icmp_response* response_icmp;
    response_icmp = (icmp_response *)&res[20];
    auto end = std::chrono::steady_clock::now();  //stoping chronometer
    if(response_pack > 0 && pack_send > 0){
      pack_received++;

      if(response_icmp -> icmp_type == 11 || response_icmp -> icmp_type == 3 ){ //decrease packet that reach ttl or unreachable
        pack_received--;
      }
    }
    else
    {

        std::cout<<"Response Error\n";
    }
    std::chrono::duration<double> elapsed_seconds = end-start;

    if(response_icmp -> icmp_type == 11){ //check if ttl reach maximum
    total_time += elapsed_seconds.count()*1000;
    rep_loss = ((pack_send-pack_received)/pack_send)*100;
    std::cout<<"packet received from: "<<host_check<<", ";
    std::cout<<rep_loss<<"% packet lost, ";
    std::cout<<"time exceeded\n";
  }
    else if(response_icmp -> icmp_type == 3){ // check if the destinaion is unreachable
    total_time += elapsed_seconds.count()*1000;
    rep_loss = ((pack_send-pack_received)/pack_send)*100;
    std::cout<<"packet received from: "<<host_check<<", ";
    std::cout<<rep_loss<<"% packet lost, ";
    std::cout<<"destination unreachable\n";
  }
  else{
    total_time += elapsed_seconds.count()*1000;
    rep_loss = ((pack_send-pack_received)/pack_send)*100;
    std::cout<<std::fixed << std::setprecision(1);
    std::cout<<"packet received from: "<<host_check<<", ";
    std::cout<<rep_loss<<"% packet lost, ";
    std::cout<<elapsed_seconds.count()*1000<<" ms\n";
  }

      usleep(1000000); //periodic delay
  }
    time(&final);
    double time_taken = double(final - inicial);
    std::cout<<"\n("<<host_check<<") --- ping statistics ---\n";
    std::cout<<pack_send<<" packets transmitted, ";
    std::cout<<pack_received<<" received, ";
    std::cout<<rep_loss<<"% packet lost, ";
    std::cout<<"time "<<round(time_taken*1000.0)<<" ms\n";
    std::cout<<"rtt avg = "<<(total_time/pack_send)<<" ms\n";

  return 0;
}

char* dns_lookup(char *host_ip){
  struct addrinfo hints, *infoptr;
  int result;
  memset(&hints, 0, sizeof hints);

  hints.ai_family = AF_INET; // for IPv4  addresses

  result = getaddrinfo(host_ip, NULL, &hints, &infoptr);
    if(result){
      std::cerr<<"getaddrinfo: "<< gai_strerror(result)<<std::endl;
      exit(1);
    }
  getnameinfo(infoptr->ai_addr, infoptr->ai_addrlen, host, sizeof (host), NULL, 0, NI_NUMERICHOST);

  freeaddrinfo(infoptr);
  return host;
}

void set_ttl(int ping_sockfd,int ttl_val){
  int change_options = setsockopt(ping_sockfd, SOL_IP, IP_TTL,
               &ttl_val, sizeof(ttl_val));
   if (change_options != 0){
    std::cout<<"\nSetting socket options to TTL failed!\n";
    return;
  }
  else
    std::cout<<"\nTTL set to: "<<ttl_val<<std::endl;
  }
