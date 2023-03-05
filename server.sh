cd server

make clean

make

clear

if [ -z $1 ]
then
    ./server 8800
else
    ./server $1
fi