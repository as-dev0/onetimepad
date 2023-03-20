#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>  // ssize_t
#include <sys/socket.h> // send(),recv()
#include <netdb.h>      // gethostbyname()
#include <fcntl.h>
#include <stdint.h>


char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";


// Error function used for reporting issues
void error(const char *msg) { 
    perror(msg); 
    exit(0); 
} 


// Set up the address struct
void setupAddressStruct(struct sockaddr_in* address, 
                        int portNumber){
 
    // Clear out the address struct
    memset((char*) address, '\0', sizeof(*address)); 

    // The address should be network capable
    address->sin_family = AF_INET;
    // Store the port number
    address->sin_port = htons(portNumber);

    // Get the DNS entry for the hostname "localhost"
    struct hostent* hostInfo = gethostbyname("localhost"); 
    if (hostInfo == NULL) { 
        fprintf(stderr, "CLIENT: ERROR, no such host\n"); 
        exit(0); 
    }
    // Copy the first IP address from the DNS entry to sin_addr.s_addr
    memcpy((char*) &address->sin_addr.s_addr, 
            hostInfo->h_addr_list[0],
            hostInfo->h_length);
}


// Takes three arguments, a string named buffer, a string named cipher
// and a string named key. Concatenates cipher and key into buffer, separated
// by a comma. The first character of buffer will be a plus sign to indicate
// that this buffer was sent from dec_client
void prepareToSend(char *buffer, char *cipher, char *key){

    strcat(buffer, "-");
    strcat(buffer, cipher);
    strcat(buffer, ",");
    strcat(buffer, key);

    // Null terminate buffer
    int lastIndex = strlen(cipher) + strlen(key) + 2;
    memset(buffer+lastIndex, '\0', 1);

}


// Takes one argument, a string named filename, and returns the number of
// characters in the file
int getLength(char *filename){

    FILE* f = fopen(filename, "r");
    fseek(f,0L,SEEK_END);
    int length = ftell(f);
    fclose(f);
    return length;
}


// Takes three arguments, an integer socketfd, a string buffer, an integer lenToSend
// This function sends the buffer using socketfd as socket file descriptor, and 
// supports sending the data over multiple sends in case the first attempt at sending
// does not send all the buffer, making sure that no duplicate data is sent.
// Returns the total number of bytes sent.
int improvedSend(int socketfd, char *buffer, int lenToSend){

    int numberBytesSent = 0;
    int numberBytesRemaining = lenToSend;

    while (numberBytesSent != lenToSend){

        int charsSent = send(socketfd , buffer + numberBytesSent, numberBytesRemaining, 0);

        numberBytesSent += charsSent;
        numberBytesRemaining -= charsSent;

    }

    return numberBytesSent;
}


// Takes three arguments, an integer socketfd, a string buffer, an integer lenToReceive
// This function receives data into the buffer using socketfd as socket file descriptor, and 
// supports receiving the data over multiple attempts in case the first attempt at receiving
// does not get all the data, making sure that no duplicate data is received.
// Returns the total number of bytes received.
int improvedReceive(int socketfd, char *buffer, int lenToReceive){

    int numberBytesReceived = 0;
    int numberBytesRemaining = lenToReceive;

    while (numberBytesReceived != lenToReceive){

        int charsReceived = recv(socketfd, buffer + numberBytesReceived, numberBytesRemaining, 0);

        numberBytesReceived += charsReceived;
        numberBytesRemaining -= charsReceived;

    }

    return numberBytesReceived;
}


// Takes two arguments, a string named haystack and a string named needle.
// Returns the index of the needle in the haystack. If the needle is not found,
// returns -1.
int indexOf(char *haystack, char *needle){

    for (int i = 0; i < strlen(haystack); i++){

        // This represents the ith character of the haystack
        char character[2]; memset(  character  ,'\0', 2 );
        strncpy(character, haystack+i,1); character[2] = '\0';

        if (strcmp(character, needle) == 0){
            return i;
        }
    }

    return -1;

}


// Takes one argument, a string named cipher, and returns 0 if all
// characters in cipher are valid (alphabet or space). Returns -1
// otherwise
int validCharacters(char *cipher){

    for (int i = 0; i < strlen(cipher); i++){

        // This represents the ith character of cipher
        char character[2]; memset(  character  ,'\0', 2 );
        strncpy(character, cipher+i,1); character[2] = '\0'; 

        if (indexOf(alphabet, character) == -1){
            return -1;
        }

    }

    return 0;

}


// Takes three arguments, a string named buffer, a string named filename,
// and an integer named len. Copies len-1 characters from the file named filename into
// the buffer
void copyFileToBuffer(char *buffer, char *filename, int len){

    int fdinput = open(filename, O_RDONLY, 00600);
    read(fdinput, buffer, len-1);
    memset(buffer+len-1, '\0',1); // Null terminate buffer

}


int main(int argc, char *argv[]) {

    // Check usage & args
    if (argc < 4) {
        fprintf(stderr,"Please enter a command of this form: ./dec_client ciphertext key port\n"); 
        exit(0); 
    } 


    // -------------------- Obtain ciphertext and key from files and validate --------------------


    int lenCipher = getLength(argv[1]);
    int lenKey = getLength(argv[2]);

    // Check length of key
    if ( lenKey < lenCipher ){
        fprintf(stderr,"Please enter a key that is at least as long as the ciphertext.\n"); 
        exit(1); 
    }

    // Allocate memory for ciphertext and key
    char *ciphertext = malloc(lenCipher); memset(ciphertext, '\0', lenCipher); 
    char *inputKey = malloc(lenKey);  memset(inputKey, '\0', lenKey);

    copyFileToBuffer(ciphertext, argv[1], lenCipher);

    if (validCharacters(ciphertext) == -1){
        fprintf(stderr,"There are invalid characters in the ciphertext.\n"); 
        exit(1);
    }

    copyFileToBuffer(inputKey, argv[2], lenKey);


    // --------------------------- Setup socket and connect to server ---------------------------


    int socketFD, portNumber, charsWritten, charsRead;
    struct sockaddr_in serverAddress;

    // Create a socket
    socketFD = socket(AF_INET, SOCK_STREAM, 0); 
    if (socketFD < 0){
        error("CLIENT: ERROR opening socket");
    }

    // Set up the server address struct
    setupAddressStruct(&serverAddress, atoi(argv[3]));

    // Connect to server
    if (connect(socketFD, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0){
        fprintf(stderr,"Error connecting to server. Port used to connect is ");
        fprintf(stderr,argv[3]);
        fprintf(stderr,"\n");
        exit(2); 
    }


    // --------------------------------- Send ciphertext and key ---------------------------------

    // My approach is to first send the length of the buffer that we will later send in a small 10-byte message,
    // then send the ciphertext and key
    // That allows the server to know how much data it should receive

    // Setup buffer that will be sent
    int bufferSize = strlen(ciphertext) + strlen(inputKey) + 3; // we allocate three more bytes: 1 for the "-", 1 for the ",", and 1 for the null terminator
    char *buffer = malloc(bufferSize); memset(buffer, '\0', bufferSize);

    // Concatenates ciphertext and key into buffer
    prepareToSend(buffer, ciphertext, inputKey);

    // Before sending the message to the server, we send the length of the buffer we will send
    char lenToSend[10]; memset(lenToSend, '\0', 10);
    sprintf(lenToSend,"%i", bufferSize-1);
    send(socketFD, lenToSend, 10, 0);

    // Send concatenated ciphettext and key to server
    charsWritten = improvedSend(socketFD, buffer, bufferSize-1); 

    if (charsWritten < 0){
        error("CLIENT: ERROR writing to socket");
    }
    if (charsWritten < strlen(buffer)){
        printf("CLIENT: WARNING: Not all data written to socket!");
    }


    // ------------------------------ Receive and print plaintext ------------------------------

    // The server will send us the length of the plaintext, and then send
    // us the actual plaintext

    // Clear out the buffer again for reuse
    memset(buffer, '\0', bufferSize);

    // The message received at this point will be the length of the plaintext to be received
    char lenToReceive[10]; memset(lenToReceive, '\0', 10);
    recv(socketFD, lenToReceive, 10, 0);
    int lengthBuffer = atoi(lenToReceive);

    // The server sends "*" back if the request went to enc_server
    if (strcmp(lenToReceive, "*") == 0){
        fprintf(stderr, "CLIENT: ERROR: Server rejected connection. You cannot connect to enc_server from dec_client.\n");
        exit(2);
    }

    // Read data from the socket. Receiving plaintext from server and storing into buffer.
    charsRead = improvedReceive(socketFD, buffer, lengthBuffer);
    memset(buffer+lengthBuffer, '\0',1);

    if (charsRead < 0){
        error("CLIENT: ERROR reading from socket");
    }
    else {
        printf("%s\n", buffer);
    }

    // Close the socket
    close(socketFD); 
    return 0;
}