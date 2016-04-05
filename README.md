## UDTCAT
udtcat is simple data transfer tool over UDP-based Data Transfer (UDT) protocol. 
udtcat is developed to send and receive data across UDT network connections from the command line, similar to the popular `nc` utility. 

## What is UDT?
UDT is a reliable UDP based application level data transport protocol for distributed data intensive applications over wide area high-speed networks. UDT uses UDP to transfer bulk data with its own reliability control and congestion control mechanisms. 
The new protocol can transfer data at a much higher speed than TCP does. read more at http://udt.sourceforge.net/

## Installation
udtcat uses on the UDT library, please install the library development package.

**Debian and Ubuntu**

`$ sudo   apt-get   update` update repository <br />
`$ sudo   apt-get   install   build-essential   libudt-dev` install UDT library and build tools <br />
`$ make` build udtcat <br />
`$ sudo   make   install` install udtcat <br />

**Fedora, Centos, and Red hat**

`$ sudo   yum   install   make   gcc-c++   udt-devel` install UDT library and build tools <br />
`$ make` build udtcat <br />
`$ sudo   make   install` install udtcat <br />

## Examples

**Simple Chat**

`$ udtcat -l` listen for connections <br />
`$ udtcat 192.168.100.99` connect to machine <br />

**File Transfer**

`$ udtcat -l -p 8899 > received_file.data` listen on port 8899 and redirect output to 'received_data.dat' <br />
`$cat data.dat | udtcat 192.168.100.99 -p 8899` send the file 'data.dat' <br />

udtcat will print total sent/received bytes upon receiving the signal SIGUSR1. `$ killall -SIGUSR1 udtcat`

for more information please read the users manual `$ man udtcat`
