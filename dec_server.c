#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>      // gethostbyname()


char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";


// Error function used for reporting issues
void error(const char *msg) {
    perror(msg);
    exit(1);
} 


// Set up the address struct for the server socket
void setupAddressStruct(struct sockaddr_in* address, 
                        int portNumber){
 
    // Clear out the address struct
    memset((char*) address, '\0', sizeof(*address)); 

    // The address should be network capable
    address->sin_family = AF_INET;
    // Store the port number
    address->sin_port = htons(portNumber);

    // Get the DNS entry for this host name
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


// Takes three arguments, a string named cipher, a string named key, 
// and a string named plain. Stores in plain the decrypted version
// of cipher using key.
void decrypt(char *cipher, char *key, char *plain){

    int i;

    for (i = 0; i < strlen(cipher); i++){
        char cipherCharacterAtI[2]; memset(  cipherCharacterAtI  ,'\0', 2 );
        char keyCharacterAtI[2]; memset(  keyCharacterAtI  ,'\0', 2 );

        strncpy(cipherCharacterAtI, cipher+i,1); cipherCharacterAtI[2] = '\0';
        strncpy(keyCharacterAtI, key+i,1); keyCharacterAtI[2] = '\0';

        int indexCipher = indexOf(alphabet,cipherCharacterAtI);
        int indexKey = indexOf(alphabet,keyCharacterAtI);
        
        int sumIndex = (indexCipher - indexKey + 54)%27;
        
        char toAppend[2]; memset(  toAppend  ,'\0', 2 );
        strncpy(toAppend, alphabet+sumIndex, 1);
        toAppend[2] = '\0';

        strcat(plain,toAppend);   
    }
    plain[i] = '\0';
}


// Takes three arguments, a string named buffer, a string named cipher, and
// a string named key. buffer is assumed to be a concatenation of cipher and key.
// Stores the ciphertext contained in the buffer into cipher and stores the key
// contained in the buffer into key.
// Returns 1 if the buffer was received from dec_client and -1 if received from enc_client.
int extractCipher_Key(char *buffer, char *cipher, char *key){

    if (strncmp(buffer, "-", 1) != 0){
        return -1;
    }

    int toggle = 0;

    for (int i = 1; i < strlen(buffer); i++){

        char c[2]; memset(  c  ,'\0', 2 );
        strncpy(c, buffer+i, 1);
        c[2] = '\0';

        if (toggle == 0){

            if (strcmp(c, ",") == 0){
                toggle = 1;
                cipher[i+1] = '\0';
            } 
            
            else {
                strcat(cipher,c);
            }
        } 
    
        else {
            strcat(key,c);
            if (i == strlen(buffer)){
                key[i-strlen(cipher)] = '\0';
            }
        }
    }
    return 1;
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


// usage: exe port
int main(int argc, char *argv[]){

    int connectionSocket;
    struct sockaddr_in serverAddress, clientAddress;
    socklen_t sizeOfClientInfo = sizeof(clientAddress);


    // -------------------------- Setup socket, bind, and listen --------------------------


    // Create the socket that will listen for connections
    int listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket < 0) {
        error("ERROR opening socket");
    }

    // Set up the address struct for the server socket
    setupAddressStruct(&serverAddress, atoi(argv[1]));

    // Associate the socket to the port
    if (bind(listenSocket, 
            (struct sockaddr *)&serverAddress, 
            sizeof(serverAddress)) < 0){
        error("ERROR on binding");
    }

    // Start listening for connetions. Allow up to 5 connections to queue up
    listen(listenSocket, 5); 
    

    // Accept a connection, blocking if one is not available until one connects
    while(1){

        // Accept the connection request which creates a connection socket
        connectionSocket = accept(listenSocket, 
                    (struct sockaddr *)&clientAddress, 
                    &sizeOfClientInfo); 
        if (connectionSocket < 0){
            error("ERROR on accept");
        }


        // -------------------------- Forking child to handle decryption and sending plaintext --------------------------


        pid_t childPid = fork();

        if (childPid == -1){
            perror("fork()\n");
            exit(1);
        
        // Code run by child
        } else if (childPid == 0){

            // My approach before sending any message is to send its size
            // so that the receiving end know how much data it should receive
            // So the client will first send us a number which is the size of its message


            // The first message received from the client will be the length of the concatenated ciphertext and key
            char lenToReceive[10]; memset(lenToReceive, '\0', 10);
            recv(connectionSocket, lenToReceive, 10, 0); // Receiving length of message
            int lengthBuffer = atoi(lenToReceive);


            char *buffer = malloc(lengthBuffer); memset(buffer, '\0', lengthBuffer);
            int charsRead;

            // Read the client's message from the socket
            charsRead = improvedReceive(connectionSocket, buffer, lengthBuffer); // Receiving message
            if (charsRead < 0){
                error("ERROR reading from socket");
            }


            // Extract ciphertext and key from received data
            char *cipher = malloc(lengthBuffer); memset(  cipher  ,'\0', lengthBuffer );
            char *key = malloc(lengthBuffer); memset(  key  ,'\0', lengthBuffer );
            int sentFromCorrectClient = extractCipher_Key(buffer,cipher,key);


            if (sentFromCorrectClient == -1){
                charsRead = send(connectionSocket, "*", 1, 0); 
                continue;
            }

            // Store decrypted data in plainText
            char *plainText = malloc(lengthBuffer); memset(  plainText  ,'\0', lengthBuffer );
            decrypt(cipher,key,plainText);


            // Before sending the message to the client, we send the length of the buffer we will send
            char lenToSend[10]; memset(lenToSend, '\0', 10);
            sprintf(lenToSend,"%i", strlen(plainText));
            send(connectionSocket, lenToSend, 10, 0); // Sending length of message


            // Send the plaintext back to the client
            charsRead = improvedSend(connectionSocket, plainText, strlen(plainText)); // Sending message
            
            if (charsRead < 0){
                error("ERROR writing to socket");
            }
            
            // Close the connection socket for this client
            close(connectionSocket);
            
        } else {
            continue;
        }
    }

    // Close the listening socket
    close(listenSocket); 
    return 0;
}
