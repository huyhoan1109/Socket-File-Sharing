cd server

make clean

make

clear

if [ -z "$1" ]
then
    ./server 8888
else
    ./server $1
fi