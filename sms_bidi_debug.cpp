#include "mbed.h"
 
//------------------------------------------------------------------------------------
/* This example was tested on C027-U20 and C027-G35 with the on board modem. 
   
   Additionally it was tested with a shield where the SARA-G350/U260/U270 RX/TX/PWRON 
   is connected to D0/D1/D4 and the GPS SCL/SDA is connected D15/D15. In this 
   configuration the following platforms were tested (it is likely that others 
   will work as well)
   - U-BLOX:    C027-G35, C027-U20, C027-C20 (for shield set define C027_FORCE_SHIELD)
   - NXP:       LPC1549v2, LPC4088qsb
   - Freescale: FRDM-KL05Z, FRDM-KL25Z, FRDM-KL46Z, FRDM-K64F
   - STM:       NUCLEO-F401RE, NUCLEO-F030R8
                mount resistors SB13/14 1k, SB62/63 0R
*/
#include "GPS.h"
#include "MDM.h"
//------------------------------------------------------------------------------------
// You need to configure these cellular modem / SIM parameters.
// These parameters are ignored for LISA-C200 variants and can be left NULL.
//------------------------------------------------------------------------------------
//! Set your secret SIM pin here (e.g. "1234"). Check your SIM manual.
#define SIMPIN      NULL
/*! The APN of your network operator SIM, sometimes it is "internet" check your 
    contract with the network operator. You can also try to look-up your settings in 
    google: https://www.google.de/search?q=APN+list */
#define APN         free
//! Set the user name for your APN, or NULL if not needed
#define USERNAME    NULL
//! Set the password for your APN, or NULL if not needed
#define PASSWORD    NULL
//------------------------------------------------------------------------------------
 
//#define CELLOCATE
 
int main(void)
{
    int ret;
#ifdef LARGE_DATA
    char buf[2048] = "";
#else
    char buf[512] = "";
#endif

     GPSSerial gps; 
        // Create the modem object
    MDMSerial mdm;
    //mdm.setDebug(4); // enable this for debugging issues 
    // initialize the modem 
    MDMParser::DevStatus devStatus = {};
    MDMParser::NetStatus netStatus = {};
    bool mdmOk = mdm.init(SIMPIN, &devStatus);
    mdm.dumpDevStatus(&devStatus);
    
    printf("SMS and GPS Loop\r\n");
    char link[128] = "";
    unsigned int i = 0xFFFFFFFF;
    const int wait = 100;
    bool abort = false;
    
    while(!abort)
    {
                while ((ret = gps.getMessage(buf, sizeof(buf))) > 0)
        {
            int len = LENGTH(ret);
            //printf("NMEA: %.*s\r\n", len-2, msg); 
            if ((PROTOCOL(ret) == GPSParser::NMEA) && (len > 6))
            {
                // talker is $GA=Galileo $GB=Beidou $GL=Glonass $GN=Combined $GP=GPS
                if ((buf[0] == '$') || buf[1] == 'G') {
                    #define _CHECK_TALKER(s) ((buf[3] == s[0]) && (buf[4] == s[1]) && (buf[5] == s[2]))
                    if (_CHECK_TALKER("GLL")) {
                        double la = 0, lo = 0;
                        char ch;
                        if (gps.getNmeaAngle(1,buf,len,la) && 
                            gps.getNmeaAngle(3,buf,len,lo) && 
                            gps.getNmeaItem(6,buf,len,ch) && ch == 'A')
                        {
                            printf("GPS Location: %.5f %.5f\r\n", la, lo); 
                            sprintf(link, "I am here!\n"
                                          "https://maps.google.com/?q=%.5f,%.5f", la, lo); 
                        }
                    } else if (_CHECK_TALKER("GGA") || _CHECK_TALKER("GNS") ) {
                        double a = 0; 
                        if (gps.getNmeaItem(9,buf,len,a)) // altitude msl [m]
                            printf("GPS Altitude: %.1f\r\n", a); 
                    } else if (_CHECK_TALKER("VTG")) {
                        double s = 0; 
                        if (gps.getNmeaItem(7,buf,len,s)) // speed [km/h]
                            printf("GPS Speed: %.1f\r\n", s); 
                    }
                }
            }
        }
        
        if (mdmOk && (i++ == 5000/wait)) {
            i = 0;
            // check the network status
            if (mdm.checkNetStatus(&netStatus)) {
                mdm.dumpNetStatus(&netStatus, fprintf, stdout);
            }
                
            // checking unread sms
            int ix[8];
            int n = mdm.smsList("REC UNREAD", ix, 8);
            if (8 < n) n = 8;
            while (0 < n--)
            {
                char num[32];
                printf("Unread SMS at index %d\r\n", ix[n]);
                if (mdm.smsRead(ix[n], num, buf, sizeof(buf))) {
                    printf("Got SMS from \"%s\" with text \"%s\"\r\n", num, buf);
                    printf("Delete SMS at index %d\r\n", ix[n]);
                    mdm.smsDelete(ix[n]);
                    // provide a reply
                    const char* reply = "Hello my friend";
                    if (strstr(buf, /*w*/"here are you"))
                        reply = *link ? link : "I don't know"; // reply wil location link
                    else if (strstr(buf, /*s*/"hutdown"))
                        abort = true, reply = "bye bye";
                    printf("Send SMS reply \"%s\" to \"%s\"\r\n", reply, num);
                    mdm.smsSend(num, reply);
                }
            }
        }
        wait_ms(wait);
    }
	
    gps.powerOff();
    mdm.powerOff();
    return 0;
}