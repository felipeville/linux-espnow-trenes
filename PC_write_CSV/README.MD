## Before using this code
Do not forget to turn on monitor mode and choose the right channel on your wireless interface card.

Here is a exmaple on how to do it : 
```
sudo ifconfig wlp5s0 down
sudo iwconfig wlp5s0 mode monitor
sudo ifconfig wlp5s0 up
sudo iwconfig wlp5s0 channel 1 
```

## How to run the code
To compile the code just use :
```
make
```

And to run the program :
```
sudo ./bin/exec <wifi_interface>
```
An output file named `data.csv` will be generated in the base directory.
