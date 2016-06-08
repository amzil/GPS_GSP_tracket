#include "mbed.h"
#include "GPS.h"
#include "MDM.h"
#include "string.h"
#include "HTTPClient.h"

//------------------------------------------------------------------------------------
 
//#define CELLOCATE
 
//Fonction permet de extraire les donnais envoyer par SMS en mode configuration
//si le message envoyer et Password:1234 la fonction permet d'extraire le nouveau mots de passe et le sauvegarder
void extrat(char str2[],char str1[]);
// Cette focntion permet d'executer le methode http get pour envoyer la position ainsi que d'autre option au serveur traccar  
void traccar_send(char domaine[],char port[],char deviceId[],double lat,double lon,double altit,double vitesse);

char str[2048];

//cree objet http 
HTTPClient http;
    
int main(void)
{
    int ret;
#ifdef LARGE_DATA
    char buf[2048] = "";
#else
    char buf[512] = "";
#endif
    char admin[35]="+33629332783";
    char interbuff[sizeof(buf)];
    bool modeConfig=false;
    char password[10]="0000";
    char verifpassword[]="";
    char nomAppareil[20]="654325";
    char serverAdresse[40]="demo.traccar.org";
    char port[]="5055";
    bool traccar=false;
    char apn[10]="free";
    char simpin[5]="NULL";
    char username_sim[20]="NULL";
    char password_sim[20]="NULL";
    char charTempsSyncorPosi[10];
    int IntTempsSyncorPosi;
    double la = 0, lo = 0;   double a = 0; double s = 0; 
    
//cree objet gps 
     GPSSerial gps; 
// Cree objet modem
    MDMSerial mdm;

    // initialer et connecté le modem
   bool mdmOk = mdm.connect(simpin, apn,username_sim,password_sim);
    MDMParser::DevStatus devStatus = {};
    MDMParser::NetStatus netStatus = {};
    mdm.dumpDevStatus(&devStatus);
    
    printf("Boucle SMS et GPS \r\n");
    char link[128] = "";
    unsigned int i = 0xFFFFFFFF;
    const int wait = 100;
    bool activer = false;

    while(!activer)
    {
       if (traccar==true)
       {
         printf("traccar activer\r\n");
         traccar_send(serverAdresse,port,nomAppareil,la,lo,a,s);
       }
                // Si un message recu du modele GPS extraire les trames GNS, VTG, GGA,       
        while ((ret = gps.getMessage(buf, sizeof(buf))) > 0)
        {
            int len = LENGTH(ret);
            if ((PROTOCOL(ret) == GPSParser::NMEA) && (len > 6))
            {
                // Les trames a extaire son du type $GA=Galileo ($GB=Beidou $GL=Glonass $GN=Combined $GP=GPS)
                if ((buf[0] == '$') || buf[1] == 'G') {
                    #define _CHECK_TALKER(s) ((buf[3] == s[0]) && (buf[4] == s[1]) && (buf[5] == s[2]))
                    if (_CHECK_TALKER("GLL")) {
                        
                        char ch;
                        if (gps.getNmeaAngle(1,buf,len,la) && 
                            gps.getNmeaAngle(3,buf,len,lo) && 
                            gps.getNmeaItem(6,buf,len,ch) && ch == 'A')
                        {
                            printf("GLocation PS : %.5f %.5f\r\n", la, lo); 
                            sprintf(link, "Je suis ICI!\n"
                                          "https://maps.google.com/?q=%.5f,%.5f", la, lo); 
                        }
                    } else if (_CHECK_TALKER("GGA") || _CHECK_TALKER("GNS") ) {

                        if (gps.getNmeaItem(9,buf,len,a)) // altitude msl [m]
                            printf("GPS Altitude: %.1f\r\n", a); 
                    } else if (_CHECK_TALKER("VTG")) {
                        
                        if (gps.getNmeaItem(7,buf,len,s)) // vitesse [km/h]
                            printf("GPS Speed: %.1f\r\n", s); 
                    }
                }
            }
        }


        if (mdmOk && (i++ == 5000/wait)) {
            i = 0;

            // Verifier l'etat du reseau GMS
            if (mdm.checkNetStatus(&netStatus)) {
                mdm.dumpNetStatus(&netStatus, fprintf, stdout);
            }
            // Verifier les SMS nonlu
            int ix[8];
            int n = mdm.smsList("REC UNREAD", ix, 8);
            if (8 < n) n = 8;
            printf("check avant while message non lut  %d\r\n", n);
            while (0 < n--)
            {
                char num[32];
                printf("Unread SMS at index %d\r\n", ix[n]);
                if (mdm.smsRead(ix[n], num, buf, sizeof(buf))) {
                    printf("Got SMS from \"%s\" with text \"%s\"\r\n", num, buf);
                    printf("Delete SMS at index %d\r\n", ix[n]);
                    mdm.smsDelete(ix[n]);
                    // inistaliser repence
                    const char* repense="ERREUR 0 buffer vide";

                    strcpy(interbuff,buf);
                    printf("Buf %s: \r\n", buf);
                    printf("interbuff %s: \r\n", interbuff);
                    
                        if((!strncmp(interbuff, "Config:", 7)) && modeConfig==false )
                        {
                            //interbuff+=6;//extrer password et ecraser l'entete "config:"  
                            extrat(verifpassword,interbuff);                  
                            if (!strcmp(verifpassword, password))//verification password de 6 caractaire
                            {
                                modeConfig=true;
                                printf(" verifpassword %s\r\n",verifpassword);
                                repense="Tu es en mode configuration !!";
                            }
                            else printf("Erreur de verification de password \r\n");
                        }   
                        else if(modeConfig)
                        {       // Configuration de l'appareille
                                if(!strncmp(interbuff, "Password:", 9))
                                {
                                    extrat(password,interbuff);
                                    repense = password;
                                }
                                else if (!strncmp(interbuff, "Admin:", 6))
                                {
                                    extrat(admin,interbuff);
                                    repense=admin;
                                }
                                else if (!strncmp(interbuff, "Device:", 7))
                                {
                                    extrat(nomAppareil,interbuff);
                                    repense=nomAppareil;
                                }
                                // Configuration traccar Server 
                                else if (!strncmp(interbuff, "Serveur:", 7))
                                {
                                    extrat(serverAdresse,interbuff);
                                    repense=serverAdresse;
                                }
                                else if (!strncmp(interbuff, "Port:", 5))
                                {
                                    extrat(port,interbuff);
                                    repense=port;
                                }
                                else if (!strncmp(interbuff, "TimeP:", 6))
                                {
                                    extrat(charTempsSyncorPosi,interbuff);   
                                    sscanf(charTempsSyncorPosi,"%d",&IntTempsSyncorPosi);
                                    repense="charTempsSyncroPosi";
                                }
                                // Configuuration Connection GPRS
                                else if (!strncmp(interbuff, "APN:", 4))
                                {
                                    extrat(apn,interbuff);   
                                    repense=apn;
                                }
                                else if (!strncmp(interbuff, "SIMPIN:", 7))
                                {
                                    extrat(simpin,interbuff);
                                    repense=simpin;
                                }
                                else if (!strncmp(interbuff, "USERNAME:", 9))
                                {
                                    extrat(username_sim,interbuff);
                                    repense=username_sim;
                                }
                                else if (!strncmp(interbuff, "PASSWORD:", 9))
                                {
                                    extrat(password_sim,interbuff);
                                    repense=password_sim;
                                }
                                else if (strstr(interbuff, /*e*/"xit"))
                                {
                                    modeConfig=false;
                                    repense="Exit mode config";
                                }
                                else 
                                {
                                printf("erreur mode config CMD non valide !!/r/n");
                                }     
                        }
                        else if(strstr(interbuff, /*p*/"osition"))
                        {
                        repense = *link ? link : "Je n'est pas recu de postion !! je ne sais pas je suis ou!! "; // repense echec de localisation
                        }
                        else if (strstr(interbuff, /*s*/"hutdown"))
                        activer = true, repense = "bye bye";
                        else if (strstr(interbuff, /*A*/"ctiver_traccar"))
                            {
                                traccar=true;
                                repense = "Active le mode d'envoi de la position au server Traccar";
                            
                            }
                        else if (strstr(interbuff, /*d*/"esactiver_traccar"))
                        {
                            traccar=false;
                           repense = "Desictive le mode d'envoi de la position au serveur Traccar";

                        }
                        else 
                        {
                        printf("erreur CMD non valide !!/r/n");
                        repense="erreur CMD non valide !!";
                        } 
                    printf("Send SMS repense \"%s\" to \"%s\"\r\n", repense, num);
                    mdm.smsSend(num, repense);
                    printf("modeConfig :%d\r\n",modeConfig);
                }
            }
        }
        wait_ms(wait);
    }
    gps.powerOff();
    mdm.powerOff();
    return 0;
}

void traccar_send(char domaine[],char port[],char deviceId[],double lat,double lon,double altit,double vitesse)
{
    char socket[]="http://";
    char covertToChar[20];
 //Constuction du socket http Osmand
    strcat(socket,domaine);
    strcat(socket,":");
    strcat(socket,port);
    strcat(socket,"/?id=");
    strcat(socket,deviceId);
    strcat(socket,"&");
    sprintf(covertToChar, "%0.3f", lat);
    strcat(socket,"&");
    sprintf(covertToChar, "%0.3f", lon);
    strcat(socket,covertToChar);
    strcat(socket,"&");
    sprintf(covertToChar, "%0.3f", altit);
    strcat(socket,covertToChar);
    strcat(socket,"&");
    sprintf(covertToChar, "%0.3f", vitesse);
    strcat(socket,covertToChar);

            //Envoi des parramettes au serveur traccar avec la methode traccar
            
                int ret = http.get(socket, str, 1024);
                           if (!ret)
                        {
                          printf("\r\nPage fetched successfully - read %d characters\r\n", strlen(str));
                          printf("Result: %s\r\n", str);
                        }
                        else
                        {
                          printf("Error - ret = %d - HTTP return code = %d\r\n", ret, http.getHTTPResponseCode());
                        }
                printf("%s\r\n",socket);
}

void extrat(char str2[],char str1[])
{
  int i=0,j=0,n=0,t=0;
   n=strlen(str1);
  for(i=0;i<n;i++)
  {
      if (str1[i]==':'){
        for(j=i;j<n;j++)
            {
             str2[t]=str1[j+1];
             t++;
            }
            break;
      }
  }
  printf("%d\n",n);
}