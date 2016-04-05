## UDTCAT
udtcat is simple data transfer tool over UDP-based Data Transfer (UDT) protocol. 
udtcat is developed to send and receive data across UDT network connections from the command line, similar to the popular `nc` utility. 

## What is UDT?
UDT is a reliable UDP based application level data transport protocol for distributed data intensive applications over wide area high-speed networks. UDT uses UDP to transfer bulk data with its own reliability control and congestion control mechanisms. 
The new protocol can transfer data at a much higher speed than TCP does. read more at http://udt.sourceforge.net/

## Installation
udtcat uses on the UDT library, please install the library development package

** Debain and Ubuntu **

`$ sudo   apt-get   update`
`$ sudo   apt-get   install   build-essential   libudt-dev`
`$ make`
`$ sudo   make   install`

** Fedora, Centos, and Red hat **

`$ sudo   yum   install   make   gcc-c++   udt-devel`
`$ make` 
`$ sudo   make   install`

## Examples

** simple chat **

`$ udtcat -l` listen for connections
`$ udtcat 192.168.100.99` connect to machine

** file transfer **

`$ udtcat -l -p 8899 > received_file.data` listen on port 8899 and redirect output to 'received_data.dat'
`$cat data.dat | udtcat 192.168.100.99 -p 8899` send the file 'data.dat'

udtcat will print total sent/received bytes upon receiving the signal SIGUSR1. `$ killall -SIGUSR1 udtcat`

for more information please read the users manual `$ man udtcat`
