#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <usb.h>
#include <errno.h>
#include <sys/time.h>

#include <fcntl.h>
#include <termios.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <time.h>

#include "simple_sockets.hpp"
#include "sensor_aggregator_protocol.hpp"

#include <iostream>
#include <string>
#include <list>
#include <map>
#include <algorithm>
#include <stdexcept>

//Handle interrupt signals to exit cleanly.
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

using std::string;
using std::list;
using std::map;
using std::pair;

#define MAX_PACKET_SIZE_READ		(64 *1024 )

//Global variable for the signal handler.
bool killed = false;
unsigned int ExitCounter = 0;

#define LM_GET_NEXT_PACKET (0x13)

#define RSSI_OFFSET 78
#define CRC_OK 0x80

//PIP 3 Byte ID packet structure with variable data segment.
//3 Byte receiver ID, 21 bit transmitter id, 3 bits of parity plus up to 20 bytes of extra data.
typedef struct {
	unsigned char ex_length : 8; //Length of data in the optional data portion
	unsigned char dropped   : 8; //The number of packet that were dropped if the queue overflowed.
	unsigned int boardID    : 24;//Basestation ID
	unsigned int time       : 32;//Timestamp in units of 4 milliseconds.
	unsigned int tagID      : 24;//Transmitter ID
	unsigned char rssi      : 8; //Received signal strength indicator
	unsigned char status    : 7; //The lower 7 bits contain the link quality indicator
	unsigned char crcok	: 1; // The CRC_OK bit
	unsigned char data[20];      //The optional variable length data segment
} __attribute__((packed)) pip_packet_t;

//USB PIPs' vendor ID and strings
const char *silicon_labs_s = "Silicon Labs\0";
const char *serial_num_s = "1234\0";
const int PACKET_LEN = 13;

//Map of usb devices in use, accessed by the USB device number
map<u_int8_t, bool> in_use;
//0 for 2.X tags, 1 for GPIP
map<usb_dev_handle*, uint8_t> versions;

#define SILICON_LABS_VENDOR  ((unsigned short) (0x10c4))
#define SILICON_LABS_PIPPROD ((unsigned char) (0x03))

#define TI_LABS_VENDOR  ((unsigned short) (0x2047))
#define TI_LABS_PIPPROD ((unsigned short) (0x0300))

void attachPIPs(list<usb_dev_handle*> &pip_devs) 
{
  struct usb_bus *bus = NULL; 
  struct usb_device *dev = NULL;

  /* Slot numbers used to differentiate multiple PIP USB connections. */
  int slot = 0;

  /* these loops crawl the whole USB tree */
  for (bus = usb_busses; bus != NULL; bus = bus->next) {
    for (dev = bus->devices; dev != NULL; dev = dev->next) {
      uint8_t version = 0;

      int found_manu, found_serial, found_prod;
      found_manu = found_prod = found_serial = 0;

      if ((unsigned short) dev->descriptor.idVendor ==  (unsigned short) TI_LABS_VENDOR)
      {
	  found_manu = 1;
      	if ((unsigned short) dev->descriptor.idProduct == (unsigned short) TI_LABS_PIPPROD)
        {	
	   found_prod = 1; 
	   version = 1;
	}else{/* ignore */}
      }
      else if ((unsigned short) dev->descriptor.idVendor ==  (unsigned short) SILICON_LABS_VENDOR)
      {
	found_manu = 1;      	
	if ((unsigned short) dev->descriptor.idProduct == (unsigned short) SILICON_LABS_PIPPROD)
        {
	    found_prod = 1; 
	    version = 0;
	}

      }else{/* Ignore */}

      //If this is a pipsqueak device that is not already opened try opening it.
      if ( (found_manu == 1) && (found_prod == 1) && not in_use[dev->devnum] ) {
        	++slot;
        	//std::cerr<<"Connected to USB Tag Reader.\n";
        	usb_dev_handle* new_handle = usb_open(dev);

        	if (!new_handle) {
          	std::cout<<"Failed to open pipsqueak.\n";
        	}
        	else {
          //Add the new device to the pip device list.
          pip_devs.push_back(new_handle);
          std::cout<<"New pipsqueak opened.\n";
          in_use[dev->devnum] = true;
          versions[new_handle] = version;

          int retval = usb_set_configuration(pip_devs.back(), 1);
          if (retval < 0 ) { 
            printf("Setting configuration to 1 failed %d \n",retval);
          }

          //Retry claiming the device up to two times.
          int retries = 2;

          int interface_num = 0;
          while ((retval = usb_claim_interface(pip_devs.back(), interface_num)) && retries-- > 0) {
            if (retval == -ENOMEM) {
              std::cerr<<"usb_claim_interface failed try "<<retries<<": -ENOMEM\n";
              std::cerr<<"This program is being run without permission to open usb devices - aborting.\n";
              retries = 0;
              //This failure indicates that we do not have permission to open the usb device.
              return;
            } else if (retval == -EBUSY) {
#if LIBUSB_HAS_GET_DRIVER_NP
              char drivername[256];
              if (usb_get_driver_np(new_handle, 0, drivername, sizeof(drivername))) {
                std::cerr<<"usb_get_driver_np failed\n";
                retries = 0;
              }
              else {
                std::cerr<<"kernel driver '"<<drivername<<"' is bound to interface 0\n";
#else
                std::cerr<<"kernel driver is bound to interface 0\n";
#endif /* LIBUSB_HAS_GET_DRIVER_NP */

#if LIBUSB_HAS_DETACH_KERNEL_DRIVER_NP
                if (usb_detach_kernel_driver_np(new_handle, 0)) {
                  std::cerr<<"usb_detach_kernel_driver_np failed\n";
                  retries = 0;
                }
                std::cerr<<"kernel driver successfully detached\n";
#else
                retries = 0;
#endif /* LIBUSB_HAS_DETACH_KERNEL_DRIVER_NP */
#if LIBUSB_HAS_GET_DRIVER_NP
              }
#endif /* LIBUSB_HAS_GET_DRIVER_NP */
            } else {
              std::cerr<<"usb_claim_interface failed: "<<retval<<" tries "<<retries<<"\n";
            }
          }
        }
      }
      if (!dev->config) { 
        std::cout<<"Couldn't retrieve descriptors\n"; 
      }
    }
  }
}


int main(int ac, char** arg_vector) 
{

  //std::cerr<<"parameters are ac:"<<ac<<std::endl;
  float min_rss = -600.0;

  //Now connect to pip devices and send their packet data to the aggregation server.
  unsigned char msg[128];
  char* signed_msg = (char*)msg;
  unsigned char buf[MAX_PACKET_SIZE_READ];
  char* signed_buf = (char*)buf;
  list<usb_dev_handle*> pip_devs;

  //Set up the USB
  usb_init(); 
  usb_find_busses(); 
  usb_find_devices(); 

  //Attach new pip devices.
  attachPIPs(pip_devs);

	while (not killed) 
	{
	    bool connected = false;
	    long long int lastReportTime = 0;
	    int numPktsRcvd = 0;
	    int numGoodPktsRcvd = 0;
	    int checkUSB = 0;

    	    //A try/catch block is set up to handle exception during quitting.
			   try
			   {
				  while (not killed)
				   {
					 if(checkUSB == 0)
					 {
						//Check for new USB devices by checking if new devices were added or force a check every 3 seconds.
						 if (0 < usb_find_busses() + usb_find_devices())
						 {
							attachPIPs(pip_devs);
							//Remove any duplicate devices
							pip_devs.sort();
							pip_devs.unique();
						 }
						checkUSB=10000;
					 }
					--checkUSB;
        				if (pip_devs.size() > 0) 
						{
							for (list<usb_dev_handle*>::iterator I = pip_devs.begin(); I != pip_devs.end(); ++I) {
		    					//A pip can fail up to two times in a row if this is the first time querying it.
		    					//If the pip fails after three retries then this pip usb_dev_handle is no longer
		    					//valid, probably because the pip was removed from the USB.
		    					int retries_left = 3;
		    					int retval = -1;
		    					while (retval < 0 and retries_left > 0) 
							{
			      					// Request the next packet from the pip
			      					msg[0] = LM_GET_NEXT_PACKET;	      
			      					retval = usb_bulk_write(*I, 2, signed_msg, 1, 100); 
			      					memset(buf, 0, MAX_PACKET_SIZE_READ);	  

			      					//Allow up to 20 extra bytes of sensor data beyond the normal packet length.
			      					if(0 == versions[*I])              
				 				retval = usb_bulk_read(*I, 0x81, signed_buf+1, PACKET_LEN+20, 100);
			      					else
				 				retval = usb_bulk_read(*I, 0x82, signed_buf+1, PACKET_LEN+20, 100);
			      					//Fill in the length of the extra portion of the packet
			      					signed_buf[0] = retval - PACKET_LEN;
			      					--retries_left;
		    					}
            						//If the pip fails 3 times in a row then it was probably disconnected.
							if (retval < 0) 
							{
							      usb_reset(*I);
							      usb_close(*I); 
							      *I = NULL;
							}
            						//If the length of the message is equal to or greater than PACKET_LEN then this is a data packet.
							else if (PACKET_LEN <= retval)
							{
								//Overlay the packet struct on top of the pointer to the pip's message.
								pip_packet_t *pkt = (pip_packet_t *)buf;
								++numPktsRcvd;

								//Check to make sure this was a good packet.
								if ((pkt->rssi != (int) 0) and (pkt->status != 0))
								{
									unsigned char* data = (unsigned char*)pkt;
									if(pkt->crcok)
									{
										++numGoodPktsRcvd;
									}

									//Even parity check
									bool parity_failed = false;
									if (not parity_failed)
									{
											  //Now assemble a sample data variable and send it to the aggregation server.
											  SampleData sd;
											  //Calculate the tagID here instead of using be32toh since it is awkward to convert a
											  //21 bit integer to 32 bits. Multiply by 8192 and 32 instead of shifting by 13 and 5
											  //bits respectively to avoid endian issues with bit shifting.
											  unsigned int netID = ((unsigned int)data[9] * 65536)  + ((unsigned int)data[10] * 256) +
											  ((unsigned int)data[11] );
											  //We do not currently use the pip's local timestamp
											  //unsigned long time = ntohl(pkt->time);
											  unsigned long baseID = ntohl(pkt->boardID << 8);

											  //The physical layer of a pipsqueak device is 1
											  sd.physical_layer = 1;
											  sd.tx_id = netID;
											  sd.rx_id = baseID;
											  //Set this to the real timestamp, milliseconds since 1970
											  timeval tval;
											  gettimeofday(&tval, NULL);
											  sd.rx_timestamp = tval.tv_sec*1000 + tval.tv_usec/1000;
											  long long int unix_time = tval.tv_sec*1000 + tval.tv_usec/1000;
											  //Convert from one byte value to a float for receive signal
											  //strength as described in the TI/chipcon Design Note DN505 on cc1100
											  sd.rss = ( (pkt->rssi) >= 128 ? (signed int)(pkt->rssi-256)/2.0 : (pkt->rssi)/2.0) - RSSI_OFFSET;
											  sd.sense_data = std::vector<unsigned char>(pkt->data, pkt->data+signed_buf[0]);
											  sd.valid = true;
											    


												
											  signed_buf[1] = (unsigned char)signed_buf[1];

											  printf("TX:%05d: ",netID);		

											  int data_temp =  (unsigned int) sd.sense_data[2]*256+(unsigned int) sd.sense_data[3];
											  int data_humidity =  (unsigned int) sd.sense_data[4]*256+(unsigned int) sd.sense_data[5];
											  //temperature
											  printf(" temp: %.2f", ((float)data_temp)/10);

											  //humility
											  printf(" humidity: %d", data_humidity);
											  printf("\n");	

											 ExitCounter++;
											 if(ExitCounter > 3)
											 {
											 //Clean up the pip connections before exiting.
   												for (list<usb_dev_handle*>::iterator I = pip_devs.begin(); I != pip_devs.end(); ++I) 
	 											{
    											    usb_reset(*I);
    												usb_close (*I); 
   												}
												
												exit(0);							
											 }
									}
								}

							}
		  }
		                }
		//Clear dead connections
		pip_devs.remove(NULL);
	  }
	 }
	 catch (std::runtime_error& re) {
	  std::cerr<<"USB sensor layer error: "<<re.what()<<'\n';
    }
    catch (std::exception& e) {
      std::cerr<<"USB sensor layer error: "<<e.what()<<'\n';
    }
    //Try to reconnect to the server after losing the connection.
    //Sleep a little bit, then try connecting to the server again.
    usleep(1000);
   }
   std::cerr<<"Exiting\n";
   //Clean up the pip connections before exiting.
   for (list<usb_dev_handle*>::iterator I = pip_devs.begin(); I != pip_devs.end(); ++I) 
	 {
    usb_reset(*I);
    usb_close (*I); 
   }
}


