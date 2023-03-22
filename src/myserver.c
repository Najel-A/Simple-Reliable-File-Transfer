// SERVER File Save @2:23pm
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/ip.h> 
#include <arpa/inet.h>
#include <netdb.h>
#include <strings.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#define MAXLINE 4096
#define SEC_PER_DAY   86400
#define SEC_PER_HOUR  3600
#define SEC_PER_MIN   60

int main(int argc, char **argv) {
    // Fails if less arguments are given than expected
    if (argc < 3) {
        fprintf(stderr, "Usage: <Port Number> <Droppc>.\n");
        exit(1);
    }

    uint16_t port = atoi(argv[1]);  // Port number specifief
    int64_t drop = atoi(argv[2]);   // Percentage of how many packets will be dropped
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        fprintf(stderr, "Socket Error.\n");
        return 1;
    }

    struct sockaddr_in servaddr, cliaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&servaddr,  sizeof(servaddr)) < 0 ) {
        fprintf(stderr, "Bind failed.\n");
        exit(1);
    }
    


    while(true){
        socklen_t len;
        len = sizeof(cliaddr);
        // File size data of bytes
        int filesize = 0;
        recvfrom(sockfd, &filesize, sizeof(filesize), 0, (struct sockaddr *)&cliaddr, &len);
        // Sending to see if server is down
        sendto(sockfd, &filesize, sizeof(filesize), 0, (struct sockaddr *)&cliaddr, len);

        // Used for creating outfile pathing
        int n, m;
        char outfile[MAXLINE] = {0};  // Sets up outfile path writing
        char *file = NULL;
        n = recvfrom(sockfd, outfile, MAXLINE, 0, (struct sockaddr *)&cliaddr, &len);
        // Used for creating outfile path if directory is not made
        int outfd;
        char *token;
        int count = 0;
        outfd  = open(outfile, O_RDWR | O_CREAT); // Make file check later
        if (outfd < 1) {
            for (uint i = 0; i < strlen(outfile); i++) {
                if (outfile[i] == '/') {
                    count++;
                }
            }
            token = strtok(outfile, "/");
            while (token != NULL && count > 0) {
                count--;
                if (token != NULL) {
                    outfd = mkdir(token, 0777);
                    chdir(token);
                }
                token = strtok(NULL, "/");
            }
            file = token;
            outfd  = open(file, O_RDWR | O_CREAT); // Make file check later
            printf("Outfile: %s\n", file);
        }

        // Timestamping
        time_t now;
        time(&now);
        struct tm *p = localtime(&now);
        char time[100];
        size_t l = strftime(time, sizeof(time) - 1, "%FT%Tz", p);
        // move last 2 digits
        if (l > 1) {
            char minute[] = { time[l-2], time[l-1], '\0' };
            sprintf(time + l - 2, "%s", minute);
            for (uint i = strlen(time); i > 0; i--) {
                if (time[i] == ':') {
                    time[i] = '.';
                    break;
                }
            }
        }

        // Packet Drop simulation
        int packet = 1;
        int sn = 1;
        int p_drop = rand() % 101;
        char *server[MAXLINE];
        int serverC;

        // Writing contents of file
        char buf[MAXLINE];  // strlen of buf will always be MTU size

        recvfrom(sockfd, &serverC, sizeof(serverC), 0, (struct sockaddr *)&cliaddr, &len);  // Getting count for server save

        // Receive data then send ACK
        // After sending ACK store in server
        // If packet is dropped ask for retransmit after window is done transmitting
        // Receive next ACK
        // Then write all of the contents
        while (filesize > 0) {
            if (p_drop > drop) {    // Packets sent
                recvfrom(sockfd, &sn, sizeof(sn), 0, (struct sockaddr *)&cliaddr, &len);
                n = recvfrom(sockfd, buf, MAXLINE, 0, (struct sockaddr *)&cliaddr, &len);
                server[sn-1] = strndup(buf, n);
                // memcpy(server[sn-1], buf, n);
                m = sendto(sockfd, &sn, sizeof(sn), 0, (struct sockaddr *)&cliaddr, len);
                filesize -= n;
                printf("%s %s, %d\n", time,"DATA", sn);
                printf("%s %s, %d\n", time,"ACK", sn);
            }
            else {  // Packets Dropped
                recvfrom(sockfd, &sn, sizeof(sn), 0, (struct sockaddr *)&cliaddr, &len);
                n = recvfrom(sockfd, buf, MAXLINE, 0, (struct sockaddr *)&cliaddr, &len);
                printf("%s %s, %d\n", time,"DROP DATA", sn);
                printf("%s %s, %d\n", time,"DROP ACK", sn);
                sn = -1;
                m = sendto(sockfd, &sn, sizeof(sn), 0, (struct sockaddr *)&cliaddr, len);
            }
            
            memset(buf, 0, n);
            
            packet++;
            p_drop = rand() % 101;

        }
        // Write Outfile
        for(int i = 0; i < serverC; i++) {
            // printf("%s\n", server[i]);
            write(outfd, server[i], strlen(server[i]));
            free(server[i]);
        }
        close(outfd);
    }
    close(sockfd);
    return 0;
    
}
