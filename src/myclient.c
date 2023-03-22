// CLIENT File Save @2:23pm
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
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

int main(int argc, char **argv) {

    // Fails if less arguments are given than expected
    if (argc < 7) {
        fprintf(stderr, "Usage: <SERVER IP> <SERVER PORT> <MTU> <WINSZ <INFILE PATH> <OUTFILE PATH>\n");
        return 1;
    }

    // Setting variables passed in from arguments
    char *IP = argv[1];
    char *port = (argv[2]);  // Turning port number into uint16_t
    int mtu = atoi(argv[3]);
    int winsz = atoi(argv[4]);
    char *infile = argv[5];
    char *outfile = argv[6];

    // Checking port values and mtu size
    if (mtu > 32000) {  // must not exceed 32000
        fprintf(stderr, "MTU must not exceed 32000\n");
        exit(1);
    }

    if (atoi(port) < 1024 || atoi(port) > 65536) {
        fprintf(stderr, "Invalid Port Range\n");
        exit(1);
    }


    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in servaddr;
    socklen_t addrlen = sizeof(servaddr);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(atoi(port));
    servaddr.sin_addr.s_addr = inet_addr(IP); // IP address instead
    //servaddr.sin_addr.s_addr = htons(atoi(port));; // IP address instead

    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {  // Doesn't work properly?
        fprintf(stderr, "No response from server.\n");
        exit(1);
    }

    // File opening section
    int fd = open(infile, O_RDONLY);
    if (fd < 1) {
        fprintf(stderr, "File does not exist\n");
        exit(0);
    }
    int bytes = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    // Socket timeout setup, needs work
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    int timeout;
    
    // Sending amount of bytes of file
    sendto(sockfd, &bytes, sizeof(bytes), 0, (struct sockaddr *)&servaddr, sizeof(servaddr)); // send outfile path
    timeout = recvfrom(sockfd, &bytes, mtu, 0, (struct sockaddr *)&servaddr, &addrlen);
    if (timeout < 0) {
        printf("Cannot detect server\n");
        exit(1);
    }

    // Sending outfile pathing to server
    sendto(sockfd, outfile, strlen(outfile), 0, (struct sockaddr *)&servaddr, sizeof(servaddr)); // send outfile path

    // Timestamping
    time_t now;
    time(&now);
    struct tm *p = localtime(&now);
    char time[100];
    size_t l = strftime(time, sizeof(time) - 1, "%FT%Tz", p);
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


    // File Transfer Process
    char buf[mtu];
    int bytes_read, n;

    char *file[MAXLINE];
    int c = 0;
    while ((bytes_read = read(fd, buf, mtu)) > 0){
        file[c] = strndup(buf, bytes_read);
        // memcpy(file[c], buf, bytes_read);
        c++;
    }
    int temp = c;
    close(fd);
    sendto(sockfd, &c, sizeof(c), 0, (struct sockaddr *)&servaddr, sizeof(servaddr)); // array size

    int left = 1;
    int right = winsz+1;
    int rcvAcks = 0;
    int acknum = -1;
    int redo = 0;
    int actual = 0;
    int packResend[50];
    while((c > 0)) {
        while (left <= right) {
            if ((left == right) && (redo == 0)) {
                right += winsz;
            }
            if (((left == right) && (redo > 0)) || (c == 0)) {  // retransmit
                for(int i = 0; i < redo; i++) {
                    sendto(sockfd, &packResend[i], sizeof(int), 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
                    sendto(sockfd, file[packResend[i]-1], strlen(file[packResend[i]-1]), 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
                    recvfrom(sockfd, &acknum, sizeof(int), 0, NULL, NULL);
                    while (acknum == -1) {
                        sendto(sockfd, &packResend[i], sizeof(int), 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
                        sendto(sockfd, file[packResend[i]-1], strlen(file[packResend[i]-1]), 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
                        recvfrom(sockfd, &acknum, sizeof(int), 0, NULL, NULL);
                        rcvAcks++;
                        if (rcvAcks > 5) {
                            fprintf(stderr, "Reached max re-transmission limit\n");
                            exit(1);
                        }
                    }
                    rcvAcks = 0;
                    printf("%s, %s, %d, %d, %d, %d\n", time,"ACK", packResend[i], right-1, packResend[i]+1, right+winsz-1);
                }
                redo = 0;
                right += winsz;
            }
            if (c == 0) {
                break;
            }
            sendto(sockfd, &left, sizeof(int), 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
            sendto(sockfd, file[actual], strlen(file[actual]), 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
            printf("%s, %s, %d, %d, %d, %d\n", time,"DATA", left, right-1, left+1, right+winsz-1);
            n = recvfrom(sockfd, &acknum, sizeof(int), 0, NULL, NULL);
            if (acknum == -1) {
                packResend[redo] = left;
                redo++;
                rcvAcks++;
            }
            else {
                rcvAcks = 0;
                printf("%s, %s, %d, %d, %d, %d\n", time,"ACK", left, right-1, left+1, right+winsz-1);
            }
            
            // // Timeout if server is down
            // if (n < 0) {
            //     if (errno == EINTR) {
            //         fprintf(stderr, "Timeout\n");
            //     }
            //     else if (errno == EWOULDBLOCK) {
            //         fprintf(stderr, "Cannot detect server\n");
            //         close(fd);
            //         close(sockfd);
            //         exit(1);
            //     }
            //     else {
            //         printf("Cannot Detect Server\n");
            //         exit(1);
            //     }
            // }

            
            left++;
            actual++;
            c--;
           
        }
        

    }
    
    // Free memory
    for(int i = 0; i < temp; i++) {
        free(file[i]);
    }
    close(sockfd);

   

}
