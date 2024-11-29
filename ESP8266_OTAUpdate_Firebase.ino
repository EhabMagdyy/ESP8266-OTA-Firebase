/*********************************************************************************************************************
******************************    Author  : Ehab Magdy Abdullah                      *********************************
******************************    Linkedin: https://www.linkedin.com/in/ehabmagdyy/  *********************************
******************************    Youtube : https://www.youtube.com/@EhabMagdyy      *********************************
**********************************************************************************************************************/

// Libraries
#include <LittleFS.h>
#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include <Updater.h>
#include <addons/TokenHelper.h>
#include <addons/SDHelper.h>

// WiFi Credentials
#define WIFI_SSID "WIFI_SSID"                /* Your Wifi SSID */
#define WIFI_PASSWORD "WIFI_PASSWORD"        /* Your Wifi Password */

// Firebase Credentials
#define API_KEY "API_KEY"
#define FIREBASE_RTDB_URL "FIREBASE_RTDB_URL"
#define STORAGE_BUCKET_ID "STORAGE_BUCKET_ID"
#define USER_EMAIL "USER_EMAIL"
#define USER_PASSWORD "USER_PASSWORD"

// Firebase Data object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
bool taskCompleted = false;

// Variables for Receiving Version and Update URL and update file name
String Update_Version = "";
//String Firebase_Firmawre_Update_URL = "";
String Update_File_Name = "";

// Current firmware version
#define CURRENT_FIRMWARE_VERSION "1.0"

// setup function
void setup()
{
    Serial.begin(115200);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("Connecting to Wi-Fi");
    unsigned long ms = millis();
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(300);
    }
    Serial.println("\nConnected!\n");

    config.api_key = API_KEY;
    config.database_url = FIREBASE_RTDB_URL;
    auth.user.email = USER_EMAIL;
    auth.user.password = USER_PASSWORD;

    config.token_status_callback = tokenStatusCallback;
    Firebase.reconnectNetwork(true);
    fbdo.setBSSLBufferSize(4096, 1024);
    config.fcs.download_buffer_size = 2048;

    Firebase.begin(&config, &auth);
}

// The Firebase Storage download callback function
void fcsDownloadCallback(FCS_DownloadStatusInfo info)
{
    if (info.status == firebase_fcs_download_status_init)    {
        Serial.printf("Downloading file %s (%d) to %s\n", info.remoteFileName.c_str(), info.fileSize, info.localFileName.c_str());
    }
    else if (info.status == firebase_fcs_download_status_download)    {
        Serial.printf("Downloaded %d%s, Elapsed time %d ms\n", (int)info.progress, "%", info.elapsedTime);
    }
    else if (info.status == firebase_fcs_download_status_complete){
        Serial.println("Download completed\n");
    }
    else if (info.status == firebase_fcs_download_status_error){
        Serial.printf("Download failed, %s\n", info.errorMsg.c_str());
    }
}

// Check if there are updates
bool CheckForUpdate()
{
    if (Firebase.ready())
    {
        // Read Available update Version Number
        if(Firebase.RTDB.getString(&fbdo, "/update/version"))
        {
            Update_Version = fbdo.stringData();
            Serial.println("Update Version: " + Update_Version);

            char update_version = Update_Version.compareTo(CURRENT_FIRMWARE_VERSION);
            
            // if Version Higher than current version
            if(update_version > 0)
            {
                // Read update file name
                if(Firebase.RTDB.getString(&fbdo, "/update/fileName"))
                {
                    Update_File_Name = fbdo.stringData();
                    Serial.println("Update File Name: " + Update_File_Name);
                    return true;
/*                    // Read update URL
                    if(Firebase.RTDB.getString(&fbdo, "/update/url"))
                    {
                        Firebase_Firmawre_Update_URL = fbdo.stringData();
                        Serial.println("Update URL: " + Firebase_Firmawre_Update_URL);
                        //return true;
                    }
                    else{ Serial.println(fbdo.errorReason().c_str()); }*/
                }
                else{ Serial.println(fbdo.errorReason().c_str()); }
            }
            else if(0 == update_version){ Serial.println("Application is Up To Date"); }
            else{ Serial.println("Firebase version is old!"); }

        }
        else{ Serial.println(fbdo.errorReason().c_str()); }
    }

    return false;
}

// Download update file and update the microcontroller firmware
void downloadAndUpdateFirmware()
{
    Serial.println("\nDownload file...\n");

    // The file systems for flash and SD/SDMMC can be changed in FirebaseFS.h.
    if (!Firebase.Storage.download(&fbdo, STORAGE_BUCKET_ID /* Firebase Storage bucket id */, Update_File_Name /* path of file stored in the bucket */, "/update.bin" /* path to local file */, mem_storage_type_flash, fcsDownloadCallback))
        Serial.println(fbdo.errorReason());

    File file = LittleFS.open("/update.bin", "r");
    if (!file)
    {
        Serial.println("Failed to open file for update");
        return;
    }

    size_t fileSize = file.size();
    Serial.printf("Starting OTA update from file: %s, size: %d bytes\n", "/update.bin", fileSize);

    if (Update.begin(fileSize)){
        size_t written = Update.writeStream(file);
        if (written == fileSize){
            Serial.println("Update successfully written.");
        }
        else{
            Serial.printf("Update failed. Written only %d/%d bytes\n", written, fileSize);
        }

        if (Update.end()){
            if (Update.isFinished()){
                Serial.println("Update successfully completed. Rebooting...");
                ESP.restart();
            }
            else{
                Serial.println("Update not finished. Something went wrong.");
            }
        }
        else{
            Serial.println("Update failed");
        }
    }
    else{
        Serial.println("Not enough space to begin OTA");
    }

    file.close();
}

// loop function
void loop()
{
    delay(5000);

    // Printing the current running version on microcontroller
    Serial.println("Current Version: " + String(CURRENT_FIRMWARE_VERSION));
    delay(1000);
    
    // if there is a new update, download and update the firmware
    if(true == CheckForUpdate()){ downloadAndUpdateFirmware() ; }
}