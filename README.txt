CS5348 Project-2: V6-Based File System

Team Members:

Rohith Reddy Krishnareddi Gari, 

Venkata kartheek Madhavarapu Steps to run the code:
1. Compile using: g++ -o fsaccess fsaccess.c
2. Run using: ./fsacess <nameofV6filesystem>

Available Commands:

initfs <Number of blocks> <Number of i-nodes>: Initialize the file system
cpin <externalfile> <v6-file>: Copy an externalfile into file system
cpout <v6-file> <externalfile>: Copy V6 File into external system
mkdir <v6-directory>: Create a new directory
rm <v6-file>: Remove a file from file system
q: save all changes and exit
help: Displays available commands