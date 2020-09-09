# raspberry_pi-bluez_server_file_transfer

<em>raspberry_pi-bluez_server_file_transfer</em> is a C tool that allows to transfer files from a Raspberry to a Slate106 through BLE (Bluetooth Low Energy).
This project is based on BlueZ 5.54 (<a href="http://www.bluez.org/">BlueZ official website</a>).

#### What is done by Bluez File Transfer
* LE scan & Bluetooth connection
* Connection process of Slate106
* File transfer 
* Restart scan 

You have two possibilities to use this project : 

* You can use an executable of the project, located in the <em>bin</em> folder
* Else, you can build the project

## How to use the executable

#### Scan & Connection

First, the SLATE106 address had to be specified in argumet of the main. So you have to run : 
```bash
$> sudo ./bluez_file_tranfer <MAC address>
``` 

MAC address is something like this :  xx:xx:xx:xx:xx:xx


Super user (sudo) is used because Bluetooth Low Energy tools need to interact with Bluetooth local adapter.
It is possible to use setcap tools to give permission but it seems that this command is only effective in <em>usr/bin</em> folder. 


#### File Transfer

When you use the executable of the <em>bin</em> folder, you will transfer the <em>hub.pkk file</em> that is in the <em>img</em> folder. If you want to change the path to the file to transfer you must build bluez_server_file_transfer.

## Build Bluez-File-Transfer

To Build Bluez File Transfer, an access to the BlueZ sources is needed. 
You have two possiblities : 
* Use the BlueZ sources provided in the <em>bluez_deps</em> folder.
* Build BlueZ. 

### Use Bluez_deps

This folder is made up only of BlueZ sources useful to build the project.
You just have to use the <em>Makefile</em> which is provided in this project. Verify in the Makefile that <code>BLUEZ_BUILD</code> is at false.

Then you can run : 

```bash
$> make
``` 

By default, the path to the file to transfer is /bin/img/  but you can specify another path by modifying <code>roothpath</code> in <em>src/file_transfer_task.c</em>


### Build BlueZ

Connect to a terminal on your Pi and download the source code of BlueZ : 
```bash
$> wget http://www.kernel.org/pub/linux/bluetooth/bluez-5.54.tar.xz
``` 

At the time of writing, the BlueZ version was 5.54. May be a new version is available, check it at <a href="http://www.bluez.org/">BlueZ official website</a>.

Then, to untar it, run : 
```bash
$> tar xvf bluez-5.54.tar.xz
``` 
Install Bluetooth dependencies :
```bash
$> sudo apt-get install -y libbluetooth-dev ibusb-dev libdbus-1-dev libglib2.0-dev libudev-dev libical-dev libreadline-dev
```  
Compile & install Bluez :
```bash
$> ./configure --enable-library
$> make
$> sudo make install
``` 
Enable Bluetooth Low Energy Features :
<p>
You have to modify the configuration file of the bluetooth to enable BLE features. 

```bash
$> sudo nano /lib/systemd/system/bluetooth.service
``` 

Replace ExecStart line by :
```bash
$> ExecStart=/usr/local/libexec/bluetooth/bluetoothd --experimental --noplugin=sap
``` 
Reaload configuration file : 
```bash
$> sudo systemctl daemon-reload
$> sudo systemctl restart bluetooth
``` 

Now, modify <code>BLUEZ_BUILD</code> in the makefile by true to indicate that Bluez have been build and to choose the right sources. 

Then, run : 

```bash
$> make
``` 

By default, the path to the file to transfer is /bin/img/  but you can specify another path by modifying <code>roothpath</code> in <em>src/file_transfer_task.c</em>

## SLATE106 configuration

The configuration of the SLATE106 must respect  : 
* Testcard deactivated
* Use WPAN1 without pincode and not WPAN2
* Picture Filename : hub.pkk

If you want to be sure that you have the right configuration, you can use the configuration file <em>APPLI.CFG</em> in <em>slate106_config</em> folder.

## Remarks

### File transfer

* In the folder where you placed the file you want to transfer, there cannot be more than 7 files.
* You can modify the path where is the file to transfer. For that, you must specify the new path by modifying <code>rootpath</code> in <em>src/file_transfer_task.c</em>

### Useful command  

If <em>"set scan parameters failed"</em> error append, run : 
```bash
$> sudo hciconfig hci0 down
$> sudo hciconfig hci0 up
``` 

If the error is still present, run : 
```bash
$> sudo bluetoothctl
$> devices
``` 
If in the list of devices, there is your Slate MAC address, run : 
        
```bash
$> remove <MAC address>
``` 

## Todo

* Add pin code authentication