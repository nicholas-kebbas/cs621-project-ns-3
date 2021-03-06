# Point-to-Point Compression Link
> This application will allow the detection of data compression.

This project implements NS-3 to test an application that allows server-client end points to detect compression without having to deal with IP-layer level compression detection.

## Migrating Code into NS-3

Pull the entire repo into a new folder named project1.  Now go to your NS-3 installation folder. From here you will go to the /src folder and place the newly created repo folder into it. For example, it would look something like this ```/ns-3.29/src/project1```, where /ns-3.29 is our current installation of NS-3.

The ```nlohmann``` folder must be copied into the NS-3 root folder, where ```src``` is. The ```config.json``` and ```randomfile``` file must also be moved to the NS-3 root. 

The four ```point-to-point...``` files in ```/project1/model``` must be copied into ```src/point-to-point/model```.

To use the actual application, copy over the ```project1.cc``` file over to ns-3.29/scratch. This is where you can run the application without rebuilding all the examples every time.

If you would like to add new files to the application, models will go in the model folder, examples in the example folder, etc... To ensure these get built correctly, make sure to edit the corresponding `wscript` files. New examples require the wscript in /examples to be updated. These files are where you can tell waf what modules, classes, etc. your application is going to be using. When waf builds ns3 again, your module will know what other headers it needs to include when running. 

## Building the Code

Now that all the files are copied to the right locations, go to the root of your ns3 installation (like ns-3.29 from earlier) and do


```
./waf configure -d optimized --enable-examples --enable-tests
```

This lets waf configure any example and tests you would want to use. Enabling examples is crucial to running your applications from the folder.

Next, run the following command to build the new applications. This will allow NS-3 to properly configure and link the applications with the existing ones originally included.

```
./waf build
```

To run a module, simply do a command like the following: 

```
./waf --run <module name>
```

Additionally, you some applications allow you to input additional parameters into the command line to change the way it runs. For this program this command would be the following:

```
./waf --run "udp-app --maxBandwidth=8 --compressionEnabled=true"
```

This command  will let the UDP application run with compression enabled, and it will set the PointToPointNetDevice's Data Rate to 8Mbps. 

## Compression/Decompression

ZLib Compression
https://www.zlib.net/manual.html

To install zlib run:
```
sudo apt-get install zlib1g-dev
```
It's important to note that if you get an "undefined reference error" during the linking phase while waf is building, that you add the line 

```
module.use.append("ZLIB1G")
```
to the def build: method of the wscript file in whatever module that is failing.
You can place it right above 
```
    if (bld.env['ENABLE_EXAMPLES']):
        bld.recurse('examples')
```
This ensures that the zlib functions are recognized and built correctly.

We're using zlib's implementaton of deflate in the point to point net device class to compress the packets before sending them. We're then using inflate once the packet has been received in order to return the packet to its previous state.   

Deflate decompresses as much data as possible, and stops when the input buffer becomes empty or the output buffer becomes full. It may introduce some output latency (reading input without producing any output) except when forced to flush. Inflate does the opposite, and returns the original data of the packet.

