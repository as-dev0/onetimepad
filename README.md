# onetimepad

I wrote this program to test my understanding of sockets, servers, clients, and other networking concepts in C.

The goal of the program is to encrypt and decrypt data using one time pads.

One time pads are a way of encrypting data. They are the only truly unbreakable encryption scheme, but they have the drawback of requiring a key as long as the data to be encrypted, which is very inconvenient to exchange.

This program consists of two servers: an encryption server and a decryption server. The encryption server receives plaintext, encrypts it using the one time pad, and sends back ciphertext. The decryption server receives ciphertext, decrypts it using the one time pad, and sends back plaintext.

The program also contains two client files: an encryption client and a decryption client. They are used to communicate with the servers.

To compile the four files, run the command: gcc -std=c99 filename.c -o filename
