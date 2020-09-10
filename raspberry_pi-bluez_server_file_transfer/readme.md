# raspberry_pi-bluez_server_file_transfer

<em>raspberry_pi-bluez_server_file_transfer</em> is a C tool that allows to transfer files from a Raspberry to a SLATE106 through BLE (Bluetooth Low Energy).
This project is based on BlueZ 5.54 (<a href="http://www.bluez.org/">BlueZ official website</a>). If a new version is available, you may use it. 

#### What raspberry_pi-bluez_server_file_transfer does
* LE scan & Bluetooth connection
* Connection process of SLATE106
* File transfer 
* Restart scan 

You have two possibilities to use this project: 

* You can use an executable of the project, located in the <em>bin</em> folder
* You can build the project

## How works the executable

#### Scan & Connection

First, the SLATE106 address had to be specified in argument of the main. So you have to run: 
```bash
$> sudo ./bluez_server_file_transfer <MAC address>
``` 

MAC address is something like this:  xx:xx:xx:xx:xx:xx


Super user (sudo) is used because Bluetooth Low Energy tools need to interact with Bluetooth local adapter.
It is possible to use setcap tools to give capabilities otherwise.  

Now you have to wake up the SLATE106 to initiate the connection process.

#### File Transfer

You will transfer the <em>hub.pkk file</em> that is in the <em>img</em> folder next to the executable. If you want to change the path to the file to transfer you must build bluez_server_file_transfer and edit <code>file_transfer_path</code> in <em>src/file_transfer_task.c</em>.

## Build bluez_server_file_transfer

To Build bluez_server_file_transfer, an access to the BlueZ sources is needed. 
You have two possibilities: 
* Use the BlueZ sources provided in the <em>bluez_deps</em> folder.
* Build BlueZ from its sources. 

### Use bluez_deps

This folder is made up only of BlueZ sources useful to build the project.
You just have to use the <em>Makefile</em> which is provided in this project.

In the <em>Makefile</em> set: 
```bash
BLUEZ_BUILD=false
``` 
Then you can run: 

```bash
$> make
``` 

By default, the path to the file to transfer is the folder next to the executable (<em>img</em> in this case)  but you can specify another path by modifying <code>file_transfer_path</code> in <em>src/file_transfer_task.c</em>.


### Build executable from BlueZ sources

#### Build BlueZ
Connect to a terminal on your Pi and download the source code of BlueZ: 
```bash
$> wget http://www.kernel.org/pub/linux/bluetooth/bluez-5.54.tar.xz
``` 

At the time of writing, the BlueZ version was 5.54. Maybe a new version is available, check it at <a href="http://www.bluez.org/">BlueZ official website</a>.

Then, to untar it, run: 
```bash
$> tar xvf bluez-5.54.tar.xz
``` 
Install Bluetooth dependencies:
```bash
$> sudo apt-get install -y libbluetooth-dev libusb-dev libdbus-1-dev libglib2.0-dev libudev-dev libical-dev libreadline-dev
```  
Compile & install Bluez:
```bash
$> ./configure --enable-library
$> make
$> sudo make install
``` 
Enable Bluetooth Low Energy Features:
<p>
You must modify the configuration file of the Bluetooth to enable BLE features. See BlueZ readme for more information.

```bash
$> sudo nano /lib/systemd/system/bluetooth.service
``` 

Replace ExecStart line by:
```bash
$> ExecStart=/usr/local/libexec/bluetooth/bluetoothd --experimental --noplugin=sap
``` 
Reaload configuration file: 
```bash
$> sudo systemctl daemon-reload
$> sudo systemctl restart bluetooth
``` 
#### Build the executable

In the <em>Makefile</em> set: 
```bash
BLUEZ_BUILD=true
``` 

Then, run: 

```bash
$> make
``` 

## SLATE106 configuration

The configuration of the SLATE106 must respect:
* Testcard deactivated
* WPAN1 authentication method to none
* Picture Filename: hub.pkk

If you want to be sure that you have the right configuration, you can use the configuration file <em>APPLI.CFG</em> in <em>slate106_config</em> folder.

## Remarks

### File transfer

* In the folder where you placed the file you want to transfer, there cannot be more than 7 files.
* You can modify the path where is the file to transfer. For that, you must specify the new path by modifying <code>file_transfer_path</code> in <em>src/file_transfer_task.c</em>. 

### Useful command  

If <em>"set scan parameters failed"</em> error append, run:
```bash
$> sudo hciconfig hci0 down
$> sudo hciconfig hci0 up
``` 

If the error is still present, run: 
```bash
$> sudo bluetoothctl
$> devices
``` 
If in the list of devices, there is your SLATE106 MAC address, run:
        
```bash
$> remove <MAC address>
``` 

## Todo

* Add pin code authentication