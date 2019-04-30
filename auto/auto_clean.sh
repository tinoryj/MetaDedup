rm -rf server2/
rm -rf server3/
rm -rf server4/

cd server
make clean

cd ..
cd client
make clean

cd ..
rm -rf share*
rm -rf *.d

