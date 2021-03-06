/*******************************************************************************
  Copyright(c) 2016 Radek Kaczorek  <rkaczorek AT gmail DOT com>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory>
#include <mcp23s17.h>

#include "piface_relay.h"

#define CHECK_BIT(var,pos) (((var)>>(pos)) & 1)

// We declare a pointer to IndiPiFaceRelay
// std::auto_ptr<IndiPiFaceRelay> indiPiFaceRelay(0);
std::unique_ptr<IndiPiFaceRelay> indiPiFaceRelay(new IndiPiFaceRelay);

void ISPoll(void *p);
void ISInit()
{
   static int isInit = 0;

   if (isInit == 1)
       return;

    isInit = 1;
    if(indiPiFaceRelay.get() == 0) indiPiFaceRelay.reset(new IndiPiFaceRelay());

}
void ISGetProperties(const char *dev)
{
        ISInit();
        indiPiFaceRelay->ISGetProperties(dev);
}
void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
        ISInit();
        indiPiFaceRelay->ISNewSwitch(dev, name, states, names, num);
}
void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
        ISInit();
        indiPiFaceRelay->ISNewText(dev, name, texts, names, num);
}
void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
        ISInit();
        indiPiFaceRelay->ISNewNumber(dev, name, values, names, num);
}
void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int num)
{
  INDI_UNUSED(dev);
  INDI_UNUSED(name);
  INDI_UNUSED(sizes);
  INDI_UNUSED(blobsizes);
  INDI_UNUSED(blobs);
  INDI_UNUSED(formats);
  INDI_UNUSED(names);
  INDI_UNUSED(num);
}
void ISSnoopDevice (XMLEle *root)
{
    ISInit();
    indiPiFaceRelay->ISSnoopDevice(root);
}
IndiPiFaceRelay::IndiPiFaceRelay()
{
	setVersion(1,0);
}
IndiPiFaceRelay::~IndiPiFaceRelay()
{
}
bool IndiPiFaceRelay::Connect()
{
	// open device
    mcp23s17_fd = mcp23s17_open(0, 0);

    if(mcp23s17_fd == -1)
	{
		IDMessage(getDeviceName(), "PiFace Relay device is not available.");
		return false;
	}

    // config register
    const uint8_t ioconfig = BANK_OFF | \
                             INT_MIRROR_OFF | \
                             SEQOP_OFF | \
                             DISSLW_OFF | \
                             HAEN_ON | \
                             ODR_OFF | \
                             INTPOL_LOW;
    mcp23s17_write_reg(ioconfig, IOCON, 0, mcp23s17_fd);

    // I/O direction
    mcp23s17_write_reg(0x00, IODIRA, 0, mcp23s17_fd);
    mcp23s17_write_reg(0x00, IODIRB, 0, mcp23s17_fd);

    // GPIOB pull ups
    mcp23s17_write_reg(0x00, GPPUB, 0, mcp23s17_fd);

	SetTimer(1000);

    IDMessage(getDeviceName(), "PiFace Relay connected successfully.");
    return true;
}
bool IndiPiFaceRelay::Disconnect()
{
	// close device
	close(mcp23s17_fd);

    IDMessage(getDeviceName(), "PiFace Relay disconnected successfully.");
    return true;
}
void IndiPiFaceRelay::TimerHit()
{
	if(isConnected())
	{
		// update gps time
		struct tm *local_timeinfo;
		static char ts[32];
		time_t rawtime;
		time(&rawtime);
		local_timeinfo = localtime (&rawtime);
		strftime(ts, 20, "%Y-%m-%dT%H:%M:%S", local_timeinfo);
		IUSaveText(&SysTimeT[0], ts);
		snprintf(ts, sizeof(ts), "%4.2f", (local_timeinfo->tm_gmtoff/3600.0));
		IUSaveText(&SysTimeT[1], ts);
		SysTimeTP.s = IPS_OK;
		IDSetText(&SysTimeTP, NULL);

		// update system info
		FILE* pipe;
		char buffer[128];

		// every 5 seconds
		if ( counter % 5 == 0 )
		{
			// reset system halt/restart button
			SwitchSP.s = IPS_IDLE;
			IDSetSwitch(&SwitchSP, NULL);
		}

		// every 10 secs
		if ( counter % 10 == 0 )
		{
			// system info
			SysInfoTP.s = IPS_BUSY;
			IDSetText(&SysInfoTP, NULL);

			//update Hardware
			pipe = popen("cat /proc/cpuinfo|grep Hardware|awk -F: '{print $2}'", "r");
			fgets(buffer, 128, pipe);
			pclose(pipe);
			IUSaveText(&SysInfoT[0], buffer);

			//update uptime
			pipe = popen("uptime|awk -F, '{print $1}'|awk -Fup '{print $2}'", "r");
			fgets(buffer, 128, pipe);
			pclose(pipe);
			IUSaveText(&SysInfoT[1], buffer);

			//update load
			pipe = popen("uptime|awk -F\", \" '{print $3\"/\"$4\"/\"$5}'|awk -F: '{print $2}'", "r");
			fgets(buffer, 128, pipe);
			pclose(pipe);
			IUSaveText(&SysInfoT[2], buffer);

			//update free mem
			pipe = popen("cat /proc/meminfo|grep MemFree|awk -F: '{print $2}'|sed -e 's/^[[:space:]]*//'", "r");
			fgets(buffer, 128, pipe);
			pclose(pipe);
			IUSaveText(&SysInfoT[3], buffer);

			//update temperature
			pipe = popen("vcgencmd measure_temp|awk -F= '{print $2}'", "r");
			fgets(buffer, 128, pipe);
			pclose(pipe);
			IUSaveText(&SysInfoT[4], buffer);

			SysInfoTP.s = IPS_OK;
			IDSetText(&SysInfoTP, NULL);

		}
		// every 60 secs
		if ( counter == 0 )
		{
			// network info
			NetInfoTP.s = IPS_BUSY;
			IDSetText(&NetInfoTP, NULL);

			//update Hostname
			pipe = popen("hostname", "r");
			fgets(buffer, 128, pipe);
			pclose(pipe);
			IUSaveText(&NetInfoT[0], buffer);

			//update Local IP
			pipe = popen("hostname -I", "r");
			fgets(buffer, 128, pipe);
			pclose(pipe);
			IUSaveText(&NetInfoT[1], buffer);

			//update Public IP
			pipe = popen("dig +short myip.opendns.com @resolver1.opendns.com", "r");
			fgets(buffer, 128, pipe);
			pclose(pipe);
			IUSaveText(&NetInfoT[2], buffer);

			NetInfoTP.s = IPS_OK;
			IDSetText(&NetInfoTP, NULL);

			counter = 60;
		}
		counter--;

		SetTimer(1000);
    }
}
const char * IndiPiFaceRelay::getDefaultName()
{
        return (char *)"PiFace Relay";
}
bool IndiPiFaceRelay::initProperties()
{
    // We init parent properties first
    INDI::DefaultDevice::initProperties();

	// system info
    IUFillText(&SysTimeT[0],"LOCAL_TIME","Local Time",NULL);
    IUFillText(&SysTimeT[1],"UTC_OFFSET","UTC Offset",NULL);
    IUFillTextVector(&SysTimeTP,SysTimeT,2,getDeviceName(),"SYSTEM_TIME","System Time","System Info",IP_RO,60,IPS_IDLE);

    IUFillText(&SysInfoT[0],"HARDWARE","Hardware",NULL);
    IUFillText(&SysInfoT[1],"UPTIME","Uptime (hh:mm)",NULL);
    IUFillText(&SysInfoT[2],"LOAD","Load (1/5/15 min.)",NULL);
    IUFillText(&SysInfoT[3],"MEM","Free Memory",NULL);
    IUFillText(&SysInfoT[4],"TEMP","System Temperature",NULL);
    IUFillTextVector(&SysInfoTP,SysInfoT,5,getDeviceName(),"SYSTEM_INFO","System Info","System Info",IP_RO,60,IPS_IDLE);

    IUFillText(&NetInfoT[0],"HOSTNAME","Hostname",NULL);
    IUFillText(&NetInfoT[1],"LOCAL_IP","Local IP",NULL);
    IUFillText(&NetInfoT[2],"PUBLIC_IP","Public IP",NULL);
    IUFillTextVector(&NetInfoTP,NetInfoT,3,getDeviceName(),"NETWORK_INFO","Network Info","System Info",IP_RO,60,IPS_IDLE);

	// main
    IUFillSwitch(&SwitchS[0], "ALL_ON", "All On", ISS_OFF);
    IUFillSwitch(&SwitchS[1], "ALL_OFF", "All Off", ISS_OFF);
    IUFillSwitch(&SwitchS[2], "SW0HALT", "Shutdown", ISS_OFF);
    IUFillSwitch(&SwitchS[3], "SW0REBOOT", "Restart", ISS_OFF);
    IUFillSwitchVector(&SwitchSP, SwitchS, 4, getDeviceName(), "SWITCH_0", "System", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&Relay1S[0], "REL1BTN", "On/Off", ISS_OFF);
    IUFillSwitchVector(&Relay1SP, Relay1S, 1, getDeviceName(), "RELAY1", "Relay 1", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&Relay2S[0], "REL2BTN", "On/Off", ISS_OFF);
    IUFillSwitchVector(&Relay2SP, Relay2S, 1, getDeviceName(), "RELAY2", "Relay 2", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&Relay3S[0], "REL3BTN", "On/Off", ISS_OFF);
    IUFillSwitchVector(&Relay3SP, Relay3S, 1, getDeviceName(), "RELAY3", "Relay 3", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&Relay4S[0], "REL4BTN", "On/Off", ISS_OFF);
    IUFillSwitchVector(&Relay4SP, Relay4S, 1, getDeviceName(), "RELAY4", "Relay 4", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

	// options
    IUFillText(&PortT[0], "PORT", "Port","PiFace Relay+");
    IUFillTextVector(&PortTP,PortT,1,getDeviceName(),"DEVICE_PORT","Ports",OPTIONS_TAB,IP_RO,0,IPS_OK);

	defineText(&PortTP);

    return true;
}
bool IndiPiFaceRelay::updateProperties()
{
    // Call parent update properties first
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
		defineText(&SysTimeTP);
		defineText(&SysInfoTP);
		defineText(&NetInfoTP);
		defineSwitch(&SwitchSP);
		defineSwitch(&Relay1SP);
		defineSwitch(&Relay2SP);
		defineSwitch(&Relay3SP);
		defineSwitch(&Relay4SP);

		LoadStates();
    }
    else
    {
		// We're disconnected
		deleteProperty(SysTimeTP.name);
		deleteProperty(SysInfoTP.name);
		deleteProperty(NetInfoTP.name);
		deleteProperty(SwitchSP.name);
		deleteProperty(Relay1SP.name);
		deleteProperty(Relay2SP.name);
		deleteProperty(Relay3SP.name);
		deleteProperty(Relay4SP.name);
    }
    return true;
}
void IndiPiFaceRelay::ISGetProperties(const char *dev)
{
    INDI::DefaultDevice::ISGetProperties(dev);

    /* Add debug controls so we may debug driver if necessary */
    //addDebugControl();
}
bool IndiPiFaceRelay::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
	return INDI::DefaultDevice::ISNewNumber(dev,name,values,names,n);
}
bool IndiPiFaceRelay::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
	// first we check if it's for our device
    if (!strcmp(dev, getDeviceName()))
    {
		// handle switch 0
		if (!strcmp(name, SwitchSP.name))
		{
			IUUpdateSwitch(&SwitchSP, states, names, n);

			if ( SwitchS[0].s == ISS_ON && SwitchSP.s == IPS_IDLE )
			{
				IDMessage(getDeviceName(), "All relays going ON. Click again to confirm.");
				SwitchS[0].s = ISS_OFF;
				SwitchSP.s = IPS_ALERT;
				IDSetSwitch(&SwitchSP, NULL);
				return true;
			}

			if ( SwitchS[1].s == ISS_ON && SwitchSP.s == IPS_IDLE )
			{
				IDMessage(getDeviceName(), "All relays going OFF. Click again to confirm.");
				SwitchS[1].s = ISS_OFF;
				SwitchSP.s = IPS_ALERT;
				IDSetSwitch(&SwitchSP, NULL);
				return true;
			}

			if ( SwitchS[2].s == ISS_ON && SwitchSP.s == IPS_IDLE )
			{
				IDMessage(getDeviceName(), "System is going to shutdown. Click again to confirm.");
				SwitchS[2].s = ISS_OFF;
				SwitchSP.s = IPS_ALERT;
				IDSetSwitch(&SwitchSP, NULL);
				return true;
			}

			if ( SwitchS[3].s == ISS_ON && SwitchSP.s == IPS_IDLE )
			{
				IDMessage(getDeviceName(), "System is going to restart. Click again to confirm.");
				SwitchS[3].s = ISS_OFF;
				SwitchSP.s = IPS_ALERT;
				IDSetSwitch(&SwitchSP, NULL);
				return true;
			}

			if ( SwitchS[0].s == ISS_ON && SwitchSP.s == IPS_ALERT )
			{
				SwitchSP.s = IPS_IDLE;
				IDSetSwitch(&SwitchSP, NULL);
				Relays(5);
				LoadStates();
				IDMessage(getDeviceName(), "All relays set ON");
				return true;
			}

			if ( SwitchS[1].s == ISS_ON && SwitchSP.s == IPS_ALERT )
			{
				SwitchSP.s = IPS_IDLE;
				IDSetSwitch(&SwitchSP, NULL);
				Relays(0);
				LoadStates();
				IDMessage(getDeviceName(), "All relays set OFF");
				return true;
			}

			if ( SwitchS[2].s == ISS_ON && SwitchSP.s == IPS_ALERT )
			{
				SwitchSP.s = IPS_IDLE;
				IDSetSwitch(&SwitchSP, NULL);
				IDMessage(getDeviceName(), "Halting system. Bye bye.");
				system("sudo shutdown -h now");
				return true;
			}

			if ( SwitchS[3].s == ISS_ON && SwitchSP.s == IPS_ALERT )
			{
				SwitchSP.s = IPS_IDLE;
				IDSetSwitch(&SwitchSP, NULL);
				IDMessage(getDeviceName(), "Restarting system. See you soon.");
				system("sudo shutdown -r now");
				return true;
			}

		}

		// handle relays
		if (!strcmp(name, Relay1SP.name))
		{
			IUUpdateSwitch(&Relay1SP, states, names, n);

			if ( Relay1S[0].s)
			{
				// click	
				Relay1SP.s = IPS_OK;
				IDSetSwitch(&Relay1SP, NULL);
				Relays(1);
				Relay1S[0].s = RelayState(1) ? RelayState(1) : ISS_OFF;
				IDMessage(getDeviceName(), "PiFace Relay Relay 1: %s", Relay1S[0].s == ISS_ON ? "ON" : "OFF" );
				Relay1SP.s = IPS_IDLE;
				if(Relay1S[0].s == ISS_ON)
					Relay1SP.s = IPS_OK;
				IDSetSwitch(&Relay1SP, NULL);
				return true;
			}
		}
		if (!strcmp(name, Relay2SP.name))
		{
			IUUpdateSwitch(&Relay2SP, states, names, n);

			if ( Relay2S[0].s)
			{
				// click	
				Relay1SP.s = IPS_OK;
				IDSetSwitch(&Relay2SP, NULL);
				Relays(2);
				Relay2S[0].s = RelayState(2) ? RelayState(2) : ISS_OFF;
				IDMessage(getDeviceName(), "PiFace Relay Relay 2: %s", Relay2S[0].s == ISS_ON ? "ON" : "OFF" );
				Relay2SP.s = IPS_IDLE;
				if(Relay2S[0].s == ISS_ON)
					Relay2SP.s = IPS_OK;
				IDSetSwitch(&Relay2SP, NULL);
				return true;
			}
		}
		if (!strcmp(name, Relay3SP.name))
		{
			IUUpdateSwitch(&Relay3SP, states, names, n);

			if ( Relay3S[0].s)
			{
				// click	
				Relay1SP.s = IPS_OK;
				IDSetSwitch(&Relay3SP, NULL);
				Relays(3);
				Relay3S[0].s = RelayState(3) ? RelayState(3) : ISS_OFF;
				IDMessage(getDeviceName(), "PiFace Relay Relay 3: %s", Relay3S[0].s == ISS_ON ? "ON" : "OFF" );
				Relay3SP.s = IPS_IDLE;
				if(Relay3S[0].s == ISS_ON)
					Relay3SP.s = IPS_OK;
				IDSetSwitch(&Relay3SP, NULL);
				return true;
			}
		}
		if (!strcmp(name, Relay4SP.name))
		{
			IUUpdateSwitch(&Relay4SP, states, names, n);

			if ( Relay4S[0].s)
			{
				// click
				Relay1SP.s = IPS_OK;
				IDSetSwitch(&Relay4SP, NULL);
				Relays(4);
				Relay4S[0].s = RelayState(4) ? RelayState(4) : ISS_OFF;
				IDMessage(getDeviceName(), "PiFace Relay Relay 4: %s", Relay4S[0].s == ISS_ON ? "ON" : "OFF" );
				Relay4SP.s = IPS_IDLE;
				if(Relay4S[0].s == ISS_ON)
					Relay4SP.s = IPS_OK;
				IDSetSwitch(&Relay4SP, NULL);
				return true;
			}
		}
	}
	return INDI::DefaultDevice::ISNewSwitch (dev, name, states, names, n);
}
bool IndiPiFaceRelay::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
	return INDI::DefaultDevice::ISNewText (dev, name, texts, names, n);
}
bool IndiPiFaceRelay::ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
	return INDI::DefaultDevice::ISNewBLOB (dev, name, sizes, blobsizes, blobs, formats, names, n);
}
bool IndiPiFaceRelay::ISSnoopDevice(XMLEle *root)
{
    return INDI::DefaultDevice::ISSnoopDevice(root);
}
bool IndiPiFaceRelay::saveConfigItems(FILE *fp)
{
	IUSaveConfigSwitch(fp, &Relay1SP);
	IUSaveConfigSwitch(fp, &Relay2SP);
	IUSaveConfigSwitch(fp, &Relay3SP);
	IUSaveConfigSwitch(fp, &Relay4SP);

    return true;
}
int IndiPiFaceRelay::Relays(int index)
{
    if (index < 0 || index > 5)
       return 1;

    int value;

    //read states
    uint8_t payload = mcp23s17_read_reg(GPIOA, 0, mcp23s17_fd);

    switch(index)
    {
	case 0:
	  value = 0x00;
	  break;
    case 1:
	  value = 0x01;
	  break;
	case 2:
	  value = 0x02;
	  break;
	case 3:
	  value = 0x04;
	  break;
	case 4:
	  value = 0x08;
	  break;
	case 5:
	  value = 0xff;
	  break;
	default:
	  value = 0x00;
    }

    // Handle all on and off
    if( value == 0x00 || value == 0xff )
    {
      payload = value;
    } else
    {
      payload = value ^ payload;
    }

    // Write to GPIO Port A
    mcp23s17_write_reg(payload, GPIOA, 0, mcp23s17_fd);
}
ISState IndiPiFaceRelay::RelayState(int index)
{
	ISState state;

    //read states
    uint8_t relays = mcp23s17_read_reg(GPIOA, 0, mcp23s17_fd);

	if(CHECK_BIT(relays,index-1) == 1)
	{
		state = ISS_ON;
	} else
	{
		state = ISS_OFF;
	}

	return state;
}
void IndiPiFaceRelay::LoadStates()
{
	// load states
	Relay1S[0].s = RelayState(1);
	Relay1SP.s = IPS_IDLE;
	if(Relay1S[0].s == ISS_ON)
		Relay1SP.s = IPS_OK;
	IDSetSwitch(&Relay1SP, NULL);
	
	Relay2S[0].s = RelayState(2);
	Relay2SP.s = IPS_IDLE;
	if(Relay2S[0].s == ISS_ON)
		Relay2SP.s = IPS_OK;
	IDSetSwitch(&Relay2SP, NULL);
	
	Relay3S[0].s = RelayState(3);
	Relay3SP.s = IPS_IDLE;
	if(Relay3S[0].s == ISS_ON)
		Relay3SP.s = IPS_OK;
	IDSetSwitch(&Relay3SP, NULL);
	
	Relay4S[0].s = RelayState(4);
	Relay4SP.s = IPS_IDLE;
	if(Relay4S[0].s == ISS_ON)
		Relay4SP.s = IPS_OK;
	IDSetSwitch(&Relay4SP, NULL);
}
