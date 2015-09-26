#include <fstream>
#include <iostream>
#include <stdlib.h>

#include "fit_encode.hpp"
#include "fit_mesg_broadcaster.hpp"
#include "fit_file_id_mesg.hpp"
#include "fit_date_time.hpp"

using namespace std;

// Static values for different purposes
static const double DEG_TO_RAD = 0.017453292519943295769236907684886;
static const double EARTH_RADIUS_IN_METERS = 6372797.560856;
static const double WGS_TO_SEMICIRCLES = 11930464.7111; // (2^31/180)
static const char* INPUT_PATH = "/home/.../Downloads/";
static const char* OUTPUT_NAME = "/home/.../.config/antfs-cli/.../courses/New.fit";

// Calculate distance between two WGS-84 coordinates on the same altitude
// with the Haversine formula (https://en.wikipedia.org/wiki/Haversine_formula)
double dist(double fromLat, double fromLon, double toLat, double toLon)
{
   double latitudeArc  = (fromLat - toLat) * DEG_TO_RAD;
   double longitudeArc = (fromLon - toLon) * DEG_TO_RAD;
   double latitudeH = sin(latitudeArc * 0.5);
   latitudeH *= latitudeH;
   double lontitudeH = sin(longitudeArc * 0.5);
   lontitudeH *= lontitudeH;
   double tmp = cos(fromLat * DEG_TO_RAD) * cos(toLat * DEG_TO_RAD);
   return 2.0 * asin(sqrt(latitudeH + tmp * lontitudeH)) * EARTH_RADIUS_IN_METERS;
}

// Create binary encoded FIT course file
int EncodeCourseFile(char* inputName, char* courseName)
{
   int tpCount = 0;				// Counter for number of coordinates
   double distance = 0.0;		// Calculated distance between coordinates
   double speed = 5.0;			// We assume a fixed speed to create timestamps
   char inputPath[50];			// Full path to input file
   char* subStr;					// Substring with individual values from CSV
   string line;               // String containing lines from CSV
   fstream inFile, outFile;   // Input and output files
   vector<double> latitude;   // Dynamic array for WGS-84 latitude values
   vector<double> longitude;	// Dynamic array for WGS-84 longitute values
   vector<float> altitude;    // Dynamic array for WGS-84 altitude values
   fit::Encode encode;        // Instance of encoder class

   // Prepare complete path for input file
   strcpy(inputPath, INPUT_PATH);
   strcat(inputPath, inputName);

   // Open input and output files
   inFile.open(inputPath, ios::in);
   outFile.open(OUTPUT_NAME, ios::in | ios::out | ios::binary | ios::trunc);

   if (!inFile.is_open()) {
      printf("Error opening input file\n");
      return -1;
   }

   if (!outFile.is_open()) {
      printf("Error opening output file\n");
      return -1;
   }

   // Read and discard CSV header line from GPSies input file
   getline(inFile,line);

   // Get relevant parameters from input file and append them to respective vectors
   while(getline(inFile,line)) {
      subStr = strtok(&line[0],",");
      latitude.push_back(atof(subStr));
      subStr = strtok(NULL,",");
      longitude.push_back(atof(subStr));
      subStr = strtok(NULL,"\n");
      altitude.push_back(atof(subStr));
      tpCount++;
   }

   // Prepare creation timestamp for different messages
   time_t current_time_unix = time(0);
   fit::DateTime initTime(current_time_unix);

   // Prepare name of the course as UTF-16 string
   // This simplified conversion has only been tested for ASCII chars !!
   wstring courseNameW(courseName,courseName+strlen(courseName));
   
   // Prepare FIT file header
   // Values are aligned with output from Garmin Express course upload
   // without any (obvious) dependencies to a specific device
   fit::FileIdMesg fileIdMesg;
   fileIdMesg.SetLocalNum(0);    // Adjust local number in base class
   fileIdMesg.SetType(FIT_FILE_COURSE);
   fileIdMesg.SetManufacturer(FIT_MANUFACTURER_GARMIN);
   fileIdMesg.SetProduct(65534);
   fileIdMesg.SetTimeCreated(initTime.GetTimeStamp());
   fileIdMesg.SetSerialNumber(1);
   fileIdMesg.SetNumber(1);
   
   // Prepare FIT course message section
   fit::CourseMesg courseMesg;
   courseMesg.SetLocalNum(1);    // Adjust local number in base class
   courseMesg.SetName(courseNameW);
   courseMesg.SetSport(1);

   // Prepare FIT lap message section   
   fit::LapMesg lapMesg;
   lapMesg.SetLocalNum(2);    // Adjust local number in base class
   lapMesg.SetStartTime(initTime.GetTimeStamp());
   lapMesg.SetTimestamp(initTime.GetTimeStamp());
   lapMesg.SetStartPositionLat(latitude[0] * WGS_TO_SEMICIRCLES);
   lapMesg.SetStartPositionLong(longitude[0] * WGS_TO_SEMICIRCLES);
   lapMesg.SetEndPositionLat(latitude[tpCount-1] * WGS_TO_SEMICIRCLES);
   lapMesg.SetEndPositionLong(longitude[tpCount-1] * WGS_TO_SEMICIRCLES);
   lapMesg.SetTotalAscent(0);    // Don't care about the profile for now
   lapMesg.SetTotalDescent(0);   // Don't care about the profile for now
   
   // Prepare two FIT event messages surrounding the following coordinate
   // records. I couldn't find these in the official course file specs,
   // but obviously at least FR 910XT needs them for proper file decoding
   fit::EventMesg eventMesg[2];
   eventMesg[0].SetLocalNum(3);    // Adjust local number in base class
   eventMesg[0].SetTimestamp(initTime.GetTimeStamp());
   eventMesg[0].SetEvent(0);
   eventMesg[0].SetEventGroup(0);
   eventMesg[0].SetEventType(0);    // Identifies the begin / start event
   eventMesg[1].SetLocalNum(3);
   eventMesg[1].SetTimestamp(initTime.GetTimeStamp());
   eventMesg[1].SetEvent(0);
   eventMesg[1].SetEventGroup(0);
   eventMesg[1].SetEventType(9);    // Identifies the end / stop event
   
   // Prepare indidividual FIT records for every coordinate
   fit::RecordMesg* recordMesg = new fit::RecordMesg[tpCount];
   for(int i=0; i < tpCount; i++) {
      recordMesg[i].SetLocalNum(4);    // Adjust local number in base class
      recordMesg[i].SetPositionLat(latitude[i] * WGS_TO_SEMICIRCLES);
      recordMesg[i].SetPositionLong(longitude[i] * WGS_TO_SEMICIRCLES);
      recordMesg[i].SetAltitude(altitude[i]);
      recordMesg[i].SetSpeed(speed);   // Fixed speed to create timestamps
      // Calculate time and distance from second coordinate on
      if(!i) {
         recordMesg[i].SetTimestamp(initTime.GetTimeStamp());
      } else {
         distance=dist(latitude[i-1],longitude[i-1],latitude[i],longitude[i]);
         recordMesg[i].SetTimestamp(recordMesg[i-1].GetTimestamp()+(int)(distance/speed));
      }
      recordMesg[i].SetDistance(distance);  
   }

   // Put different sections of binary file together   
   encode.Open(outFile);
   encode.Write(fileIdMesg);
   encode.Write(courseMesg);
   encode.Write(lapMesg);
   encode.Write(eventMesg[0]);   // Event msg before records with definition
   for(int i=0; i < tpCount; i++) encode.Write(recordMesg[i]);
   encode.Write(eventMesg[1]);   // Event msg after records without definition

   // Calculate checksum and finally close binary FIT
   if (!encode.Close())
   {
      printf("Error closing encode.\n");
      return -1;
   }
   
   // Close input and output files
   outFile.close();
   inFile.close();
   
   // Delete allocated RecordMesg instances
   delete[] recordMesg;

   printf("Encoding finished\n");
   
   return 0;
}

int main(int argc, char* argv[])
{
   printf("FIT course file encoding\n");

   if (argc != 3)
   {
      printf("Usage: encode <downloaded filename.csv> <course name>\n");
      return -1;
   }

   return EncodeCourseFile(argv[1], argv[2]);
}
